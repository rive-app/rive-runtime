/*
 * Copyright 2025 Rive
 */

#include <vulkan/vulkan.h>
#include <optional>
#include <vector>

#include "rive/renderer/vulkan/vulkan_context.hpp"

namespace rive_vkb
{
class VulkanDevice;
class VulkanInstance;

class VulkanFrameSynchronizer
{
public:
    ~VulkanFrameSynchronizer();

    VulkanFrameSynchronizer(const VulkanFrameSynchronizer&) = delete;
    VulkanFrameSynchronizer& operator=(const VulkanFrameSynchronizer&) = delete;
    VkCommandBuffer currentCommandBuffer() const
    {
        return current().commandBuffer;
    }
    uint64_t safeFrameNumber() const { return current().safeFrameNumber; }
    uint64_t currentFrameNumber() const { return m_monotonicFrameNumber; }

    // Queue a copy of the specified image with optional bounds. Must be done
    // before endFrame.
    void queueImageCopy(VkImage,
                        VkFormat,
                        rive::gpu::vkutil::ImageAccess* inOutLastAccess,
                        rive::IAABB pixelReadBounds);

    // This gets the pixels from the last image copy requested. Must be called
    // after the endFrame of the frame the request occurred during.
    [[nodiscard]] VkResult getPixelsFromLastImageCopy(
        std::vector<uint8_t>* outPixels);

protected:
    struct Options
    {
        uint64_t initialFrameNumber = 0;

        // In effectively all cases it's best to have 2 in-flight frames (rather
        // than one per swapchain image), to keep latency down.
        uint32_t inFlightFrameCount = 2;

        // This should be true for swapchains, since the acquire/present calls
        // need to use semaphores
        bool externalGPUSynchronization = false;
    };

    struct InFlightFrame
    {
        VkFence fence = VK_NULL_HANDLE;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkSemaphore semaphore = VK_NULL_HANDLE;
        uint64_t safeFrameNumber = 0;
    };

    VulkanFrameSynchronizer(VulkanInstance&,
                            VulkanDevice&,
                            rive::rcp<rive::gpu::VulkanContext>&&,
                            const Options&,
                            bool* successOut);

    VkDevice vkDevice() const { return m_device; }

    [[nodiscard]] VkResult waitForFenceAndBeginFrame(VkSemaphore* = nullptr);
    [[nodiscard]] VkResult endFrame(
        std::optional<VkSemaphore> externalSignalSemaphore = std::nullopt);

    const InFlightFrame& current() const
    {
        return m_inFlightFrames[m_renderFrameIndex];
    }
    InFlightFrame& current() { return m_inFlightFrames[m_renderFrameIndex]; }

    // Get the previous in-flight frame
    InFlightFrame& prev()
    {
        return m_inFlightFrames[(m_renderFrameIndex + m_inFlightFrames.size() -
                                 1) %
                                m_inFlightFrames.size()];
    }

    rive::gpu::VulkanContext* context() const { return m_vk.get(); }

    VkQueue graphicsQueue() const { return m_graphicsQueue; }

    VkResult createSemaphore(VkSemaphore*);
    void destroySemaphore(VkSemaphore);

private:
    rive::rcp<rive::gpu::VulkanContext> m_vk;
    VkDevice m_device;

    enum class PixelReadState
    {
        None,
        Queued,
        Ready,
    };

    rive::rcp<rive::gpu::vkutil::Buffer> m_pixelReadBuffer;
    uint32_t m_pixelReadWidth = 0;
    uint32_t m_pixelReadHeight = 0;
    VkFormat m_pixelReadFormat = VK_FORMAT_UNDEFINED;
    PixelReadState m_pixelReadState = PixelReadState::None;

    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;

    uint64_t m_monotonicFrameNumber = 0;
    uint32_t m_renderFrameIndex = 0;

    // Best practice, regardless of how many swapchain images there are, is to
    // have exactly two sets of frame objects, one for the previously-completed
    // frame, and one for the currently-building frame.
    std::vector<InFlightFrame> m_inFlightFrames;

    // These are all the commands the swapchain needs to do its work - this
    // macro is also used to load them in the .cpp
#define RIVE_VK_FRAME_SYNC_INSTANCE_COMMANDS(F)                                \
    F(vkCreateCommandPool)                                                     \
    F(vkDestroyCommandPool)                                                    \
    F(vkCreateFence)                                                           \
    F(vkDestroyFence)                                                          \
    F(vkCreateSemaphore)                                                       \
    F(vkDestroySemaphore)                                                      \
    F(vkAllocateCommandBuffers)                                                \
    F(vkFreeCommandBuffers)                                                    \
    F(vkWaitForFences)                                                         \
    F(vkCmdPipelineBarrier)                                                    \
    F(vkQueueSubmit)                                                           \
    F(vkGetDeviceQueue)                                                        \
    F(vkResetFences)                                                           \
    F(vkResetCommandBuffer)                                                    \
    F(vkBeginCommandBuffer)                                                    \
    F(vkEndCommandBuffer)                                                      \
    F(vkCmdCopyImageToBuffer)

#define DECLARE_VULKAN_COMMAND(name) PFN_##name m_##name = nullptr;
    RIVE_VK_FRAME_SYNC_INSTANCE_COMMANDS(DECLARE_VULKAN_COMMAND)
#undef DECLARE_VULKAN_COMMAND
};
} // namespace rive_vkb
