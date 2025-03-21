/*
 * Copyright 2024 Rive
 */

#include "fiddle_context.hpp"

#if !defined(RIVE_VULKAN) || defined(RIVE_TOOLS_NO_GLFW)

std::unique_ptr<FiddleContext> FiddleContext::MakeVulkanPLS(
    FiddleContextOptions options)
{
    return nullptr;
}

#else

#include "rive_vk_bootstrap/rive_vk_bootstrap.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "rive/renderer/vulkan/render_context_vulkan_impl.hpp"
#include "rive/renderer/vulkan/vkutil_resource_pool.hpp"
#include "shader_hotload.hpp"
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include <vk_mem_alloc.h>

using namespace rive;
using namespace rive::gpu;

class FiddleContextVulkanPLS : public FiddleContext
{
public:
    FiddleContextVulkanPLS(FiddleContextOptions options) : m_options(options)
    {
        rive_vkb::load_vulkan();

        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        m_instance = VKB_CHECK(
            vkb::InstanceBuilder()
                .set_app_name("path_fiddle")
                .set_engine_name("Rive Renderer")
#ifdef DEBUG
                .set_debug_callback(rive_vkb::default_debug_callback)
                .enable_validation_layers(
                    m_options.enableVulkanValidationLayers)
#endif
                .enable_extensions(glfwExtensionCount, glfwExtensions)
                .require_api_version(1, options.coreFeaturesOnly ? 0 : 3, 0)
                .set_minimum_instance_version(1, 0, 0)
                .build());
        m_instanceTable = m_instance.make_table();

        VulkanFeatures vulkanFeatures;
        std::tie(m_device, vulkanFeatures) = rive_vkb::select_device(
            vkb::PhysicalDeviceSelector(m_instance)
                .defer_surface_initialization(),
            m_options.coreFeaturesOnly ? rive_vkb::FeatureSet::coreOnly
                                       : rive_vkb::FeatureSet::allAvailable,
            m_options.gpuNameFilter);
        m_vkbTable = m_device.make_table();
        m_queue = VKB_CHECK(m_device.get_queue(vkb::QueueType::graphics));
        m_renderContext = RenderContextVulkanImpl::MakeContext(
            m_instance,
            m_device.physical_device,
            m_device,
            vulkanFeatures,
            m_instance.fp_vkGetInstanceProcAddr);
        m_commandBufferPool =
            make_rcp<vkutil::ResourcePool<vkutil::CommandBuffer>>(
                ref_rcp(vk()),
                *m_device.get_queue_index(vkb::QueueType::graphics));
        m_semaphorePool =
            make_rcp<vkutil::ResourcePool<vkutil::Semaphore>>(ref_rcp(vk()));
        m_fencePool =
            make_rcp<vkutil::ResourcePool<vkutil::Fence>>(ref_rcp(vk()));
    }

    ~FiddleContextVulkanPLS()
    {
        // Destroy these before destroying the VkDevice.
        m_renderContext.reset();
        m_renderTarget.reset();
        m_pixelReadBuffer.reset();
        m_swapchainImageViews.clear();
        m_fencePool.reset();
        m_frameFence.reset();

        VK_CHECK(m_vkbTable.queueWaitIdle(m_queue));

        m_swapchainSemaphore = nullptr;
        m_frameFence = nullptr;
        m_frameCommandBuffer = nullptr;

        m_commandBufferPool = nullptr;
        m_semaphorePool = nullptr;
        m_fencePool = nullptr;

        if (m_swapchain != VK_NULL_HANDLE)
        {
            vkb::destroy_swapchain(m_swapchain);
        }

        if (m_windowSurface != VK_NULL_HANDLE)
        {
            m_instanceTable.destroySurfaceKHR(m_windowSurface, nullptr);
        }

        vkb::destroy_device(m_device);
        vkb::destroy_instance(m_instance);
    }

    float dpiScale(GLFWwindow* window) const override
    {
#ifdef __APPLE__
        return 2;
#else
        return 1;
#endif
    }

    Factory* factory() override { return m_renderContext.get(); }

    rive::gpu::RenderContext* renderContextOrNull() override
    {
        return m_renderContext.get();
    }

    rive::gpu::RenderTarget* renderTargetOrNull() override
    {
        return m_renderTarget.get();
    }

