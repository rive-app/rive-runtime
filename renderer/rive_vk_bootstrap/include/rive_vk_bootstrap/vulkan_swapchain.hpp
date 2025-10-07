/*
 * Copyright 2025 Rive
 */

#include <vulkan/vulkan.h>
#include <vector>

#include "rive/renderer/vulkan/vulkan_context.hpp"
#include "rive_vk_bootstrap/vulkan_frame_synchronizer.hpp"

namespace rive_vkb
{
class VulkanDevice;
class VulkanInstance;

class VulkanSwapchain : public VulkanFrameSynchronizer
{
    using Super = VulkanFrameSynchronizer;

public:
    struct Options
    {
        // Span of desired formats, ordered by preference.
        std::vector<VkSurfaceFormatKHR> formatPreferences;
        std::vector<VkPresentModeKHR> presentModePreferences;

        VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        uint64_t initialFrameNumber = 0;
        uint32_t preferredImageCount = 2;
    };

    VulkanSwapchain(VulkanInstance&,
                    VulkanDevice&,
                    rive::rcp<rive::gpu::VulkanContext>,
                    VkSurfaceKHR,
                    const Options&);
    ~VulkanSwapchain();

    VulkanSwapchain(const VulkanSwapchain&) = delete;
    VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;

    VkImage currentVkImage() const { return current().image; }
    VkImageView currentVkImageView() const { return current().view; }
    rive::gpu::vkutil::ImageAccess currentLastAccess() const
    {
        return current().lastAccess;
    }

    VkFormat imageFormat() const { return m_imageFormat; }
    VkImageUsageFlags imageUsageFlags() const { return m_imageUsageFlags; }

    bool isFrameStarted() const;
    void beginFrame();

    // Queue a copy of the swapchain image for this frame with optional bounds.
    // Must be done before endFrame.
    void queueImageCopy(rive::gpu::vkutil::ImageAccess* inOutLastAccess,
                        rive::IAABB optPixelReadBounds = {});

    using Super::queueImageCopy;

    void endFrame(const rive::gpu::vkutil::ImageAccess&);

    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }

private:
    struct SwapchainImage
    {
        VkImage image;
        VkImageView view;
        rive::gpu::vkutil::ImageAccess lastAccess;
    };

    SwapchainImage& current()
    {
        return m_swapchainImages.at(m_currentImageIndex);
    }

    const SwapchainImage& current() const
    {
        return m_swapchainImages.at(m_currentImageIndex);
    }

    VkSurfaceFormatKHR findBestFormat(
        VulkanDevice& device,
        VkSurfaceKHR surface,
        const std::vector<VkSurfaceFormatKHR>& preferences);

    VkPresentModeKHR findBestPresentMode(
        VulkanDevice& device,
        VkSurfaceKHR surface,
        const std::vector<VkPresentModeKHR>& presentModePreferences);

    VkSwapchainKHR m_swapchain;

    VkFormat m_imageFormat;
    VkImageUsageFlags m_imageUsageFlags;
    uint32_t m_width;
    uint32_t m_height;

    uint32_t m_currentImageIndex = std::numeric_limits<uint32_t>::max();

    std::vector<SwapchainImage> m_swapchainImages;

    // These are all the commands the swapchain needs to do its work - this
    // macro is also used to load them in the .cpp
#define RIVE_VK_SWAPCHAIN_INSTANCE_COMMANDS(F)                                 \
    F(vkAcquireNextImageKHR)                                                   \
    F(vkDestroySwapchainKHR)                                                   \
    F(vkDestroyImageView)                                                      \
    F(vkDeviceWaitIdle)                                                        \
    F(vkQueuePresentKHR)

#define DECLARE_VULKAN_COMMAND(name) PFN_##name m_##name = nullptr;
    RIVE_VK_SWAPCHAIN_INSTANCE_COMMANDS(DECLARE_VULKAN_COMMAND)
#undef DECLARE_VULKAN_COMMAND
};
} // namespace rive_vkb
