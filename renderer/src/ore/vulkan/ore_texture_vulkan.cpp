/*
 * Copyright 2025 Rive
 */

#include "ore_texture_vulkan.hpp"
#include "ore_buffer_vulkan.hpp"
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

// ============================================================================
// Texture
// ============================================================================

void TextureVulkan::upload(const TextureDataDesc& data)
{
    // Stage CPU-side, queue for the next host CB. Callers may run with
    // no recording CB (verify hooks, scripted shader setup).
    assert(m_vkOreContext != nullptr);
    assert(m_vkImage != VK_NULL_HANDLE);
    if (data.data == nullptr)
    {
        m_vkOreContext->setLastError("upload: data is null");
        return;
    }

    const uint32_t bptVK = textureFormatBytesPerTexel(m_format);
    // mipLevel must index a declared level.
    if (data.mipLevel >= m_numMipmaps)
    {
        m_vkOreContext->setLastError("upload: mipLevel (%u) >= numMipmaps (%u)",
                                     data.mipLevel,
                                     m_numMipmaps);
        return;
    }
    // layer must index a declared slice (1 for non-array/non-cube).
    if (data.layer >= m_depthOrArrayLayers)
    {
        m_vkOreContext->setLastError(
            "upload: layer (%u) >= depthOrArrayLayers (%u)",
            data.layer,
            m_depthOrArrayLayers);
        return;
    }
    // Mip-adjusted extents (Vulkan-spec floor(max(1, dim >> mipLevel))).
    const uint32_t mipWidth =
        (m_width >> data.mipLevel) > 0 ? (m_width >> data.mipLevel) : 1u;
    const uint32_t mipHeight =
        (m_height >> data.mipLevel) > 0 ? (m_height >> data.mipLevel) : 1u;
    const uint32_t width = data.width > 0 ? data.width : mipWidth;
    const uint32_t height = data.height > 0 ? data.height : mipHeight;
    // imageExtent.depth is the copy's z-extent. The array slice is chosen via
    // baseArrayLayer with layerCount 1, so only true 3D textures carry depth
    // greater than 1. Defaulting to m_depthOrArrayLayers would set depth to the
    // array count for array2D/cube and over-read the staging source.
    const uint32_t maxDepth =
        m_type == TextureType::texture3D ? m_depthOrArrayLayers : 1u;
    const uint32_t depth = data.depth > 0 ? data.depth : maxDepth;
    // Region must fit within the mip's extent. 64-bit so a large x/y offset
    // can't wrap past the guard.
    if (static_cast<uint64_t>(data.x) + width > mipWidth ||
        static_cast<uint64_t>(data.y) + height > mipHeight)
    {
        m_vkOreContext->setLastError(
            "upload: region (x=%u y=%u w=%u h=%u) out of bounds for "
            "mip %u (%ux%u)",
            data.x,
            data.y,
            width,
            height,
            data.mipLevel,
            mipWidth,
            mipHeight);
        return;
    }
    // z-slice must fit the texture depth (1 for everything but 3D).
    if (static_cast<uint64_t>(data.z) + depth > maxDepth)
    {
        m_vkOreContext->setLastError(
            "upload: z-region (z=%u depth=%u) out of bounds (maxDepth=%u)",
            data.z,
            depth,
            maxDepth);
        return;
    }
    // bytesPerTexel == 0 means a block-compressed format. We don't have
    // a block-size-aware path for bufferRowLength yet, so reject upfront.
    if (bptVK == 0)
    {
        m_vkOreContext->setLastError(
            "upload: block-compressed formats not yet supported");
        return;
    }
    // Uncompressed: bytesPerRow must be a whole number of texels and
    // cover at least width texels so bufferRowLength can encode the
    // caller pitch and the GPU read stays inside the staging buffer.
    if (data.bytesPerRow != 0 && (data.bytesPerRow % bptVK) != 0)
    {
        m_vkOreContext->setLastError(
            "upload: bytesPerRow (%u) must be a whole number of texels "
            "(bytesPerTexel=%u)",
            data.bytesPerRow,
            bptVK);
        return;
    }
    if (data.bytesPerRow != 0 &&
        data.bytesPerRow < static_cast<uint64_t>(width) * bptVK)
    {
        m_vkOreContext->setLastError(
            "upload: bytesPerRow (%u) < width * bytesPerTexel (%llu)",
            data.bytesPerRow,
            static_cast<unsigned long long>(static_cast<uint64_t>(width) *
                                            bptVK));
        return;
    }
    // rowsPerImage is the per-slice stride; 0 means "use height", otherwise
    // it must cover at least height rows.
    if (data.rowsPerImage > 0 && data.rowsPerImage < height)
    {
        m_vkOreContext->setLastError("upload: rowsPerImage (%u) < height (%u)",
                                     data.rowsPerImage,
                                     height);
        return;
    }
    // Derive row/total sizes in 64-bit so the tightly-packed fallback
    // (width * bptVK) and the total can't wrap before the uint32_t guard.
    // bytesPerRow == 0 means tightly packed.
    const uint64_t bytesPerRow64 = data.bytesPerRow != 0
                                       ? static_cast<uint64_t>(data.bytesPerRow)
                                       : static_cast<uint64_t>(width) * bptVK;
    const uint32_t rowsPerImage =
        data.rowsPerImage > 0 ? data.rowsPerImage : height;
    const VkDeviceSize uploadSize = bytesPerRow64 *
                                    static_cast<uint64_t>(rowsPerImage) *
                                    static_cast<uint64_t>(depth);
    // BufferVulkan size is uint32_t; refuse larger uploads rather than
    // silently truncating the staging copy.
    if (uploadSize > UINT32_MAX)
    {
        m_vkOreContext->setLastError(
            "upload: size (%llu) exceeds uint32_t staging buffer max",
            static_cast<unsigned long long>(uploadSize));
        return;
    }

    auto stagingBuffer =
        rcp<BufferVulkan>(new BufferVulkan(m_manager,
                                           static_cast<uint32_t>(uploadSize),
                                           BufferUsage::upload));
    stagingBuffer->m_vk = m_vk;

    VkBufferCreateInfo bufCI{};
    bufCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufCI.size = uploadSize;
    bufCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo allocCI{};
    allocCI.usage = VMA_MEMORY_USAGE_CPU_ONLY;
    allocCI.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocationInfo allocInfo{};
    VkResult vmaRes = vmaCreateBuffer(m_vk->allocator(),
                                      &bufCI,
                                      &allocCI,
                                      &stagingBuffer->m_vkBuffer,
                                      &stagingBuffer->m_vmaAllocation,
                                      &allocInfo);
    if (vmaRes != VK_SUCCESS || allocInfo.pMappedData == nullptr)
    {
        m_vkOreContext->setLastError(
            "upload: staging buffer allocation failed (size=%llu, vk=%d)",
            static_cast<unsigned long long>(uploadSize),
            static_cast<int>(vmaRes));
        return;
    }
    stagingBuffer->m_vkMappedPtr = allocInfo.pMappedData;

    stagingBuffer->update(data.data, static_cast<uint32_t>(uploadSize), 0);

    // bufferRowLength / bufferImageHeight are in texels; 0 means tightly
    // packed at imageExtent. Honour caller pitch when present. Upstream
    // guards make the div-by-bptVK safe. The `ore_array_upload` GM locks
    // this — without it the GL backend silently strides wrong.
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength =
        data.bytesPerRow != 0 ? data.bytesPerRow / bptVK : 0;
    region.bufferImageHeight = data.rowsPerImage;
    region.imageSubresource.aspectMask = aspectMask(m_format);
    region.imageSubresource.mipLevel = data.mipLevel;
    region.imageSubresource.baseArrayLayer = data.layer;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {static_cast<int32_t>(data.x),
                          static_cast<int32_t>(data.y),
                          static_cast<int32_t>(data.z)};
    region.imageExtent = {width, height, depth};

    m_vkOreContext->vkQueuePendingTextureUpload({
        ref_rcp(this),
        std::move(stagingBuffer),
        region,
        aspectMask(m_format),
    });
}

TextureVulkan::~TextureVulkan()
{
    // Only destroy VMA-owned images (borrowed textures have
    // m_vmaAllocation==null).
    if (m_vkImage != VK_NULL_HANDLE && m_vmaAllocation != VK_NULL_HANDLE)
        vmaDestroyImage(m_vk->allocator(), m_vkImage, m_vmaAllocation);
}

// ============================================================================
// TextureView
// ============================================================================

TextureViewVulkan::~TextureViewVulkan()
{
    if (m_vkImageView != VK_NULL_HANDLE && m_vkDestroyImageView != nullptr)
        m_vkDestroyImageView(m_vkDevice, m_vkImageView, nullptr);
}

} // namespace rive::ore