    void onSizeChanged(GLFWwindow* window,
                       int width,
                       int height,
                       uint32_t sampleCount) override
    {
        VK_CHECK(m_vkbTable.queueWaitIdle(m_queue));

        if (m_swapchain != VK_NULL_HANDLE)
        {
            vkb::destroy_swapchain(m_swapchain);
        }

        if (m_windowSurface != VK_NULL_HANDLE)
        {
            m_instanceTable.destroySurfaceKHR(m_windowSurface, nullptr);
        }

        VK_CHECK(glfwCreateWindowSurface(m_instance,
                                         window,
                                         nullptr,
                                         &m_windowSurface));

        VkSurfaceCapabilitiesKHR windowCapabilities;
        VK_CHECK(m_instanceTable.fp_vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            m_device.physical_device,
            m_windowSurface,
            &windowCapabilities));

        vkb::SwapchainBuilder swapchainBuilder(m_device, m_windowSurface);
        swapchainBuilder
            .set_desired_format({
                // Swap the target format in "vkcore" mode, just for fun so we
                // test both
                // configurations.
                .format = m_options.coreFeaturesOnly ? VK_FORMAT_B8G8R8A8_UNORM
                                                     : VK_FORMAT_R8G8B8A8_UNORM,
                .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
            })
            .add_fallback_format({
                .format = m_options.coreFeaturesOnly ? VK_FORMAT_R8G8B8A8_UNORM
                                                     : VK_FORMAT_B8G8R8A8_UNORM,
                .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
            })
            .set_desired_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR)
            .add_fallback_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)
            .add_fallback_present_mode(VK_PRESENT_MODE_FIFO_RELAXED_KHR)
            .add_fallback_present_mode(VK_PRESENT_MODE_FIFO_KHR);
        if (!m_options.coreFeaturesOnly &&
            (windowCapabilities.supportedUsageFlags &
             VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT))
        {
            swapchainBuilder.add_image_usage_flags(
                VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);
            if (m_options.enableReadPixels)
            {
                swapchainBuilder.add_image_usage_flags(
                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
            }
        }
        else
        {
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

        m_renderTarget =
            impl()->makeRenderTarget(width, height, m_swapchain.image_format);

        m_pixelReadBuffer = nullptr;
    }

    void toggleZoomWindow() override {}

    void hotloadShaders() override
    {
        rive::Span<const uint32_t> newShaderBytecodeData =
            loadNewShaderFileData();
        if (newShaderBytecodeData.size() > 0)
        {
            impl()->hotloadShaders(newShaderBytecodeData);
        }
    }

    std::unique_ptr<Renderer> makeRenderer(int width, int height) override
    {
        return std::make_unique<RiveRenderer>(m_renderContext.get());
    }

    void begin(const RenderContext::FrameDescriptor& frameDescriptor) override
    {
        m_swapchainSemaphore = m_semaphorePool->make();
        m_vkbTable.acquireNextImageKHR(m_swapchain,
                                       UINT64_MAX,
                                       *m_swapchainSemaphore,
                                       VK_NULL_HANDLE,
                                       &m_swapchainImageIndex);

        m_renderContext->beginFrame(std::move(frameDescriptor));

        m_frameCommandBuffer = m_commandBufferPool->make();

        VkCommandBufferBeginInfo commandBufferBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        };
        m_vkbTable.beginCommandBuffer(*m_frameCommandBuffer,
                                      &commandBufferBeginInfo);

        m_renderTarget->setTargetTextureView(
            m_swapchainImageViews[m_swapchainImageIndex],
            {});

        m_frameFence = m_fencePool->make();
    }

    void flushPLSContext() final
    {
        m_renderContext->flush({
            .renderTarget = m_renderTarget.get(),
            .externalCommandBuffer = *m_frameCommandBuffer,
            .frameCompletionFence = m_frameFence.get(),
        });
    }

    void end(GLFWwindow* window, std::vector<uint8_t>* pixelData) final
    {
        flushPLSContext();

        uint32_t w = m_renderTarget->width();
        uint32_t h = m_renderTarget->height();

        if (pixelData != nullptr)
        {
            // Copy the framebuffer out to a buffer.
            if (m_pixelReadBuffer == nullptr)
            {
                m_pixelReadBuffer = vk()->makeBuffer(
                    {
                        .size = h * w * 4,
                        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    },
                    vkutil::Mappability::readWrite);
            }
            assert(m_pixelReadBuffer->info().size == h * w * 4);

            m_renderTarget->setTargetLastAccess(vk()->simpleImageMemoryBarrier(
                *m_frameCommandBuffer,
                m_renderTarget->targetLastAccess(),
                {
                    .pipelineStages = VK_PIPELINE_STAGE_TRANSFER_BIT,
                    .accessMask = VK_ACCESS_TRANSFER_READ_BIT,
                    .layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                },
                m_swapchainImages[m_swapchainImageIndex]));

            VkBufferImageCopy imageCopyDesc = {
                .imageSubresource =
                    {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .mipLevel = 0,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                    },
                .imageExtent = {w, h, 1},
            };

            m_vkbTable.cmdCopyImageToBuffer(
                *m_frameCommandBuffer,
                m_swapchainImages[m_swapchainImageIndex],
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                *m_pixelReadBuffer,
                1,
                &imageCopyDesc);

            vk()->bufferMemoryBarrier(
                *m_frameCommandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_HOST_BIT,
                0,
                {
                    .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                    .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
                    .buffer = *m_pixelReadBuffer,
                });
        }

        m_renderTarget->setTargetLastAccess(vk()->simpleImageMemoryBarrier(
            *m_frameCommandBuffer,
            m_renderTarget->targetLastAccess(),
            {
                .pipelineStages = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                .accessMask = VK_ACCESS_NONE,
                .layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            },
            m_swapchainImages[m_swapchainImageIndex]));

        VK_CHECK(m_vkbTable.endCommandBuffer(*m_frameCommandBuffer));

        auto flushSemaphore = m_semaphorePool->make();
        VkPipelineStageFlags waitDstStageMask =
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

        VkSubmitInfo submitInfo = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = m_swapchainSemaphore->vkSemaphoreAddressOf(),
            .pWaitDstStageMask = &waitDstStageMask,
            .commandBufferCount = 1,
            .pCommandBuffers = m_frameCommandBuffer->vkCommandBufferAddressOf(),
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = flushSemaphore->vkSemaphoreAddressOf(),
        };

        VK_CHECK(
            m_vkbTable.queueSubmit(m_queue, 1, &submitInfo, *m_frameFence));

        if (pixelData != nullptr)
        {
            // Wait for all rendering to complete before transferring the
            // framebuffer data to pixelData.
            m_frameFence->wait();
            m_pixelReadBuffer->invalidateContents();

            // Copy the buffer containing the framebuffer contents to pixelData.
            pixelData->resize(h * w * 4);

            assert(m_pixelReadBuffer->info().size == h * w * 4);
            for (uint32_t y = 0; y < h; ++y)
            {
                auto src =
                    static_cast<const uint8_t*>(m_pixelReadBuffer->contents()) +
                    w * 4 * y;
                uint8_t* dst = pixelData->data() + (h - y - 1) * w * 4;
                memcpy(dst, src, w * 4);
                if (m_swapchain.image_format == VK_FORMAT_B8G8R8A8_UNORM)
                {
                    // Reverse bgr -> rgb.
                    for (uint32_t x = 0; x < w * 4; x += 4)
                    {
                        std::swap(dst[x], dst[x + 2]);
                    }
                }
            }
        }

        VkPresentInfoKHR presentInfo = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = flushSemaphore->vkSemaphoreAddressOf(),
            .swapchainCount = 1,
            .pSwapchains = &m_swapchain.swapchain,
            .pImageIndices = &m_swapchainImageIndex,
        };

        m_vkbTable.queuePresentKHR(m_queue, &presentInfo);

        m_swapchainSemaphore = nullptr;
        m_frameFence = nullptr;
        m_frameCommandBuffer = nullptr;
    }

