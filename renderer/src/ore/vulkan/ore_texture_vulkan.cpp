/*
 * Copyright 2025 Rive
 */

#include "ore_texture_vulkan.hpp"
#include "rive/renderer/ore/ore_context_vulkan.hpp"

#include <vk_mem_alloc.h>

#include <cassert>

namespace rive::ore
{

// ============================================================================
// Helpers
// ============================================================================

static bool isDepthStencilFormat(TextureFormat fmt)
{
    switch (fmt)
    {
        case TextureFormat::depth16unorm:
        case TextureFormat::depth24plusStencil8:
        case TextureFormat::depth32float:
        case TextureFormat::depth32floatStencil8:
            return true;
        default:
            return false;
    }
}

static bool hasStencil(TextureFormat fmt)
{
    return fmt == TextureFormat::depth24plusStencil8 ||
           fmt == TextureFormat::depth32floatStencil8;
}

static VkImageAspectFlags aspectMask(TextureFormat fmt)
{
    if (isDepthStencilFormat(fmt))
    {
        VkImageAspectFlags flags = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (hasStencil(fmt))
            flags |= VK_IMAGE_ASPECT_STENCIL_BIT;
        return flags;
    }
    return VK_IMAGE_ASPECT_COLOR_BIT;
}

// Transition an image from one layout to another using a pipeline barrier
// on the given command buffer.
static void transitionLayout(PFN_vkCmdPipelineBarrier pfnCmdPipelineBarrier,
                             VkCommandBuffer cmdBuf,
                             VkImage image,
                             VkImageAspectFlags aspectMask,
                             VkImageLayout oldLayout,
                             VkImageLayout newLayout,
                             uint32_t mipCount = 1,
                             uint32_t layerCount = 1)
{
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspectMask;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mipCount;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = layerCount;

    VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
        newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
             newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
             newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    }
    else
    {
        // Generic fallback — conservative but correct.
        barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
        barrier.dstAccessMask =
            VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        dstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    }

    pfnCmdPipelineBarrier(cmdBuf,
                          srcStage,
                          dstStage,
                          0,
                          0,
                          nullptr,
                          0,
                          nullptr,
                          1,
                          &barrier);
}

// ============================================================================
// Texture
// ============================================================================

