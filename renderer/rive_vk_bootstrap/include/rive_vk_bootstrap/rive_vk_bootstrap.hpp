/*
 * Copyright 2024 Rive
 */

#include <VkBootstrap.h>
#include "rive/renderer/vulkan/vulkan_context.hpp"

namespace rive_vkb
{
template <typename T>
T vkb_check(vkb::Result<T> result, const char* code, int line, const char* file)
{
    if (!result)
    {
        fprintf(stderr,
                "%s:%u: Error: %s: %s\n",
                file,
                line,
                code,
                result.error().message().c_str());
        abort();
    }
    return *result;
}

#define VKB_CHECK(RESULT)                                                      \
    ::rive_vkb::vkb_check(RESULT, #RESULT, __LINE__, __FILE__)

vkb::SystemInfo load_vulkan();

#ifdef DEBUG
VKAPI_ATTR VkBool32 VKAPI_CALL
default_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT,
                       VkDebugUtilsMessageTypeFlagsEXT,
                       const VkDebugUtilsMessengerCallbackDataEXT*,
                       void* pUserData);
#endif

enum class FeatureSet
{
    coreOnly,
    allAvailable,
};

// Select a GPU, using 'gpuNameFilter' or 'getenv("RIVE_GPU")', otherwise
// preferring discrete. Abort if the filter matches more than one name.
std::tuple<vkb::Device, rive::gpu::VulkanFeatures> select_device(
    vkb::PhysicalDeviceSelector& selector,
    FeatureSet,
    const char* gpuNameFilter = nullptr);

inline std::tuple<vkb::Device, rive::gpu::VulkanFeatures> select_device(
    vkb::Instance instance,
    FeatureSet featureSet,
    const char* gpuNameFilter = nullptr)
{
    vkb::PhysicalDeviceSelector selector(instance);
    return select_device(selector, featureSet, gpuNameFilter);
}

struct SwapchainImage
{
    VkImage externalImage;
    // TODO: Once the client is fully responsible for synchronization, this
    // should turn into just a VkImageView.
    rive::rcp<rive::gpu::vkutil::TextureView> imageView;
    VkFence fence;
    VkSemaphore frameBeginSemaphore;
    VkSemaphore frameCompleteSemaphore;
    VkCommandBuffer commandBuffer;
    // Resource lifetime counters. Resources last used on or before
    // 'safeFrameNumber' are safe to be released or recycled.
    uint64_t currentFrameNumber = 0;
    uint64_t safeFrameNumber = 0;
};

class Swapchain
{
public:
    // Vulkan native swapchain.
    Swapchain(const vkb::Device&,
              rive::rcp<rive::gpu::VulkanContext>,
              uint32_t width,
              uint32_t height,
              vkb::Swapchain&&,
              uint64_t currentFrameNumber = 0);

    // Offscreen texture.
    Swapchain(const vkb::Device&,
              rive::rcp<rive::gpu::VulkanContext>,
              uint32_t width,
              uint32_t height,
              VkFormat imageFormat,
              VkImageUsageFlags additionalUsageFlags =
                  VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
              uint64_t currentFrameNumber = 0);

    ~Swapchain();

    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }
    VkFormat imageFormat() const { return m_imageFormat; }
    const vkb::DispatchTable& dispatchTable() const { return m_dispatchTable; }
    uint64_t currentFrameNumber() const { return m_currentFrameNumber; }

    const SwapchainImage* acquireNextImage();
    const SwapchainImage* currentImage() const
    {
        return m_currentImageIndex == INVALID_IMAGE_INDEX
                   ? nullptr
                   : &m_swapchainImages[m_currentImageIndex];
    }

    // Submits and presents the current swapchain image.
    // 'lastAccess' lets us know know how to barrier the swapchain image.
    // 'pixelData', if not null, reads the swapchain image being presented.
    rive::gpu::VulkanContext::TextureAccess submit(
        rive::gpu::VulkanContext::TextureAccess lastAccess,
        std::vector<uint8_t>* pixelData = nullptr);

private:
    constexpr static uint32_t INVALID_IMAGE_INDEX = -1;

    void init(const vkb::Device&, const std::vector<VkImage>& images);

    const vkb::DispatchTable m_dispatchTable;
    const VkQueue m_queue;
    const rive::rcp<rive::gpu::VulkanContext> m_vk;
    const uint32_t m_width;
    const uint32_t m_height;
    const VkFormat m_imageFormat;
    const VkImageUsageFlags m_imageUsageFlags;
    const rive::rcp<rive::gpu::vkutil::Texture> m_offscreenTexture;
    vkb::Swapchain m_vkbSwapchain;
    VkCommandPool m_commandPool;
    VkSemaphore m_nextAcquireSemaphore;
    std::vector<SwapchainImage> m_swapchainImages;
    uint32_t m_currentImageIndex = INVALID_IMAGE_INDEX;
    uint64_t m_currentFrameNumber = 0;
    rive::rcp<rive::gpu::vkutil::Buffer> m_pixelReadBuffer;
};

} // namespace rive_vkb
