/*
 * Copyright 2025 Rive
 */

#include "rive_vk_bootstrap/vulkan_device.hpp"
#include "rive_vk_bootstrap/vulkan_instance.hpp"
#include "rive_vk_bootstrap/vulkan_swapchain.hpp"
#include "logging.hpp"
#include "vulkan_error_handling.hpp"
#include "vulkan_library.hpp"

namespace rive_vkb
{
std::unique_ptr<VulkanSwapchain> VulkanSwapchain::Create(
    VulkanInstance& instance,
    VulkanDevice& device,
    rive::rcp<rive::gpu::VulkanContext> vk,
    VkSurfaceKHR surface,
    const Options& opts)
{
    bool success = true;

    // Private constructor, can't use make_unique
    auto outSwapchain =
        std::unique_ptr<VulkanSwapchain>{new VulkanSwapchain(instance,
                                                             device,
                                                             std::move(vk),
                                                             surface,
                                                             opts,
                                                             &success)};
    CONFIRM_OR_RETURN_VALUE(success, nullptr);
    return outSwapchain;
}

VulkanSwapchain::VulkanSwapchain(VulkanInstance& instance,
                                 VulkanDevice& device,
                                 rive::rcp<rive::gpu::VulkanContext>&& vk,
                                 VkSurfaceKHR surface,
                                 const Options& opts,
                                 bool* successOut) :
    Super(instance,
          device,
          std::move(vk),
          {
              .initialFrameNumber = opts.initialFrameNumber,
              .externalGPUSynchronization = true,
          },
          successOut)
{
    // Validate parameters before handling the success value from the super
    // class so that programmer errors get caught sooner
    assert(opts.formatPreferences.size() > 0 &&
           "Must request at least one surface format");
    assert(opts.presentModePreferences.size() > 0 &&
           "Must request at least one present mode");

    // The super class sets this when it constructs, so check it before
    // re-setting it to false.
    if (!*successOut)
    {
        return;
    }

    *successOut = false;

    // Load all of the functions we care about
#define LOAD(name) LOAD_MEMBER_INSTANCE_FUNC_OR_RETURN(this, name, instance);
    RIVE_VK_SWAPCHAIN_INSTANCE_COMMANDS(LOAD);
#undef LOAD

    // Check the device to see what our best-match image format is
    auto surfaceFormat =
        tryFindBestFormat(device, surface, opts.formatPreferences);
    CONFIRM_OR_RETURN(surfaceFormat.has_value());
    auto presentMode =
        tryFindBestPresentMode(device, surface, opts.presentModePreferences);
    CONFIRM_OR_RETURN(presentMode.has_value());

    VkCompositeAlphaFlagBitsKHR compositeAlphaFlags =
#if defined(__ANDROID__)
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
#else
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
#endif

    VkSurfaceCapabilitiesKHR surfaceCaps;
    VK_CONFIRM_OR_RETURN_MSG(
        device.getSurfaceCapabilities(surface, &surfaceCaps),
        "Failed to get Vulkan device surface capabilities");

    VkSwapchainCreateInfoKHR swapchainCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount =
            std::max(opts.preferredImageCount, surfaceCaps.minImageCount),
        .imageFormat = surfaceFormat->format,
        .imageColorSpace = surfaceFormat->colorSpace,
        .imageExtent = surfaceCaps.currentExtent,
        .imageArrayLayers = 1,
        .imageUsage = opts.imageUsageFlags,
        .preTransform = surfaceCaps.currentTransform,
        .compositeAlpha = compositeAlphaFlags,
        .presentMode = presentMode.value(),
        .clipped = true,
    };

    m_width = surfaceCaps.currentExtent.width;
    m_height = surfaceCaps.currentExtent.height;

    DEFINE_AND_LOAD_INSTANCE_FUNC_OR_RETURN(vkCreateSwapchainKHR, instance);

    VK_CONFIRM_OR_RETURN_MSG(vkCreateSwapchainKHR(device.vkDevice(),
                                                  &swapchainCreateInfo,
                                                  nullptr,
                                                  &m_swapchain),
                             "Failed to create Vulkan swapchain");

    m_imageFormat = surfaceFormat->format;
    m_imageUsageFlags = opts.imageUsageFlags;