private:
    RenderContextVulkanImpl* impl() const
    {
        return m_renderContext->static_impl_cast<RenderContextVulkanImpl>();
    }

    VulkanContext* vk() const { return impl()->vulkanContext(); }

    const FiddleContextOptions m_options;
    vkb::Instance m_instance;
    vkb::InstanceDispatchTable m_instanceTable;
    vkb::Device m_device;
    vkb::DispatchTable m_vkbTable;
    VkQueue m_queue;

    VkSurfaceKHR m_windowSurface = VK_NULL_HANDLE;
    vkb::Swapchain m_swapchain;
    std::vector<VkImage> m_swapchainImages;
    std::vector<rcp<vkutil::TextureView>> m_swapchainImageViews;
    uint32_t m_swapchainImageIndex = 0;

    rcp<vkutil::ResourcePool<vkutil::CommandBuffer>> m_commandBufferPool;
    rcp<vkutil::CommandBuffer> m_frameCommandBuffer;

    rcp<vkutil::ResourcePool<vkutil::Semaphore>> m_semaphorePool;
    rcp<vkutil::Semaphore> m_swapchainSemaphore;

    rcp<vkutil::ResourcePool<vkutil::Fence>> m_fencePool;
    rcp<vkutil::Fence> m_frameFence;

    std::unique_ptr<RenderContext> m_renderContext;
    rcp<RenderTargetVulkan> m_renderTarget;
    rcp<vkutil::Buffer> m_pixelReadBuffer;
};

std::unique_ptr<FiddleContext> FiddleContext::MakeVulkanPLS(
    FiddleContextOptions options)
{
    return std::make_unique<FiddleContextVulkanPLS>(options);
}

#endif
