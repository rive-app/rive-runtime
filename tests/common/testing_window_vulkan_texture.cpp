/*
 * Copyright 2024 Rive
 */

#include "testing_window.hpp"

#ifndef RIVE_VULKAN

TestingWindow* TestingWindow::MakeVulkanTexture(const BackendParams&)
{
    return nullptr;
}

#else

#include "common/offscreen_render_target.hpp"
#include "rive_vk_bootstrap/vulkan_device.hpp"
#include "rive_vk_bootstrap/vulkan_instance.hpp"
#include "rive_vk_bootstrap/vulkan_headless_frame_synchronizer.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "rive/renderer/vulkan/render_context_vulkan_impl.hpp"
#include "rive/renderer/vulkan/render_target_vulkan.hpp"

namespace rive::gpu
{
class TestingWindowVulkanTexture : public TestingWindow
{
public:
    TestingWindowVulkanTexture(const BackendParams& backendParams) :
        m_backendParams(backendParams)
    {
        using namespace rive_vkb;

        m_instance = std::make_unique<VulkanInstance>(VulkanInstance::Options{
            .appName = "Rive Unit Tests",
            .idealAPIVersion =
                m_backendParams.core ? VK_API_VERSION_1_0 : VK_API_VERSION_1_3,
#ifndef NDEBUG
            .desiredValidationType =
                m_backendParams.disableValidationLayers
                    ? VulkanValidationType::none
                    : (m_backendParams.wantVulkanSynchronizationValidation
                           ? VulkanValidationType::synchronization
                           : VulkanValidationType::core),
            .wantDebugCallbacks = !m_backendParams.disableDebugCallbacks,
#endif
        });

        m_device = std::make_unique<VulkanDevice>(
            *m_instance,
            VulkanDevice::Options{
                .coreFeaturesOnly = m_backendParams.core,
                .gpuNameFilter = m_backendParams.gpuNameFilter.c_str(),
                .headless = true,
            });

        m_renderContext = RenderContextVulkanImpl::MakeContext(
            m_instance->vkInstance(),
            m_device->vkPhysicalDevice(),
            m_device->vkDevice(),
            m_device->vulkanFeatures(),
            m_instance->getVkGetInstanceProcAddrPtr(),
            {
                .forceAtomicMode = backendParams.atomic,
                .shaderCompilationMode =
                    ShaderCompilationMode::alwaysSynchronous,
            });
    }

    ~TestingWindowVulkanTexture()
    {
        // Destroy the offscreen frame syncrhonizer  first because it
        // synchronizes for in-flight command buffers.
        m_frameSynchronizer = nullptr;

        m_renderContext.reset();
        m_renderTarget.reset();
    }

    rive::Factory* factory() override { return m_renderContext.get(); }

    rive::gpu::RenderContext* renderContext() const override
    {
        return m_renderContext.get();
    }

    rcp<rive_tests::OffscreenRenderTarget> makeOffscreenRenderTarget(
        uint32_t width,
        uint32_t height,
        bool riveRenderable) const override
    {
        return rive_tests::OffscreenRenderTarget::MakeVulkan(vk(),
                                                             width,
                                                             height,
                                                             riveRenderable);
    }

