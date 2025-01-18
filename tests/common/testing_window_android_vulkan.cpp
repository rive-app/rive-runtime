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
#include "rive/renderer/vulkan/vkutil_resource_pool.hpp"
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
                .build());
        m_instanceFns = m_instance.make_table();

        VkAndroidSurfaceCreateInfoKHR androidSurfaceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
            .window = window,
        };
        auto fp_vkCreateAndroidSurfaceKHR =
            reinterpret_cast<PFN_vkCreateAndroidSurfaceKHR>(
                m_instance.fp_vkGetInstanceProcAddr(
                    m_instance,
                    "vkCreateAndroidSurfaceKHR"));
        assert(fp_vkCreateAndroidSurfaceKHR);
        VK_CHECK(fp_vkCreateAndroidSurfaceKHR(m_instance,
                                              &androidSurfaceCreateInfo,
                                              nullptr,
                                              &m_windowSurface));

        VulkanFeatures vulkanFeatures;
        std::tie(m_physicalDevice, vulkanFeatures) =
            rive_vkb::select_physical_device(
                vkb::PhysicalDeviceSelector(m_instance)
                    .set_surface(m_windowSurface),
                coreFeaturesOnly ? rive_vkb::FeatureSet::coreOnly
                                 : rive_vkb::FeatureSet::allAvailable);
        m_device = VKB_CHECK(vkb::DeviceBuilder(m_physicalDevice).build());
        m_vkbTable = m_device.make_table();
        m_queue = VKB_CHECK(m_device.get_queue(vkb::QueueType::graphics));
        m_renderContext = RenderContextVulkanImpl::MakeContext(
            m_instance,
            m_physicalDevice,
            m_device,
            vulkanFeatures,
            m_instance.fp_vkGetInstanceProcAddr,
            m_instance.fp_vkGetDeviceProcAddr);

        VkSurfaceCapabilitiesKHR windowCapabilities;
        VK_CHECK(m_instanceFns.fp_vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            m_physicalDevice,
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
        m_swapchain = VKB_CHECK(swapchainBuilder.build());

        m_swapchainImages = *m_swapchain.get_images();

        m_swapchainImageViews.clear();
        m_swapchainImageViews.reserve(m_swapchainImages.size());
        for (VkImage image : m_swapchainImages)
        {
            m_swapchainImageViews.push_back(vk()->makeExternalTextureView(
                m_swapchain.image_usage_flags,
                {
                    .image = image,
                    .viewType = VK_IMAGE_VIEW_TYPE_2D,
                    .format = m_swapchain.image_format,
                    .subresourceRange =
                        {
                            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                            .levelCount = 1,
                            .layerCount = 1,
                        },
                }));
        }

        m_renderTarget = impl()->makeRenderTarget(m_width,
                                                  m_height,
                                                  m_swapchain.image_format);
        m_commandBufferPool =
            make_rcp<vkutil::ResourcePool<vkutil::CommandBuffer>>(
                ref_rcp(vk()),
                *m_device.get_queue_index(vkb::QueueType::graphics));
        m_semaphorePool =
            make_rcp<vkutil::ResourcePool<vkutil::Semaphore>>(ref_rcp(vk()));
        m_fencePool =
            make_rcp<vkutil::ResourcePool<vkutil::Fence>>(ref_rcp(vk()));
    }

    ~TestingWindowAndroidVulkan()
    {
        // Destroy these before destroying the VkDevice.
        m_renderContext.reset();
        m_renderTarget.reset();
        m_swapchainImageViews.clear();
        m_fencePool.reset();

        VK_CHECK(m_vkbTable.queueWaitIdle(m_queue));

        m_commandBufferPool = nullptr;
        m_semaphorePool = nullptr;
        m_fencePool = nullptr;

        if (m_swapchain != VK_NULL_HANDLE)
        {
            vkb::destroy_swapchain(m_swapchain);
        }

        if (m_windowSurface != VK_NULL_HANDLE)
        {
            m_instanceFns.destroySurfaceKHR(m_windowSurface, nullptr);
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
        auto swapchainSemaphore = m_semaphorePool->make();
        m_vkbTable.acquireNextImageKHR(m_swapchain,
                                       UINT64_MAX,
                                       *swapchainSemaphore,
                                       VK_NULL_HANDLE,
                                       &m_swapchainImageIndex);

        auto commandBuffer = m_commandBufferPool->make();

        VkCommandBufferBeginInfo commandBufferBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        };
        m_vkbTable.beginCommandBuffer(*commandBuffer, &commandBufferBeginInfo);

        m_renderTarget->setTargetTextureView(
            m_swapchainImageViews[m_swapchainImageIndex],
            {});

        rcp<vkutil::Fence> frameFence = m_fencePool->make();

        m_renderContext->flush({
            .renderTarget = m_renderTarget.get(),
            .externalCommandBuffer = *commandBuffer,
            .frameCompletionFence = frameFence.get(),
        });

        m_renderTarget->setTargetLastAccess(vk()->simpleImageMemoryBarrier(
            *commandBuffer,
            m_renderTarget->targetLastAccess(),
            {
                .pipelineStages = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                .accessMask = VK_ACCESS_NONE,
                .layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            },
            m_swapchainImages[m_swapchainImageIndex]));

        VK_CHECK(m_vkbTable.endCommandBuffer(*commandBuffer));

        auto flushSemaphore = m_semaphorePool->make();
        VkPipelineStageFlags waitDstStageMask =
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

        VkSubmitInfo submitInfo = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = swapchainSemaphore->vkSemaphoreAddressOf(),
            .pWaitDstStageMask = &waitDstStageMask,
            .commandBufferCount = 1,
            .pCommandBuffers = commandBuffer->vkCommandBufferAddressOf(),
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = flushSemaphore->vkSemaphoreAddressOf(),
        };

        VK_CHECK(m_vkbTable.queueSubmit(m_queue, 1, &submitInfo, *frameFence));

        VkPresentInfoKHR presentInfo = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = flushSemaphore->vkSemaphoreAddressOf(),
            .swapchainCount = 1,
            .pSwapchains = &m_swapchain.swapchain,
            .pImageIndices = &m_swapchainImageIndex,
        };

        m_vkbTable.queuePresentKHR(m_queue, &presentInfo);
    }

private:
    RenderContextVulkanImpl* impl() const
    {
        return m_renderContext->static_impl_cast<RenderContextVulkanImpl>();
    }

    VulkanContext* vk() const { return impl()->vulkanContext(); }

    bool m_clockwiseFill;
    vkb::Instance m_instance;
    vkb::InstanceDispatchTable m_instanceFns;
    vkb::PhysicalDevice m_physicalDevice;
    vkb::Device m_device;
    vkb::DispatchTable m_vkbTable;
    VkQueue m_queue;

    VkSurfaceKHR m_windowSurface = VK_NULL_HANDLE;
    vkb::Swapchain m_swapchain;
    std::vector<VkImage> m_swapchainImages;
    std::vector<rcp<vkutil::TextureView>> m_swapchainImageViews;
    uint32_t m_swapchainImageIndex = 0;

    rcp<vkutil::ResourcePool<vkutil::CommandBuffer>> m_commandBufferPool;
    rcp<vkutil::ResourcePool<vkutil::Semaphore>> m_semaphorePool;
    rcp<vkutil::ResourcePool<vkutil::Fence>> m_fencePool;

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
