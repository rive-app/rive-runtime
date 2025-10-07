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

// This is similar to a swapchain, but instead renders to an offscreen image.
class VulkanHeadlessFrameSynchronizer : public VulkanFrameSynchronizer
{
    using Super = VulkanFrameSynchronizer;

public:
    struct Options
    {
        uint32_t width = 0;
        uint32_t height = 0;
        VkFormat imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
        VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        uint64_t initialFrameNumber = 0;
    };

    VulkanHeadlessFrameSynchronizer(VulkanInstance&,
                                    VulkanDevice&,
                                    rive::rcp<rive::gpu::VulkanContext>,
                                    const Options&);
    ~VulkanHeadlessFrameSynchronizer();

    VulkanHeadlessFrameSynchronizer(const VulkanHeadlessFrameSynchronizer&) =
        delete;
    VulkanHeadlessFrameSynchronizer& operator=(
        const VulkanHeadlessFrameSynchronizer&) = delete;

    VkImage vkImage() const { return *m_image; }
    VkImageView vkImageView() const { return *m_imageView; }
    rive::gpu::vkutil::ImageAccess lastAccess() const
    {
        return m_lastImageAccess;
    }

    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }

    VkFormat imageFormat() const { return m_imageFormat; }
    VkImageUsageFlags imageUsageFlags() const { return m_imageUsageFlags; }

    bool isFrameStarted() const;
    void beginFrame();

    void queueImageCopy(rive::gpu::vkutil::ImageAccess* inOutLastAccess,
                        rive::IAABB optPixelReadBounds = {});

    void endFrame(const rive::gpu::vkutil::ImageAccess&);

private:
    bool m_isInFrame = false;

    VkFormat m_imageFormat;
    VkImageUsageFlags m_imageUsageFlags;
    uint32_t m_width;
    uint32_t m_height;

    rive::gpu::vkutil::ImageAccess m_lastImageAccess;

    const rive::rcp<rive::gpu::vkutil::Image> m_image;
    const rive::rcp<rive::gpu::vkutil::ImageView> m_imageView;

    // These are all the commands the swapchain needs to do its work - this
    // macro is also used to load them in the .cpp
#define RIVE_VK_OFFSCREEN_FRAME_INSTANCE_COMMANDS(F)                           \
    F(vkAcquireNextImageKHR)                                                   \
    F(vkDestroySwapchainKHR)                                                   \
    F(vkDestroyImageView)                                                      \
    F(vkDeviceWaitIdle)                                                        \
    F(vkQueuePresentKHR)

#define DECLARE_VULKAN_COMMAND(name) PFN_##name m_##name = nullptr;
    RIVE_VK_OFFSCREEN_FRAME_INSTANCE_COMMANDS(DECLARE_VULKAN_COMMAND)
#undef DECLARE_VULKAN_COMMAND
};
} // namespace rive_vkb
