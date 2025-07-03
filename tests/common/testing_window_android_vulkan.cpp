/*
 * Copyright 2024 Rive
 */

#include "testing_window.hpp"

#if !defined(RIVE_ANDROID) || !defined(RIVE_VULKAN)

TestingWindow* TestingWindow::MakeAndroidVulkan(const BackendParams&,
                                                void* platformWindow)
{
    return nullptr;
}

#else

#include "common/offscreen_render_target.hpp"
#include "rive_vk_bootstrap/rive_vk_bootstrap.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "rive/renderer/vulkan/render_context_vulkan_impl.hpp"
#include "rive/renderer/vulkan/render_target_vulkan.hpp"
#include <vulkan/vulkan_android.h>
#include <vk_mem_alloc.h>
#include <android/native_app_glue/android_native_app_glue.h>

using namespace rive;
using namespace rive::gpu;

class TestingWindowAndroidVulkan : public TestingWindow
{
public:
    TestingWindowAndroidVulkan(const BackendParams& backendParams,
                               ANativeWindow* window) :
        m_backendParams(backendParams)
    {
        m_androidWindowWidth = m_width = ANativeWindow_getWidth(window);
        m_androidWindowHeight = m_height = ANativeWindow_getHeight(window);
        rive_vkb::load_vulkan();

        vkb::InstanceBuilder instanceBuilder;
        instanceBuilder.set_app_name("path_fiddle")
            .set_engine_name("Rive Renderer")
            .enable_extension(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME)
            .require_api_version(1, m_backendParams.core ? 0 : 3, 0)
            .set_minimum_instance_version(1, 0, 0);
#ifdef DEBUG
        instanceBuilder.enable_validation_layers(
            !backendParams.disableValidationLayers);
        if (!backendParams.disableDebugCallbacks)
        {
            instanceBuilder.set_debug_callback(
                rive_vkb::default_debug_callback);
        }
#endif
        m_instance = VKB_CHECK(instanceBuilder.build());
        m_instanceDispatchTable = m_instance.make_table();

        VkAndroidSurfaceCreateInfoKHR androidSurfaceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
            .window = window,
        };
        auto pfnvkCreateAndroidSurfaceKHR =
            reinterpret_cast<PFN_vkCreateAndroidSurfaceKHR>(
                m_instance.fp_vkGetInstanceProcAddr(
                    m_instance,
                    "vkCreateAndroidSurfaceKHR"));
        assert(pfnvkCreateAndroidSurfaceKHR);
        VK_CHECK(pfnvkCreateAndroidSurfaceKHR(m_instance,
                                              &androidSurfaceCreateInfo,
                                              nullptr,
                                              &m_windowSurface));

        VulkanFeatures vulkanFeatures;
        std::tie(m_device, vulkanFeatures) = rive_vkb::select_device(
            vkb::PhysicalDeviceSelector(m_instance)
                .set_surface(m_windowSurface),
            m_backendParams.core ? rive_vkb::FeatureSet::coreOnly
                                 : rive_vkb::FeatureSet::allAvailable);
        m_renderContext = RenderContextVulkanImpl::MakeContext(
            m_instance,
            m_device.physical_device,
            m_device,
            vulkanFeatures,
            m_instance.fp_vkGetInstanceProcAddr);

