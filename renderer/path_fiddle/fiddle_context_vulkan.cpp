/*
 * Copyright 2024 Rive
 */

#include "fiddle_context.hpp"

#if !defined(RIVE_VULKAN) || defined(RIVE_TOOLS_NO_GLFW)

std::unique_ptr<FiddleContext> FiddleContext::MakeVulkanPLS(FiddleContextOptions options)
{
    return nullptr;
}

#else

#include "rive_vk_bootstrap/rive_vk_bootstrap.hpp"
#include "rive_vk_bootstrap/vulkan_fence_pool.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "rive/renderer/vulkan/render_context_vulkan_impl.hpp"
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include <vk_mem_alloc.h>

using namespace rive;
using namespace rive::gpu;

// +1 because PLS doesn't wait for the previous fence until partway through flush.
// (After we need to acquire a new image from the swapchain.)
static constexpr int kResourcePoolSize = gpu::kBufferRingSize + 1;

class FiddleContextVulkanPLS : public FiddleContext
{
public:
    FiddleContextVulkanPLS(FiddleContextOptions options) : m_options(options)
    {
        rive_vkb::load_vulkan();

        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        m_instance = VKB_CHECK(vkb::InstanceBuilder()
                                   .set_app_name("path_fiddle")
                                   .set_engine_name("Rive Renderer")
#ifdef DEBUG
                                   .set_debug_callback(rive_vkb::default_debug_callback)
#endif
                                   .enable_validation_layers(m_options.enableVulkanValidationLayers)
                                   .enable_extensions(glfwExtensionCount, glfwExtensions)
                                   .build());
        m_instanceDispatch = m_instance.make_table();

        VulkanFeatures vulkanFeatures;
        std::tie(m_physicalDevice, vulkanFeatures) = rive_vkb::select_physical_device(
            vkb::PhysicalDeviceSelector(m_instance).defer_surface_initialization(),
            m_options.gpuNameFilter);
        m_device = VKB_CHECK(vkb::DeviceBuilder(m_physicalDevice).build());
        m_vkDispatch = m_device.make_table();
        m_queue = VKB_CHECK(m_device.get_queue(vkb::QueueType::graphics));

        VkCommandPoolCreateInfo commandPoolCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VkCommandPoolCreateFlagBits::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = *m_device.get_queue_index(vkb::QueueType::graphics),
        };

        VK_CHECK(m_vkDispatch.createCommandPool(&commandPoolCreateInfo, nullptr, &m_commandPool));

        VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = m_commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };

        for (VkCommandBuffer& commandBuffer : m_commandBuffers)
        {
            VK_CHECK(
                m_vkDispatch.allocateCommandBuffers(&commandBufferAllocateInfo, &commandBuffer));
        }

        m_plsContext = PLSRenderContextVulkanImpl::MakeContext(m_instance,
                                                               m_physicalDevice,
                                                               m_device,
                                                               vulkanFeatures,
                                                               m_instance.fp_vkGetInstanceProcAddr,
                                                               m_instance.fp_vkGetDeviceProcAddr);

        VkSemaphoreCreateInfo semaphoreCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        };
        for (VkSemaphore& semaphore : m_semaphores)
        {
            VK_CHECK(m_vkDispatch.createSemaphore(&semaphoreCreateInfo, nullptr, &semaphore));
        }

        m_fencePool = make_rcp<VulkanFencePool>(ref_rcp(vk()));
    }

    ~FiddleContextVulkanPLS()
    {
        // Destroy these before destroying the VkDevice.
        m_plsContext.reset();
        m_renderTarget.reset();
        m_pixelReadBuffer.reset();
        m_swapchainImageViews.clear();
        m_fencePool.reset();
        m_frameFence.reset();

        VK_CHECK(m_vkDispatch.queueWaitIdle(m_queue));

        m_vkDispatch.freeCommandBuffers(m_commandPool, kResourcePoolSize, m_commandBuffers);
        m_vkDispatch.destroyCommandPool(m_commandPool, nullptr);

        for (VkSemaphore semaphore : m_semaphores)
        {
            m_vkDispatch.destroySemaphore(semaphore, nullptr);
        }

        if (m_swapchain != VK_NULL_HANDLE)
        {
            vkb::destroy_swapchain(m_swapchain);
        }

        if (m_windowSurface != VK_NULL_HANDLE)
        {
            m_instanceDispatch.destroySurfaceKHR(m_windowSurface, nullptr);
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

    Factory* factory() override { return m_plsContext.get(); }

    rive::gpu::PLSRenderContext* plsContextOrNull() override { return m_plsContext.get(); }

    rive::gpu::PLSRenderTarget* plsRenderTargetOrNull() override { return m_renderTarget.get(); }

    void onSizeChanged(GLFWwindow* window, int width, int height, uint32_t sampleCount) override
    {
        VK_CHECK(m_vkDispatch.queueWaitIdle(m_queue));

        if (m_swapchain != VK_NULL_HANDLE)
        {
            vkb::destroy_swapchain(m_swapchain);
        }

        if (m_windowSurface != VK_NULL_HANDLE)
        {
            m_instanceDispatch.destroySurfaceKHR(m_windowSurface, nullptr);
        }

        VK_CHECK(glfwCreateWindowSurface(m_instance, window, nullptr, &m_windowSurface));

        VkSurfaceCapabilitiesKHR windowCapabilities;
        VK_CHECK(
            m_instanceDispatch.fp_vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice,
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
            .add_fallback_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)
            .add_fallback_present_mode(VK_PRESENT_MODE_FIFO_RELAXED_KHR)
            .add_fallback_present_mode(VK_PRESENT_MODE_FIFO_KHR);
        if (windowCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT)
        {
            swapchainBuilder.add_image_usage_flags(VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);
            if (m_options.enableReadPixels)
            {
                swapchainBuilder.add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
            }
        }
        else
        {
            swapchainBuilder.add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
                .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT);
        }
        m_swapchain = VKB_CHECK(swapchainBuilder.build());
        m_swapchainImages = *m_swapchain.get_images();

        m_swapchainImageViews.clear();
        m_swapchainImageViews.reserve(m_swapchainImages.size());
        for (VkImage image : m_swapchainImages)
        {
            m_swapchainImageViews.push_back(
                vk()->makeExternalTextureView(m_swapchain.image_usage_flags,
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

        m_swapchainImageLayouts = std::vector(m_swapchainImages.size(), VK_IMAGE_LAYOUT_UNDEFINED);

        m_swapchainImageIndex = 0;

        m_renderTarget = impl()->makeRenderTarget(width, height, m_swapchain.image_format);

        m_pixelReadBuffer = nullptr;
    }

    void toggleZoomWindow() override {}

    std::unique_ptr<Renderer> makeRenderer(int width, int height) override
    {
        return std::make_unique<RiveRenderer>(m_plsContext.get());
    }

    void begin(const PLSRenderContext::FrameDescriptor& frameDescriptor) override
    {
        m_vkDispatch.acquireNextImageKHR(m_swapchain,
                                         UINT64_MAX,
                                         m_semaphores[m_resourcePoolIdx],
                                         VK_NULL_HANDLE,
                                         &m_swapchainImageIndex);

        m_plsContext->beginFrame(std::move(frameDescriptor));

        VkCommandBuffer commandBuffer = m_commandBuffers[m_resourcePoolIdx];
        m_vkDispatch.resetCommandBuffer(commandBuffer, {});

        VkCommandBufferBeginInfo commandBufferBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        };

        m_vkDispatch.beginCommandBuffer(commandBuffer, &commandBufferBeginInfo);

        m_renderTarget->setTargetTextureView(m_swapchainImageViews[m_swapchainImageIndex]);

        insertSwapchainImageBarrier(VK_IMAGE_LAYOUT_GENERAL);

        m_frameFence = m_fencePool->makeFence();
    }

    void flushPLSContext() final
    {
        m_plsContext->flush({
            .renderTarget = m_renderTarget.get(),
            .externalCommandBuffer = m_commandBuffers[m_resourcePoolIdx],
            .frameCompletionFence = m_frameFence.get(),
        });
    }

    void end(GLFWwindow* window, std::vector<uint8_t>* pixelData) final
    {
        flushPLSContext();

        VkCommandBuffer commandBuffer = m_commandBuffers[m_resourcePoolIdx];
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

            insertSwapchainImageBarrier(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

            m_vkDispatch.cmdCopyImageToBuffer(commandBuffer,
                                              m_swapchainImages[m_swapchainImageIndex],
                                              VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                              *m_pixelReadBuffer,
                                              1,
                                              &imageCopyDesc);

            vk()->insertBufferMemoryBarrier(commandBuffer,
                                            VK_ACCESS_TRANSFER_WRITE_BIT,
                                            VK_ACCESS_HOST_READ_BIT,
                                            *m_pixelReadBuffer);
        }

        insertSwapchainImageBarrier(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        VK_CHECK(m_vkDispatch.endCommandBuffer(commandBuffer));

        VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo submitInfo = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pWaitDstStageMask = &waitDstStageMask,
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffer,
        };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &m_semaphores[m_resourcePoolIdx];

        VK_CHECK(m_vkDispatch.queueSubmit(m_queue, 1, &submitInfo, *m_frameFence));

        if (pixelData != nullptr)
        {
            // Copy the buffer containing the framebuffer contents to pixelData.
            pixelData->resize(h * w * 4);

            // Wait for all rendering to complete before transferring the
            // framebuffer data to pixelData.
            m_frameFence->wait();

            assert(m_pixelReadBuffer->info().size == h * w * 4);
            for (uint32_t y = 0; y < h; ++y)
            {
                auto src = static_cast<const uint8_t*>(m_pixelReadBuffer->contents()) + w * 4 * y;
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
            .swapchainCount = 1,
            .pSwapchains = &m_swapchain.swapchain,
            .pImageIndices = &m_swapchainImageIndex,
        };

        m_vkDispatch.queuePresentKHR(m_queue, &presentInfo);

        m_resourcePoolIdx = (m_resourcePoolIdx + 1) % kResourcePoolSize;
        m_frameFence = nullptr;
    }

private:
    PLSRenderContextVulkanImpl* impl() const
    {
        return m_plsContext->static_impl_cast<PLSRenderContextVulkanImpl>();
    }

    VulkanContext* vk() const { return impl()->vulkanContext(); }

    void insertSwapchainImageBarrier(VkImageLayout newLayout)
    {
        vk()->insertImageMemoryBarrier(m_commandBuffers[m_resourcePoolIdx],
                                       m_swapchainImages[m_swapchainImageIndex],
                                       m_swapchainImageLayouts[m_swapchainImageIndex],
                                       newLayout);
        m_swapchainImageLayouts[m_swapchainImageIndex] = newLayout;
    }

    const FiddleContextOptions m_options;
    vkb::Instance m_instance;
    vkb::InstanceDispatchTable m_instanceDispatch;
    vkb::PhysicalDevice m_physicalDevice;
    vkb::Device m_device;
    vkb::DispatchTable m_vkDispatch;
    VkQueue m_queue;

    VkSurfaceKHR m_windowSurface = VK_NULL_HANDLE;
    vkb::Swapchain m_swapchain;
    std::vector<VkImage> m_swapchainImages;
    std::vector<rcp<vkutil::TextureView>> m_swapchainImageViews;
    std::vector<VkImageLayout> m_swapchainImageLayouts;
    uint32_t m_swapchainImageIndex;

    VkCommandPool m_commandPool;
    VkCommandBuffer m_commandBuffers[kResourcePoolSize];
    VkSemaphore m_semaphores[kResourcePoolSize];

    rcp<VulkanFencePool> m_fencePool;
    rcp<VulkanFence> m_frameFence;

    std::unique_ptr<PLSRenderContext> m_plsContext;
    rcp<PLSRenderTargetVulkan> m_renderTarget;
    rcp<vkutil::Buffer> m_pixelReadBuffer;

    int m_resourcePoolIdx = 0;
};

std::unique_ptr<FiddleContext> FiddleContext::MakeVulkanPLS(FiddleContextOptions options)
{
    return std::make_unique<FiddleContextVulkanPLS>(options);
}

#endif
