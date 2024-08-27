/*
 * Copyright 2023 Rive
 */

#include "rive/renderer/vulkan/vkutil.hpp"

#include "rive/rive_types.hpp"
#include "rive/renderer/vulkan/vulkan_context.hpp"

namespace rive::gpu::vkutil
{
void vkutil::RenderingResource::onRefCntReachedZero() const
{
    // VulkanContext will hold off on deleting "this" until any in-flight command
    // buffers have finished (potentially) referencing our underlying Vulkan
    // objects.
    m_vk->onRenderingResourceReleased(this);
}

Buffer::Buffer(rcp<VulkanContext> vk, const VkBufferCreateInfo& info, Mappability mappability) :
    RenderingResource(std::move(vk)), m_mappability(mappability), m_info(info)
{
    m_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    init();
}

Buffer::~Buffer() { resizeImmediately(0); }

void Buffer::resizeImmediately(size_t sizeInBytes)
{
    if (m_info.size != sizeInBytes)
    {
        if (m_vmaAllocation != VK_NULL_HANDLE)
        {
            vmaUnmapMemory(m_vk->vmaAllocator, m_vmaAllocation);
            vmaDestroyBuffer(m_vk->vmaAllocator, m_vkBuffer, m_vmaAllocation);
        }
        m_info.size = sizeInBytes;
        init();
    }
}

VmaAllocationCreateFlags vma_flags_for_mappability(Mappability mappability)
{
    switch (mappability)
    {
        case Mappability::none:
            return 0;
        case Mappability::writeOnly:
            return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        case Mappability::readWrite:
            return VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    }
    RIVE_UNREACHABLE();
}

void Buffer::init()
{
    if (m_info.size > 0)
    {
        VmaAllocationCreateInfo allocInfo = {
            .flags = vma_flags_for_mappability(m_mappability),
            .usage = VMA_MEMORY_USAGE_AUTO,
        };

        VK_CHECK(vmaCreateBuffer(m_vk->vmaAllocator,
                                 &m_info,
                                 &allocInfo,
                                 &m_vkBuffer,
                                 &m_vmaAllocation,
                                 nullptr));

        // Leave the buffer constantly mapped and let the OS/drivers handle the
        // rest.
        VK_CHECK(vmaMapMemory(m_vk->vmaAllocator, m_vmaAllocation, &m_contents));
    }
    else
    {
        m_vkBuffer = VK_NULL_HANDLE;
        m_vmaAllocation = VK_NULL_HANDLE;
        m_contents = nullptr;
    }
}

void Buffer::flushMappedContents(size_t updatedSizeInBytes)
{
    // Leave the buffer constantly mapped and let the OS/drivers handle the
    // rest.
    vmaFlushAllocation(m_vk->vmaAllocator, m_vmaAllocation, 0, updatedSizeInBytes);
}

BufferRing::BufferRing(rcp<VulkanContext> vk,
                       VkBufferUsageFlags usage,
                       Mappability mappability,
                       size_t size) :
    m_targetSize(size)
{
    VkBufferCreateInfo bufferCreateInfo = {
        .size = size,
        .usage = usage,
    };
    for (int i = 0; i < gpu::kBufferRingSize; ++i)
    {
        m_buffers[i] = vk->makeBuffer(bufferCreateInfo, mappability);
    }
}

void BufferRing::setTargetSize(size_t size)
{
    // Buffers always get bound, even if unused, so make sure they aren't empty
    // and we get a valid Vulkan handle.
    if (m_buffers[0]->info().usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
    {
        size = std::max<size_t>(size, 256);
        // Uniform blocks must be multiples of 256 bytes in size.
        assert(size % 256 == 0);
    }
    else
    {
        size = std::max<size_t>(size, 1);
    }
    m_targetSize = size;
}

void BufferRing::synchronizeSizeAt(int bufferRingIdx)
{
    if (m_buffers[bufferRingIdx]->info().size != m_targetSize)
    {
        m_buffers[bufferRingIdx]->resizeImmediately(m_targetSize);
    }
}

void* BufferRing::contentsAt(int bufferRingIdx, size_t dirtySize)
{
    m_pendingFlushSize = dirtySize;
    return m_buffers[bufferRingIdx]->contents();
}

void BufferRing::flushMappedContentsAt(int bufferRingIdx)
{
    assert(m_pendingFlushSize > 0);
    m_buffers[bufferRingIdx]->flushMappedContents(m_pendingFlushSize);
    m_pendingFlushSize = 0;
}

Texture::Texture(rcp<VulkanContext> vk, const VkImageCreateInfo& info) :
    RenderingResource(std::move(vk)), m_info(info)
{
    m_info = info;
    m_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;

    if (m_info.imageType == 0)
    {
        m_info.imageType = VK_IMAGE_TYPE_2D;
    }

    if (m_info.mipLevels == 0)
    {
        m_info.mipLevels = 1;
    }
    else if (m_info.mipLevels > 1)
    {
        // We generate mipmaps internally with image blits.
        m_info.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    if (m_info.arrayLayers == 0)
    {
        m_info.arrayLayers = 1;
    }

    if (m_info.samples == 0)
    {
        m_info.samples = VK_SAMPLE_COUNT_1_BIT;
    }

    if (m_info.usage & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT)
    {
        // Attempt to create transient attachments with lazily-allocated memory.
        // This should succeed on mobile. Otherwise, use normal memory.
        VmaAllocationCreateInfo allocInfo = {
            .usage = VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED,
        };

        if (vmaCreateImage(m_vk->vmaAllocator,
                           &m_info,
                           &allocInfo,
                           &m_vkImage,
                           &m_vmaAllocation,
                           nullptr) == VK_SUCCESS)
        {
            printf("SUCCESS AT TRANSIENT LAZY!\n");
            return;
        }
    }

    VmaAllocationCreateInfo allocInfo = {
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    VK_CHECK(vmaCreateImage(m_vk->vmaAllocator,
                            &m_info,
                            &allocInfo,
                            &m_vkImage,
                            &m_vmaAllocation,
                            nullptr));
}

Texture::~Texture()
{
    if (m_vmaAllocation != VK_NULL_HANDLE)
    {
        vmaDestroyImage(m_vk->vmaAllocator, m_vkImage, m_vmaAllocation);
    }
}

TextureView::TextureView(rcp<VulkanContext> vk,
                         rcp<Texture> textureRef,
                         VkImageUsageFlags flags,
                         const VkImageViewCreateInfo& info) :
    RenderingResource(std::move(vk)),
    m_textureRefOrNull(std::move(textureRef)),
    m_usageFlags(flags),
    m_info(info)
{
    m_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    VK_CHECK(m_vk->CreateImageView(m_vk->device, &m_info, nullptr, &m_vkImageView));
}

TextureView::~TextureView() { m_vk->DestroyImageView(m_vk->device, m_vkImageView, nullptr); }

Framebuffer::Framebuffer(rcp<VulkanContext> vk, const VkFramebufferCreateInfo& info) :
    RenderingResource(std::move(vk)), m_info(info)
{
    m_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    VK_CHECK(m_vk->CreateFramebuffer(m_vk->device, &m_info, nullptr, &m_vkFramebuffer));
}

Framebuffer::~Framebuffer() { m_vk->DestroyFramebuffer(m_vk->device, m_vkFramebuffer, nullptr); }
} // namespace rive::gpu::vkutil