        VkSurfaceCapabilitiesKHR windowCapabilities;
        VK_CHECK(m_instanceDispatchTable
                     .fp_vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
                         m_device.physical_device,
                         m_windowSurface,
                         &windowCapabilities));
        auto swapchainBuilder =
            vkb::SwapchainBuilder(m_device, m_windowSurface)
                .set_desired_format({
                    .format = m_backendParams.srgb ? VK_FORMAT_R8G8B8A8_SRGB
                                                   : VK_FORMAT_R8G8B8A8_UNORM,
                    .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
                })
                .add_fallback_format({
                    .format = VK_FORMAT_R8G8B8A8_UNORM,
                    .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
                })
                .set_desired_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR)
                .add_fallback_present_mode(VK_PRESENT_MODE_FIFO_KHR)
                .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
                .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT);
        if (windowCapabilities.supportedUsageFlags &
            VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT)
        {
            swapchainBuilder.add_image_usage_flags(
                VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);
        }
        m_swapchain = std::make_unique<rive_vkb::Swapchain>(
            m_device,
            ref_rcp(vk()),
            m_androidWindowWidth,
            m_androidWindowHeight,
            VKB_CHECK(swapchainBuilder.build()));

        m_renderTarget =
            impl()->makeRenderTarget(m_width,
                                     m_height,
                                     m_swapchain->imageFormat(),
                                     m_swapchain->imageUsageFlags());
    }

    ~TestingWindowAndroidVulkan()
    {
        // Destroy the swapchain first because it synchronizes for in-flight
        // command buffers.
        m_swapchain = nullptr;

        m_renderContext.reset();
        m_renderTarget.reset();
        m_overflowTexture.reset();

        if (m_windowSurface != VK_NULL_HANDLE)
        {
            m_instanceDispatchTable.destroySurfaceKHR(m_windowSurface, nullptr);
        }

        vkb::destroy_device(m_device);
        vkb::destroy_instance(m_instance);
    }

    Factory* factory() override { return m_renderContext.get(); }

    rive::gpu::RenderContext* renderContext() const override
    {
        return m_renderContext.get();
    }

    rive::gpu::RenderTarget* renderTarget() const override
    {
        return m_renderTarget.get();
    }

    void resize(int width, int height) override
    {
        TestingWindow::resize(width, height);
        m_renderTarget =
            impl()->makeRenderTarget(m_width,
                                     m_height,
                                     m_swapchain->imageFormat(),
                                     m_swapchain->imageUsageFlags());
        if (m_width > m_androidWindowWidth || m_height > m_androidWindowHeight)
        {
            VkImageUsageFlags overflowTextureUsageFlags =
                m_swapchain->imageUsageFlags();
            // Some ARM Mali GPUs experience a device loss when rendering to the
            // overflow texture as an input attachment. The current assumption
            // is that it has to do with some combination of input attachment
            // usage and pixel readbacks. For now, just don't enable the input
            // attachment flag on the overflow texture.
            overflowTextureUsageFlags &= ~VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
            m_overflowTexture = vk()->makeTexture2D({
                .format = m_swapchain->imageFormat(),
                .extent = {static_cast<uint32_t>(m_width),
                           static_cast<uint32_t>(m_height)},
                .usage = m_swapchain->imageUsageFlags(),
            });
        }
        else
        {
            m_overflowTexture.reset();
        }
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
        m_renderContext->beginFrame(RenderContext::FrameDescriptor{
            .renderTargetWidth = m_width,
            .renderTargetHeight = m_height,
            .loadAction = options.doClear
                              ? gpu::LoadAction::clear
                              : gpu::LoadAction::preserveRenderTarget,
            .clearColor = options.clearColor,
            .msaaSampleCount = m_backendParams.msaa ? 4 : 0,
            .disableRasterOrdering = options.disableRasterOrdering,
            .wireframe = options.wireframe,
            .clockwiseFillOverride =
                m_backendParams.clockwise || options.clockwiseFillOverride,
            .synthesizeCompilationFailures =
                options.synthesizeCompilationFailures,
        });

        return std::make_unique<RiveRenderer>(m_renderContext.get());
    }

    void flushPLSContext(RenderTarget* offscreenRenderTarget) override
    {
        if (m_swapchainImage == nullptr)
        {
            m_swapchainImage = m_swapchain->acquireNextImage();
            if (m_overflowTexture != nullptr)
            {
                m_renderTarget->setTargetImageView(
                    m_overflowTexture->vkImageView(),
                    m_overflowTexture->vkImage(),
                    m_overflowTexture->lastAccess());
            }
            else
            {
                m_renderTarget->setTargetImageView(
                    m_swapchainImage->imageView,
                    m_swapchainImage->image,
                    m_swapchainImage->imageLastAccess);
            }
        }
        m_renderContext->flush({
            .renderTarget = offscreenRenderTarget != nullptr
                                ? offscreenRenderTarget
                                : m_renderTarget.get(),
            .externalCommandBuffer = m_swapchainImage->commandBuffer,
            .currentFrameNumber = m_swapchainImage->currentFrameNumber,
            .safeFrameNumber = m_swapchainImage->safeFrameNumber,
        });
    }

    void endFrame(std::vector<uint8_t>* pixelData) override
    {
        flushPLSContext(nullptr);
        if (m_overflowTexture == nullptr)
        {
            // We rendered directly to the window. Submit and read back
            // normally.
            m_swapchain->submit(m_renderTarget->targetLastAccess(),
                                pixelData,
                                IAABB::MakeWH(m_width, m_height));
        }
        else
        {
            // Blit the overflow texture onto the screen in order to give some
            // visual feedback.
            vkutil::ImageAccess swapchainLastAccess =
                vk()->simpleImageMemoryBarrier(
                    m_swapchainImage->commandBuffer,
                    m_swapchainImage->imageLastAccess,
                    {
                        .pipelineStages = VK_PIPELINE_STAGE_TRANSFER_BIT,
                        .accessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                        .layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    },
                    m_swapchainImage->image);
            vk()->blitSubRect(
                m_swapchainImage->commandBuffer,
                m_renderTarget->accessTargetImage(
                    m_swapchainImage->commandBuffer,
                    {
                        .pipelineStages = VK_PIPELINE_STAGE_TRANSFER_BIT,
                        .accessMask = VK_ACCESS_TRANSFER_READ_BIT,
                        .layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    }),
                m_swapchainImage->image,
                IAABB{0,
                      0,
                      std::min<int>(m_width, m_androidWindowWidth),
                      std::min<int>(m_height, m_androidWindowHeight)});
            m_overflowTexture->lastAccess() =
                m_renderTarget->targetLastAccess();
            // Readback from the overflow texture when we submit.
            m_swapchain->submit(swapchainLastAccess,
                                pixelData,
                                IAABB::MakeWH(m_width, m_height),
                                m_overflowTexture.get());
        }
        m_swapchainImage = nullptr;
    }

private:
    RenderContextVulkanImpl* impl() const
    {
        return m_renderContext->static_impl_cast<RenderContextVulkanImpl>();
    }

    VulkanContext* vk() const { return impl()->vulkanContext(); }

    const BackendParams m_backendParams;
    uint32_t m_androidWindowWidth;
    uint32_t m_androidWindowHeight;
    vkb::Instance m_instance;
    vkb::InstanceDispatchTable m_instanceDispatchTable;
    vkb::Device m_device;
    VkSurfaceKHR m_windowSurface = VK_NULL_HANDLE;
    std::unique_ptr<rive_vkb::Swapchain> m_swapchain;
    std::unique_ptr<RenderContext> m_renderContext;
    rcp<RenderTargetVulkanImpl> m_renderTarget;
    rcp<vkutil::Texture2D> m_overflowTexture; // Used when the desired render
                                              // size doesn't fit in the window.
    const rive_vkb::SwapchainImage* m_swapchainImage = nullptr;
};

TestingWindow* TestingWindow::MakeAndroidVulkan(
    const BackendParams& backendParams,
    void* platformWindow)
{
    return new TestingWindowAndroidVulkan(
        backendParams,
        reinterpret_cast<ANativeWindow*>(platformWindow));
}

#endif