    // Get the swapchain images and then build out our internal image data
    std::vector<VkImage> vkImages;
    {
        DEFINE_AND_LOAD_INSTANCE_FUNC_OR_RETURN(vkGetSwapchainImagesKHR,
                                                instance);

        uint32_t count;
        VK_CONFIRM_OR_RETURN_MSG(
            vkGetSwapchainImagesKHR(device.vkDevice(),
                                    m_swapchain,
                                    &count,
                                    nullptr),
            "Failed to query Vulkan swapchain image count");
        vkImages.resize(count);
        VK_CONFIRM_OR_RETURN_MSG(vkGetSwapchainImagesKHR(device.vkDevice(),
                                                         m_swapchain,
                                                         &count,
                                                         vkImages.data()),
                                 "Failed to get Vulkan swapchain images");
    }

    m_swapchainImages.resize(vkImages.size());

    DEFINE_AND_LOAD_INSTANCE_FUNC_OR_RETURN(vkCreateImageView, instance);

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

        VK_CONFIRM_OR_RETURN_MSG(vkCreateImageView(device.vkDevice(),
                                                   &viewCreateInfo,
                                                   nullptr,
                                                   &m_swapchainImages[i].view),
                                 "Failed to create Vulkan image view");

        VK_CONFIRM_OR_RETURN_MSG(
            createSemaphore(&m_swapchainImages[i].frameSemaphore),
            "Failed to create Vulkan frame semaphore");
    }

    *successOut = true;
}

VulkanSwapchain::~VulkanSwapchain()
{
    // Don't do anything until everything is flushed through.
    if (m_vkDeviceWaitIdle != nullptr)
    {
        std::ignore = m_vkDeviceWaitIdle(vkDevice());
    }

    for (auto& data : m_swapchainImages)
    {
        assert(
            m_vkDestroyImageView != nullptr &&
            "should not have created swapchain images until m_vkDestroyImageView was loaded");
        destroySemaphore(data.frameSemaphore);
        m_vkDestroyImageView(vkDevice(), data.view, nullptr);
    }

    if (m_vkDestroySwapchainKHR != nullptr)
    {
        m_vkDestroySwapchainKHR(vkDevice(), m_swapchain, nullptr);
    }
}

std::optional<VkSurfaceFormatKHR> VulkanSwapchain::tryFindBestFormat(
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

    LOG_ERROR_LINE("Could not find any preferred Vulkan surface format");
    return std::nullopt;
}

std::optional<VkPresentModeKHR> VulkanSwapchain::tryFindBestPresentMode(
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

    LOG_ERROR_LINE("Could not find any preferred Vulkan present mode");
    return std::nullopt;
}

bool VulkanSwapchain::isFrameStarted() const
{
    return m_currentImageIndex < m_swapchainImages.size();
}

VkResult VulkanSwapchain::beginFrame()
{
    assert(!isFrameStarted());

    // Do the work for the frame synchronization to begin
    VkSemaphore semaphoreToSignal;

    VK_RETURN_RESULT_ON_ERROR(
        Super::waitForFenceAndBeginFrame(&semaphoreToSignal));

    // Next, acquire the next image from the swap chain, and signal the
    static constexpr auto NO_TIMEOUT = std::numeric_limits<uint64_t>::max();
    VK_RETURN_RESULT_ON_ERROR_MSG(
        m_vkAcquireNextImageKHR(vkDevice(),
                                m_swapchain,
                                NO_TIMEOUT,
                                semaphoreToSignal,
                                VK_NULL_HANDLE,
                                &m_currentImageIndex),
        "Failed to acquire next Vulkan swapchain image");
    return VK_SUCCESS;
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

VkResult VulkanSwapchain::endFrame(
    const rive::gpu::vkutil::ImageAccess& lastAccess)
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
    VK_RETURN_RESULT_ON_ERROR(Super::endFrame(swapImage.frameSemaphore));

    // Now queue the actual presentation of the swpchain image
    VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &swapImage.frameSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &m_swapchain,
        .pImageIndices = &m_currentImageIndex,
    };

    VK_RETURN_RESULT_ON_ERROR_MSG(
        m_vkQueuePresentKHR(graphicsQueue(), &presentInfo),
        "Failed to queue Vulkan presentation");

    // This puts us in the !IsFrameStarted() state
    m_currentImageIndex = std::numeric_limits<uint32_t>::max();
    return VK_SUCCESS;
}

} // namespace rive_vkb