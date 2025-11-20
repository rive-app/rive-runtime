/*
 * Copyright 2025 Rive
 */

#include "rive_vk_bootstrap/vulkan_device.hpp"
#include "rive_vk_bootstrap/vulkan_instance.hpp"
#include "rive_vk_bootstrap/vulkan_frame_synchronizer.hpp"
#include "logging.hpp"
#include "vulkan_library.hpp"

namespace rive_vkb
{
VulkanFrameSynchronizer::VulkanFrameSynchronizer(
    VulkanInstance& instance,
    VulkanDevice& device,
    rive::rcp<rive::gpu::VulkanContext> vk,
    const Options& opts) :

    m_vk(std::move(vk)),
    m_device(device.vkDevice()),
    m_monotonicFrameNumber(opts.initialFrameNumber)
{
    // Load all of the functions we care about
#define LOAD(name) LOAD_REQUIRED_MEMBER_INSTANCE_FUNC(name, instance);
    RIVE_VK_FRAME_SYNC_INSTANCE_COMMANDS(LOAD);
#undef LOAD

    m_vkGetDeviceQueue(m_device,
                       device.graphicsQueueFamilyIndex(),
                       0,
                       &m_graphicsQueue);

    // Create the command pool
    VkCommandPoolCreateInfo commandPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = device.graphicsQueueFamilyIndex(),
    };

    VK_CHECK(m_vkCreateCommandPool(m_device,
                                   &commandPoolCreateInfo,
                                   nullptr,
                                   &m_commandPool));

    // Create the alternating-frame sync objects
    assert(opts.inFlightFrameCount > 1);
    m_inFlightFrames.resize(opts.inFlightFrameCount);
    for (auto& sync : m_inFlightFrames)
    {
        static constexpr VkFenceCreateInfo fenceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };

        VK_CHECK(
            m_vkCreateFence(m_device, &fenceCreateInfo, nullptr, &sync.fence));

        VkCommandBufferAllocateInfo cbufferAllocateInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = m_commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };

        VK_CHECK(m_vkAllocateCommandBuffers(m_device,
                                            &cbufferAllocateInfo,
                                            &sync.commandBuffer));
        sync.semaphore = createSemaphore();

        sync.safeFrameNumber = m_monotonicFrameNumber;
    }

    if (!opts.externalGPUSynchronization)
    {
        // Without external GPU synchronization we need the to explicitly signal
        // the very last semaphore in the chain so that the first frame has
        // something correct to wait on.

        VkSubmitInfo submitInfo = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &prev().semaphore,
        };

        m_vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    }
}

VulkanFrameSynchronizer::~VulkanFrameSynchronizer()
{
    // Note that the derived class will have already waited for the device to be
    // idle so we can safely destroy things here.
    for (auto& frame : m_inFlightFrames)
    {
        destroySemaphore(frame.semaphore);
        m_vkFreeCommandBuffers(m_device,
                               m_commandPool,
                               1,
                               &frame.commandBuffer);
        m_vkDestroyFence(m_device, frame.fence, nullptr);
    }

    m_vkDestroyCommandPool(m_device, m_commandPool, nullptr);
}

VkSemaphore VulkanFrameSynchronizer::waitForFenceAndBeginFrame()
{
    // Before we can use the command buffers/semaphores for the current frame,
    // we need to wait on its fence to stall the CPU until it's ready.
    static constexpr auto NO_TIMEOUT = std::numeric_limits<uint64_t>::max();
    VK_CHECK(
        m_vkWaitForFences(m_device, 1, &current().fence, true, NO_TIMEOUT));

    // Now we need to reset the command buffer
    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    };

    VK_CHECK(m_vkResetCommandBuffer(current().commandBuffer, 0));
    VK_CHECK(m_vkBeginCommandBuffer(current().commandBuffer, &beginInfo));
    m_monotonicFrameNumber++;

    // Return the semaphore that external GPU synchronization (i.e. a swapchain)
    // will signal.
    return current().semaphore;
}

void VulkanFrameSynchronizer::endFrame(
    std::optional<VkSemaphore> externalSignalSemaphore)
{
    auto& frame = current();

    // This frame is done - reset the fence so that the submit can signal it.
    VK_CHECK(m_vkResetFences(m_device, 1, &frame.fence));

    // Next, the command buffer needs to be ended (so it can be submitted)
    m_vkEndCommandBuffer(frame.commandBuffer);

    VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

    // If we were provided an external semaphore to signal (i.e. from a
    // swapchain), we'll wait on the current frame's semaphore and signal the
    // external one, otherwise we'll wait for the previous frame's semaphore and
    // signal the current one.
    auto waitSemaphore = externalSignalSemaphore.has_value() ? frame.semaphore
                                                             : prev().semaphore;
    auto signalSemaphore = externalSignalSemaphore.has_value()
                               ? externalSignalSemaphore.value()
                               : frame.semaphore;

    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &waitSemaphore,
        .pWaitDstStageMask = &waitStageMask,
        .commandBufferCount = 1,
        .pCommandBuffers = &frame.commandBuffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &signalSemaphore,
    };

    VK_CHECK(m_vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, frame.fence));

    // It will be safe to destroy assets in use in the current in-flight frame
    // when the current frame number finishes.
    frame.safeFrameNumber = m_monotonicFrameNumber;

    // Cycle the render index to the next.
    m_renderFrameIndex = (m_renderFrameIndex + 1) % m_inFlightFrames.size();

    if (m_pixelReadState == PixelReadState::Queued)
    {
        m_pixelReadState = PixelReadState::Ready;
    }
}

