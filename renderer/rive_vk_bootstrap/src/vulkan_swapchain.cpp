/*
 * Copyright 2025 Rive
 */

#include "rive_vk_bootstrap/vulkan_device.hpp"
#include "rive_vk_bootstrap/vulkan_instance.hpp"
#include "rive_vk_bootstrap/vulkan_swapchain.hpp"
#include "logging.hpp"
#include "vulkan_library.hpp"

namespace rive_vkb
{
VulkanSwapchain::VulkanSwapchain(VulkanInstance& instance,
                                 VulkanDevice& device,
                                 rive::rcp<rive::gpu::VulkanContext> vk,
                                 VkSurfaceKHR surface,
                                 const Options& opts) :
    Super(instance,
          device,
          std::move(vk),
          {
              .initialFrameNumber = opts.initialFrameNumber,
              .externalGPUSynchronization = true,
          })
{
    assert(opts.formatPreferences.size() > 0 &&
           "Must request at least one surface format");
    assert(opts.presentModePreferences.size() > 0 &&
           "Must request at least one present mode");

    // Load all of the functions we care about
#define LOAD(name) LOAD_REQUIRED_MEMBER_INSTANCE_FUNC(name, instance);
    RIVE_VK_SWAPCHAIN_INSTANCE_COMMANDS(LOAD);
#undef LOAD

    // Check the device to see what our best-match image format is
    auto surfaceFormat =
        findBestFormat(device, surface, opts.formatPreferences);
    auto presentMode =
        findBestPresentMode(device, surface, opts.presentModePreferences);

    VkCompositeAlphaFlagBitsKHR compositeAlphaFlags =
#if defined(__ANDROID__)
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
#else
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
#endif

    auto surfaceCaps = device.getSurfaceCapabilities(surface);

    VkSwapchainCreateInfoKHR swapchainCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount =
            std::max(opts.preferredImageCount, surfaceCaps.minImageCount),
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = surfaceCaps.currentExtent,
        .imageArrayLayers = 1,
        .imageUsage = opts.imageUsageFlags,
        .preTransform = surfaceCaps.currentTransform,
        .compositeAlpha = compositeAlphaFlags,
        .presentMode = presentMode,
        .clipped = true,
    };

    m_width = surfaceCaps.currentExtent.width;
    m_height = surfaceCaps.currentExtent.height;

    DEFINE_AND_LOAD_INSTANCE_FUNC(vkCreateSwapchainKHR, instance);
    VK_CHECK(vkCreateSwapchainKHR(device.vkDevice(),
                                  &swapchainCreateInfo,
                                  nullptr,
                                  &m_swapchain));

    m_imageFormat = surfaceFormat.format;
    m_imageUsageFlags = opts.imageUsageFlags;

    // Get the swapchain images and then build out our internal image data
    std::vector<VkImage> vkImages;
    {
        DEFINE_AND_LOAD_INSTANCE_FUNC(vkGetSwapchainImagesKHR, instance);

        uint32_t count;
        vkGetSwapchainImagesKHR(device.vkDevice(),
                                m_swapchain,
                                &count,
                                nullptr);
        vkImages.resize(count);
        vkGetSwapchainImagesKHR(device.vkDevice(),
                                m_swapchain,
                                &count,
                                vkImages.data());
    }

    m_swapchainImages.resize(vkImages.size());

    DEFINE_AND_LOAD_INSTANCE_FUNC(vkCreateImageView, instance);
    for (uint32_t i = 0; i < vkImages.size(); i++)
    {
        m_swapchainImages[i].image = vkImages[i];

        VkImageViewCreateInfo viewCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = vkImages[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = m_imageFormat,
            .subresourceRange =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .levelCount = 1,
                    .layerCount = 1,
                },
        };

        vkCreateImageView(device.vkDevice(),
                          &viewCreateInfo,
                          nullptr,
                          &m_swapchainImages[i].view);
    }
}

VulkanSwapchain::~VulkanSwapchain()
{
    // Don't do anything until everything is flushed through.
    m_vkDeviceWaitIdle(vkDevice());

    for (auto& data : m_swapchainImages)
    {
        m_vkDestroyImageView(vkDevice(), data.view, nullptr);
    }

    m_vkDestroySwapchainKHR(vkDevice(), m_swapchain, nullptr);
}

VkSurfaceFormatKHR VulkanSwapchain::findBestFormat(
    VulkanDevice& device,
    VkSurfaceKHR surface,
    const std::vector<VkSurfaceFormatKHR>& preferences)
{
    auto formats = device.getSurfaceFormats(surface);
    for (auto& pref : preferences)
    {
        for (auto& format : formats)
        {
            if (format.format == pref.format &&
                format.colorSpace == pref.colorSpace)
            {
                return pref;
            }
        }
    }

    LOG_ERROR_LINE("Could not find any preferred surface format");
    abort();
}

VkPresentModeKHR VulkanSwapchain::findBestPresentMode(
    VulkanDevice& device,
    VkSurfaceKHR surface,
    const std::vector<VkPresentModeKHR>& presentModePreferences)
{
    auto modes = device.getSurfacePresentModes(surface);
    for (auto& pref : presentModePreferences)
    {
        for (auto& mode : modes)
        {
            if (mode == pref)
            {
                return pref;
            }
        }
    }

    LOG_ERROR_LINE("Could not find any preferred present mode");
    abort();
}

bool VulkanSwapchain::isFrameStarted() const
{
    return m_currentImageIndex < m_swapchainImages.size();
}

void VulkanSwapchain::beginFrame()
{
    assert(!isFrameStarted());

    // Do the work for the frame synchronization to begin
    auto semaphoreToSignal = Super::waitForFenceAndBeginFrame();

    // Next, acquire the next image from the swap chain, and signal the
    static constexpr auto NO_TIMEOUT = std::numeric_limits<uint64_t>::max();
    VK_CHECK(m_vkAcquireNextImageKHR(vkDevice(),
                                     m_swapchain,
                                     NO_TIMEOUT,
                                     semaphoreToSignal,
                                     VK_NULL_HANDLE,
                                     &m_currentImageIndex));
}

void VulkanSwapchain::queueImageCopy(
    rive::gpu::vkutil::ImageAccess* inOutLastAccess,
    rive::IAABB optPixelReadBounds)
{
    if (optPixelReadBounds.empty())
    {
        optPixelReadBounds = rive::IAABB::MakeWH(m_width, m_height);
    }
    queueImageCopy(current().image,
                   m_imageFormat,
                   inOutLastAccess,
                   optPixelReadBounds);
}

void VulkanSwapchain::endFrame(const rive::gpu::vkutil::ImageAccess& lastAccess)
{
    assert(isFrameStarted());

    auto& swapImage = current();

    // Whether or not we attempted to copy from the swapchain we need to
    // transition it to the present layout.
    swapImage.lastAccess = context()->simpleImageMemoryBarrier(
        currentCommandBuffer(),
        lastAccess,
        {
            .pipelineStages = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            .accessMask = VK_ACCESS_NONE,
            .layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        },
        swapImage.image);

    // Now that the memory barrier is in the command buffer, we can end the
    // frame sync frame.
    VkSemaphore waitSemaphore = Super::endFrame();

    // Now queue the actual presentation of the swpchain image
    VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &waitSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &m_swapchain,
        .pImageIndices = &m_currentImageIndex,
    };

    m_vkQueuePresentKHR(graphicsQueue(), &presentInfo);

    // This puts us in the !IsFrameStarted() state
    m_currentImageIndex = std::numeric_limits<uint32_t>::max();
}

} // namespace rive_vkb