    std::unique_ptr<rive::Renderer> beginFrame(
        const FrameOptions& options) override
    {
        if (m_frameSynchronizer == nullptr ||
            m_frameSynchronizer->width() != m_width ||
            m_frameSynchronizer->height() != m_height)
        {
            VkFormat imageFormat =
                m_backendParams.srgb   ? VK_FORMAT_R8G8B8A8_SRGB
                : m_backendParams.core ? VK_FORMAT_R8G8B8A8_UNORM
                                       : VK_FORMAT_B8G8R8A8_UNORM;
            // Don't use VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT or
            // VK_IMAGE_USAGE_STORAGE_BIT so we can test our (more complex)
            // codepaths that make us work without it.
            // The GMs that use offscreen "riveRenderable" render targets ensure
            // we still cover the simpler direct rendering cases.
            VkImageUsageFlags usageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                           VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                           VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            uint64_t currentFrameNumber =
                m_frameSynchronizer != nullptr
                    ? m_frameSynchronizer->currentFrameNumber()
                    : 0;

            m_frameSynchronizer =
                std::make_unique<rive_vkb::VulkanHeadlessFrameSynchronizer>(
                    *m_instance,
                    *m_device,
                    ref_rcp(vk()),
                    rive_vkb::VulkanHeadlessFrameSynchronizer::Options{
                        .width = m_width,
                        .height = m_height,
                        .imageFormat = imageFormat,
                        .imageUsageFlags = usageFlags,
                        .initialFrameNumber = currentFrameNumber,
                    });
            m_renderTarget = impl()->makeRenderTarget(
                m_width,
                m_height,
                m_frameSynchronizer->imageFormat(),
                m_frameSynchronizer->imageUsageFlags());
        }

        rive::gpu::RenderContext::FrameDescriptor frameDescriptor = {
            .renderTargetWidth = m_width,
            .renderTargetHeight = m_height,
            .loadAction = options.doClear
                              ? rive::gpu::LoadAction::clear
                              : rive::gpu::LoadAction::preserveRenderTarget,
            .clearColor = options.clearColor,
            .msaaSampleCount = m_backendParams.msaa ? 4u : 0u,
            .disableRasterOrdering = options.disableRasterOrdering,
            .wireframe = options.wireframe,
            .fillsDisabled = options.fillsDisabled,
            .strokesDisabled = options.strokesDisabled,
            .clockwiseFillOverride =
                m_backendParams.clockwise || options.clockwiseFillOverride,
            .synthesizedFailureType = options.synthesizedFailureType,
        };
        m_renderContext->beginFrame(frameDescriptor);
        return std::make_unique<RiveRenderer>(m_renderContext.get());
    }

    void flushPLSContext(RenderTarget* offscreenRenderTarget) final
    {
        if (!m_frameSynchronizer->isFrameStarted())
        {
            m_frameSynchronizer->beginFrame();

            m_renderTarget->setTargetImageView(
                m_frameSynchronizer->vkImageView(),
                m_frameSynchronizer->vkImage(),
                m_frameSynchronizer->lastAccess());
        }

        m_renderContext->flush({
            .renderTarget = offscreenRenderTarget != nullptr
                                ? offscreenRenderTarget
                                : m_renderTarget.get(),
            .externalCommandBuffer =
                m_frameSynchronizer->currentCommandBuffer(),
            .currentFrameNumber = m_frameSynchronizer->currentFrameNumber(),
            .safeFrameNumber = m_frameSynchronizer->safeFrameNumber(),
        });
    }

    void endFrame(std::vector<uint8_t>* pixelData) override
    {
        flushPLSContext(nullptr);
        auto lastAccess = m_renderTarget->targetLastAccess();
        if (pixelData != nullptr)
        {
            m_frameSynchronizer->queueImageCopy(&lastAccess);
        }

        m_frameSynchronizer->endFrame(lastAccess);

        if (pixelData != nullptr)
        {
            m_frameSynchronizer->getPixelsFromLastImageCopy(pixelData);
        }
    }

private:
    RenderContextVulkanImpl* impl() const
    {
        return m_renderContext->static_impl_cast<RenderContextVulkanImpl>();
    }

    VulkanContext* vk() const { return impl()->vulkanContext(); }

    const BackendParams m_backendParams;
    std::unique_ptr<rive_vkb::VulkanInstance> m_instance;
    std::unique_ptr<rive_vkb::VulkanDevice> m_device;
    std::unique_ptr<RenderContext> m_renderContext;
    std::unique_ptr<rive_vkb::VulkanHeadlessFrameSynchronizer>
        m_frameSynchronizer;
    rcp<RenderTargetVulkanImpl> m_renderTarget;
};
}; // namespace rive::gpu

TestingWindow* TestingWindow::MakeVulkanTexture(
    const BackendParams& backendParams)
{
    return new rive::gpu::TestingWindowVulkanTexture(backendParams);
}

#endif