void VulkanFrameSynchronizer::queueImageCopy(
    VkImage image,
    VkFormat format,
    rive::gpu::vkutil::ImageAccess* inOutLastAccess,
    rive::IAABB pixelReadBounds)
{
    assert(m_pixelReadState == PixelReadState::None &&
           "Pixel read was while another is active.");
    VkDeviceSize requiredBufferSize =
        pixelReadBounds.height() * pixelReadBounds.width() * 4;

    // Ensure that we have a read buffer that can hold the amount of data we
    // need it to.
    if (m_pixelReadBuffer == nullptr ||
        m_pixelReadBuffer->info().size < requiredBufferSize)
    {
        m_pixelReadBuffer = m_vk->makeBuffer(
            {
                .size = requiredBufferSize,
                .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            },
            rive::gpu::vkutil::Mappability::readWrite);
    }

    constexpr rive::gpu::vkutil::ImageAccess TRANSFER_SRC_ACCESS = {
        .pipelineStages = VK_PIPELINE_STAGE_TRANSFER_BIT,
        .accessMask = VK_ACCESS_TRANSFER_READ_BIT,
        .layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    };

    // We need to move the image into a mode where it can be copied.
    auto& frame = current();
    *inOutLastAccess = m_vk->simpleImageMemoryBarrier(frame.commandBuffer,
                                                      *inOutLastAccess,
                                                      TRANSFER_SRC_ACCESS,
                                                      image);

    // Queue the actual copy
    VkBufferImageCopy copyDesc = {
        .imageSubresource =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        .imageOffset = {pixelReadBounds.left, pixelReadBounds.top, 0},
        .imageExtent = {uint32_t(pixelReadBounds.width()),
                        uint32_t(pixelReadBounds.height()),
                        1},
    };

    m_vkCmdCopyImageToBuffer(frame.commandBuffer,
                             image,
                             VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                             *m_pixelReadBuffer,
                             1,
                             &copyDesc);

    // Now transition the buffer's state so it will be readable by the CPU once
    // the operation is done.
    m_vk->bufferMemoryBarrier(frame.commandBuffer,
                              VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VK_PIPELINE_STAGE_HOST_BIT,
                              0,
                              {
                                  .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                                  .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
                                  .buffer = *m_pixelReadBuffer,
                              });

    m_pixelReadWidth = uint32_t(pixelReadBounds.width());
    m_pixelReadHeight = uint32_t(pixelReadBounds.height());
    m_pixelReadFormat = format;
    m_pixelReadState = PixelReadState::Queued;
}

void VulkanFrameSynchronizer::getPixelsFromLastImageCopy(
    std::vector<uint8_t>* outPixels)
{
    assert(m_pixelReadState != PixelReadState::None &&
           "Pixels from image copy requested without one submitted");
    assert(m_pixelReadState != PixelReadState::Queued &&
           "Pixels from image copy requested before endFrame was called");

    // In order to read this back, we need to wait for the previously-finished
    // frame to finish.
    auto& sync = prev();
    static constexpr auto NO_TIMEOUT = std::numeric_limits<uint64_t>::max();
    VK_CHECK(m_vkWaitForFences(m_device, 1, &sync.fence, true, NO_TIMEOUT));

    // Make the texture data available to the CPU
    m_pixelReadBuffer->invalidateContents();

    outPixels->resize(m_pixelReadWidth * m_pixelReadHeight * 4);
    assert(m_pixelReadBuffer->info().size >= outPixels->size());

    for (auto y = 0u; y < m_pixelReadHeight; y++)
    {
        // Copy the given row (the destination is flipped vertically vs the
        // source so read the source the other way around)
        auto src = static_cast<const uint8_t*>(m_pixelReadBuffer->contents()) +
                   m_pixelReadWidth * 4 * (m_pixelReadHeight - 1 - y);
        uint8_t* dst = &outPixels->at(y * m_pixelReadWidth * 4);
        memcpy(dst, src, m_pixelReadWidth * 4);

        if (m_pixelReadFormat == VK_FORMAT_B8G8R8A8_UNORM)
        {
            // Need to swap BGRA -> RGBA
            for (auto x = 0u; x < m_pixelReadWidth * 4; x += 4)
            {
                std::swap(dst[x], dst[x + 2]);
            }
        }
    }

    m_pixelReadState = PixelReadState::None;
}

VkSemaphore VulkanFrameSynchronizer::createSemaphore()
{
    static constexpr VkSemaphoreCreateInfo semaCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    VkSemaphore semaphore;
    VK_CHECK(
        m_vkCreateSemaphore(m_device, &semaCreateInfo, nullptr, &semaphore));
    return semaphore;
}

void VulkanFrameSynchronizer::destroySemaphore(VkSemaphore semaphore)
{
    m_vkDestroySemaphore(m_device, semaphore, nullptr);
}

} // namespace rive_vkb