/*
 * Copyright 2024 Rive
 */

#include "testing_window.hpp"

#if !defined(RIVE_ANDROID) || !defined(RIVE_VULKAN)

TestingWindow* TestingWindow::MakeAndroidVulkan(void* platformWindow,
                                                bool coreFeaturesOnly,
                                                bool clockwiseFill)
{
    return nullptr;
}

#else

#include "rive_vk_bootstrap/rive_vk_bootstrap.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "rive/renderer/vulkan/render_context_vulkan_impl.hpp"
#include <vulkan/vulkan_android.h>
#include <vk_mem_alloc.h>
#include <android/native_app_glue/android_native_app_glue.h>

using namespace rive;
using namespace rive::gpu;

class TestingWindowAndroidVulkan : public TestingWindow
{
public:
    TestingWindowAndroidVulkan(ANativeWindow* window,
                               bool coreFeaturesOnly,
                               bool clockwiseFill) :
        m_clockwiseFill(clockwiseFill)
    {
        m_width = ANativeWindow_getWidth(window);
        m_height = ANativeWindow_getHeight(window);
        rive_vkb::load_vulkan();

        m_instance = VKB_CHECK(
            vkb::InstanceBuilder()
                .set_app_name("path_fiddle")
                .set_engine_name("Rive Renderer")
#ifdef DEBUG
                .set_debug_callback(rive_vkb::default_debug_callback)
                .enable_validation_layers(true)
#endif
                .enable_extension(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME)
                .require_api_version(1, coreFeaturesOnly ? 0 : 3, 0)
                .set_minimum_instance_version(1, 0, 0)
                .build());
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
            coreFeaturesOnly ? rive_vkb::FeatureSet::coreOnly
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
        vkb::SwapchainBuilder swapchainBuilder(m_device, m_windowSurface);
        swapchainBuilder
            .set_desired_format({
                .format = VK_FORMAT_B8G8R8A8_UNORM,
                .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
            })
            .add_fallback_format({
                .format = VK_FORMAT_R8G8B8A8_UNORM,
                .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
            })
            .set_desired_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR)
            .add_fallback_present_mode(VK_PRESENT_MODE_FIFO_KHR);
        if (windowCapabilities.supportedUsageFlags &
            VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT)
        {
            printf("Android window supports "
                   "VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT\n");
            swapchainBuilder.add_image_usage_flags(
                VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);
        }
        else
        {
            printf("Android window does NOT support "
                   "VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT. "
                   "Performance will suffer.\n");
            swapchainBuilder
                .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
                .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT);
        }
        m_swapchain = std::make_unique<rive_vkb::Swapchain>(
            m_device,
            ref_rcp(vk()),
            m_width,
            m_height,
            VKB_CHECK(swapchainBuilder.build()));

        m_renderTarget = impl()->makeRenderTarget(m_width,
                                                  m_height,
                                                  m_swapchain->imageFormat());
    }

    ~TestingWindowAndroidVulkan()
    {
        // Destroy the swapchain first because it synchronizes for in-flight
        // command buffers.
        m_swapchain = nullptr;

        m_renderContext.reset();
        m_renderTarget.reset();

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
        fprintf(stderr, "TestingWindowAndroidVulkan::resize not supported.");
        abort();
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
            .wireframe = options.wireframe,
            .clockwiseFillOverride =
                m_clockwiseFill || options.clockwiseFillOverride,
        });

        return std::make_unique<RiveRenderer>(m_renderContext.get());
    }

    void flushPLSContext() override
    {
        fprintf(stderr,
                "TestingWindowAndroidVulkan::flushPLSContext not supported.");
        abort();
    }

    void endFrame(std::vector<uint8_t>* pixelData) override
    {
        const rive_vkb::SwapchainImage* swapchainImage =
            m_swapchain->acquireNextImage();
        m_renderTarget->setTargetTextureView(swapchainImage->imageView, {});
        m_renderContext->flush({
            .renderTarget = m_renderTarget.get(),
            .externalCommandBuffer = swapchainImage->commandBuffer,
        });
        m_renderTarget->setTargetLastAccess(
            m_swapchain->submit(m_renderTarget->targetLastAccess(), pixelData));
    }

private:
    RenderContextVulkanImpl* impl() const
    {
        return m_renderContext->static_impl_cast<RenderContextVulkanImpl>();
    }

    VulkanContext* vk() const { return impl()->vulkanContext(); }

    bool m_clockwiseFill;
    vkb::Instance m_instance;
    vkb::InstanceDispatchTable m_instanceDispatchTable;
    vkb::Device m_device;
    VkSurfaceKHR m_windowSurface = VK_NULL_HANDLE;
    std::unique_ptr<rive_vkb::Swapchain> m_swapchain;
    std::unique_ptr<RenderContext> m_renderContext;
    rcp<RenderTargetVulkan> m_renderTarget;
};

TestingWindow* TestingWindow::MakeAndroidVulkan(void* platformWindow,
                                                bool coreFeaturesOnly,
                                                bool clockwiseFill)
{
    return new TestingWindowAndroidVulkan(
        reinterpret_cast<ANativeWindow*>(platformWindow),
        coreFeaturesOnly,
        clockwiseFill);
}

#endif
