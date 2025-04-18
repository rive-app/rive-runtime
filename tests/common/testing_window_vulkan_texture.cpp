/*
 * Copyright 2024 Rive
 */

#include "testing_window.hpp"

#ifndef RIVE_VULKAN

TestingWindow* TestingWindow::MakeVulkanTexture(bool coreFeaturesOnly,
                                                bool clockwiseFill,
                                                const char* gpuNameFilter)
{
    return nullptr;
}

#else

#include "rive_vk_bootstrap/rive_vk_bootstrap.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "rive/renderer/vulkan/render_context_vulkan_impl.hpp"
#include "rive/renderer/vulkan/render_target_vulkan.hpp"

namespace rive::gpu
{
class TestingWindowVulkanTexture : public TestingWindow
{
public:
    TestingWindowVulkanTexture(bool coreFeaturesOnly,
                               bool clockwiseFill,
                               const char* gpuNameFilter) :
        m_coreFeaturesOnly(coreFeaturesOnly), m_clockwiseFill(clockwiseFill)
    {
        rive_vkb::load_vulkan();

        m_instance =
            VKB_CHECK(vkb::InstanceBuilder()
                          .set_app_name("rive_tools")
                          .set_engine_name("Rive Renderer")
                          .set_headless(true)
#ifdef DEBUG
                          .enable_validation_layers()
                          .set_debug_callback(rive_vkb::default_debug_callback)
#endif
                          .require_api_version(1, m_coreFeaturesOnly ? 0 : 3, 0)
                          .set_minimum_instance_version(1, 0, 0)
                          .build());

        VulkanFeatures vulkanFeatures;
        std::tie(m_device, vulkanFeatures) = rive_vkb::select_device(
            m_instance,
            m_coreFeaturesOnly ? rive_vkb::FeatureSet::coreOnly
                               : rive_vkb::FeatureSet::allAvailable,
            gpuNameFilter);
        m_renderContext = RenderContextVulkanImpl::MakeContext(
            m_instance,
            m_device.physical_device,
            m_device,
            vulkanFeatures,
            m_instance.fp_vkGetInstanceProcAddr);
    }

    ~TestingWindowVulkanTexture()
    {
        // Destroy the swapchain first because it synchronizes for in-flight
        // command buffers.
        m_swapchain = nullptr;

        m_renderContext.reset();
        m_renderTarget.reset();

        vkb::destroy_device(m_device);
        vkb::destroy_instance(m_instance);
    }

    rive::Factory* factory() override { return m_renderContext.get(); }

    rive::gpu::RenderContext* renderContext() const override
    {
        return m_renderContext.get();
    }

    std::unique_ptr<rive::Renderer> beginFrame(
        const FrameOptions& options) override
    {
        if (m_swapchain == nullptr || m_swapchain->width() != m_width ||
            m_swapchain->height() != m_height)
        {
            VkFormat swapchainFormat = m_coreFeaturesOnly
                                           ? VK_FORMAT_R8G8B8A8_UNORM
                                           : VK_FORMAT_B8G8R8A8_UNORM;
            VkImageUsageFlags additionalUsageFlags =
                m_coreFeaturesOnly ? VK_IMAGE_USAGE_TRANSFER_DST_BIT
                                   : VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
            uint64_t currentFrameNumber =
                m_swapchain != nullptr ? m_swapchain->currentFrameNumber() : 0;
            m_swapchain =
                std::make_unique<rive_vkb::Swapchain>(m_device,
                                                      ref_rcp(vk()),
                                                      m_width,
                                                      m_height,
                                                      swapchainFormat,
                                                      additionalUsageFlags,
                                                      currentFrameNumber);
            m_renderTarget =
                impl()->makeRenderTarget(m_width,
                                         m_height,
                                         m_swapchain->imageFormat(),
                                         m_swapchain->imageUsageFlags());
        }

        rive::gpu::RenderContext::FrameDescriptor frameDescriptor = {
            .renderTargetWidth = m_width,
            .renderTargetHeight = m_height,
            .loadAction = options.doClear
                              ? rive::gpu::LoadAction::clear
                              : rive::gpu::LoadAction::preserveRenderTarget,
            .clearColor = options.clearColor,
            .wireframe = options.wireframe,
            .clockwiseFillOverride =
                m_clockwiseFill || options.clockwiseFillOverride,
        };
        m_renderContext->beginFrame(frameDescriptor);
        return std::make_unique<RiveRenderer>(m_renderContext.get());
    }

    void flushPLSContext() final
    {
        const rive_vkb::SwapchainImage* swapchainImage =
            m_swapchain->currentImage();
        if (swapchainImage == nullptr)
        {
            swapchainImage = m_swapchain->acquireNextImage();
            m_renderTarget->setTargetImageView(swapchainImage->imageView,
                                               swapchainImage->image,
                                               swapchainImage->imageLastAccess);
        }

        m_renderContext->flush({
            .renderTarget = m_renderTarget.get(),
            .externalCommandBuffer = swapchainImage->commandBuffer,
            .currentFrameNumber = swapchainImage->currentFrameNumber,
            .safeFrameNumber = swapchainImage->safeFrameNumber,
        });
    }

    void endFrame(std::vector<uint8_t>* pixelData) override
    {
        flushPLSContext();
        m_swapchain->submit(m_renderTarget->targetLastAccess(), pixelData);
    }

private:
    RenderContextVulkanImpl* impl() const
    {
        return m_renderContext->static_impl_cast<RenderContextVulkanImpl>();
    }

    VulkanContext* vk() const { return impl()->vulkanContext(); }

    const bool m_coreFeaturesOnly;
    const bool m_clockwiseFill;
    vkb::Instance m_instance;
    vkb::Device m_device;
    std::unique_ptr<RenderContext> m_renderContext;
    std::unique_ptr<rive_vkb::Swapchain> m_swapchain;
    rcp<RenderTargetVulkanImpl> m_renderTarget;
};
}; // namespace rive::gpu

TestingWindow* TestingWindow::MakeVulkanTexture(bool coreFeaturesOnly,
                                                bool clockwiseFill,
                                                const char* gpuNameFilter)
{
    return new rive::gpu::TestingWindowVulkanTexture(coreFeaturesOnly,
                                                     clockwiseFill,
                                                     gpuNameFilter);
}

#endif
