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

bool TextureVulkan::vkMarkWritten(uint32_t mip, uint32_t layer)
{
    size_t i = static_cast<size_t>(layer) * m_numMipmaps + mip;
    if (i >= m_vkWritten.size())
    {
        m_vkWritten.resize(i + 1, false);
    }
    bool wasWritten = m_vkWritten[i];
    m_vkWritten[i] = true;
    return wasWritten;
}

void TextureVulkan::uploadImpl(const TextureDataDesc& data)
{
    // Stage CPU-side, queue for the next host CB. Callers may run with
    // no recording CB (verify hooks, scripted shader setup).
    assert(m_vkOreContext != nullptr);
    assert(m_vkImage != VK_NULL_HANDLE);
    const uint32_t bptVK = textureFormatBytesPerTexel(m_format);
    // No block-size-aware path for bufferRowLength yet.
    if (bptVK == 0)
    {
        m_vkOreContext->setLastError(
            "upload: block-compressed formats not yet supported");
        return;
    }
    const VkDeviceSize uploadSize = static_cast<uint64_t>(data.bytesPerRow) *
                                    data.rowsPerImage * data.depth;
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

    // bufferRowLength and bufferImageHeight are in texels.
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = data.bytesPerRow / bptVK;
    region.bufferImageHeight = data.rowsPerImage;
    region.imageSubresource.aspectMask = aspectMask(m_format);
    region.imageSubresource.mipLevel = data.mipLevel;
    region.imageSubresource.baseArrayLayer = data.layer;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {static_cast<int32_t>(data.x),
                          static_cast<int32_t>(data.y),
                          static_cast<int32_t>(data.z)};
    region.imageExtent = {data.width, data.height, data.depth};

    vkMarkWritten(data.mipLevel, data.layer);
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