void TextureVulkan::upload(const TextureDataDesc& data)
{
    // Staging upload records into whichever command buffer the owning
    // Context currently has open — either Ore's own CB (owned-CB mode) or
    // the host's CB (external-CB mode). Resolved at call time so the same
    // Texture can span both modes across its lifetime.
    assert(m_vkOreContext != nullptr);
    VkCommandBuffer cmdBuf = m_vkOreContext->m_vkCommandBuffer;
    auto pfnCmdPipelineBarrier = m_vkOreContext->m_vk->CmdPipelineBarrier;
    auto pfnCmdCopyBufferToImage = m_vkOreContext->m_vk->CmdCopyBufferToImage;
    assert(cmdBuf != VK_NULL_HANDLE);
    assert(m_vkImage != VK_NULL_HANDLE);

    // Determine byte size of the upload region.
    uint32_t bytesPerRow = data.bytesPerRow;
    uint32_t height = data.height > 0 ? data.height : m_height;
    uint32_t depth = data.depth > 0 ? data.depth : m_depthOrArrayLayers;
    VkDeviceSize uploadSize =
        static_cast<VkDeviceSize>(bytesPerRow) * height * depth;

    // Create a transient host-visible staging buffer.
    VkBufferCreateInfo bufCI{};
    bufCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufCI.size = uploadSize;
    bufCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo allocCI{};
    allocCI.usage = VMA_MEMORY_USAGE_CPU_ONLY;
    allocCI.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocationInfo allocInfo{};
    VkBuffer stagingBuf = VK_NULL_HANDLE;
    VmaAllocation stagingAlloc = VK_NULL_HANDLE;
    vmaCreateBuffer(m_vmaAllocator,
                    &bufCI,
                    &allocCI,
                    &stagingBuf,
                    &stagingAlloc,
                    &allocInfo);

    memcpy(allocInfo.pMappedData, data.data, static_cast<size_t>(uploadSize));

    // Transition to TRANSFER_DST.
    transitionLayout(pfnCmdPipelineBarrier,
                     cmdBuf,
                     m_vkImage,
                     aspectMask(m_format),
                     m_vkLayout,
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                     m_numMipmaps,
                     m_depthOrArrayLayers);
    m_vkLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    // Copy staging buffer → image. `bufferRowLength` and
    // `bufferImageHeight` are in texels — 0 means "tightly packed at
    // imageExtent". When the caller's source has padding (e.g. a
    // sub-rect of a larger image, or `bytesPerRow > width *
    // bytesPerTexel`), the staging copy must honour that pitch or the
    // image comes back with the padding bytes interleaved into every
    // other row. Pre-fix this was hardcoded to 0; the
    // `ore_array_upload` GM (Phase 4 witness) caught the regression
    // on every Android Vulkan target.
    const uint32_t bptVK = textureFormatBytesPerTexel(m_format);
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength =
        (data.bytesPerRow != 0 && bptVK != 0 && (data.bytesPerRow % bptVK) == 0)
            ? data.bytesPerRow / bptVK
            : 0;
    region.bufferImageHeight = data.rowsPerImage;
    region.imageSubresource.aspectMask = aspectMask(m_format);
    region.imageSubresource.mipLevel = data.mipLevel;
    region.imageSubresource.baseArrayLayer = data.layer;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {static_cast<int32_t>(data.x),
                          static_cast<int32_t>(data.y),
                          static_cast<int32_t>(data.z)};
    region.imageExtent = {data.width > 0 ? data.width : m_width, height, depth};

    pfnCmdCopyBufferToImage(cmdBuf,
                            stagingBuf,
                            m_vkImage,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            1,
                            &region);

    // Transition back to SHADER_READ_ONLY.
    transitionLayout(pfnCmdPipelineBarrier,
                     cmdBuf,
                     m_vkImage,
                     aspectMask(m_format),
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                     m_numMipmaps,
                     m_depthOrArrayLayers);
    m_vkLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // The staging buffer must stay alive until the frame command buffer has
    // been submitted and the GPU is done with it.  Defer destruction to
    // Context::endFrame(), which submits the command buffer and waits on the
    // fence before cleaning up.
    m_vkOreContext->m_vkDeferredStagingBuffers.push_back(
        {stagingBuf, stagingAlloc});
}

void TextureVulkan::onRefCntReachedZero() const
{
    // Only destroy VMA-owned images (borrowed textures have
    // m_vmaAllocation==null).
    VmaAllocator allocator = m_vmaAllocator;
    VkImage image = m_vkImage;
    VmaAllocation alloc = m_vmaAllocation;
    ContextVulkan* ctx = m_vkOreContext;

    auto destroy = [=]() {
        if (image != VK_NULL_HANDLE && alloc != VK_NULL_HANDLE)
            vmaDestroyImage(allocator, image, alloc);
    };

    delete this;

    if (ctx != nullptr)
        ctx->vkDeferDestroy(std::move(destroy));
    else
        destroy();
}

// ============================================================================
// TextureView
// ============================================================================

void TextureViewVulkan::onRefCntReachedZero() const
{
    VkDevice dev = m_vkDevice;
    VkImageView view = m_vkImageView;
    auto destroyFn = m_vkDestroyImageView;
    ContextVulkan* ctx = m_vkOreContext;

    auto destroy = [=]() {
        if (view != VK_NULL_HANDLE && destroyFn != nullptr)
            destroyFn(dev, view, nullptr);
    };

    delete this;

    if (ctx != nullptr)
        ctx->vkDeferDestroy(std::move(destroy));
    else
        destroy();
}

} // namespace rive::ore
