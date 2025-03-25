/*
 * Copyright 2023 Rive
 */

#include "rive/renderer/vulkan/vkutil.hpp"

#include "rive/rive_types.hpp"
#include "rive/renderer/vulkan/vulkan_context.hpp"
#include <vk_mem_alloc.h>

namespace rive::gpu::vkutil
{
void vkutil::RenderingResource::onRefCntReachedZero() const
{
    // VulkanContext will hold off on deleting "this" until any in-flight
    // command buffers have finished (potentially) referencing our underlying
    // Vulkan objects.
    m_vk->onRenderingResourceReleased(this);
}

Buffer::Buffer(rcp<VulkanContext> vk,
               const VkBufferCreateInfo& info,
               Mappability mappability) :
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
            if (m_mappability != Mappability::none)
            {
                vmaUnmapMemory(m_vk->allocator(), m_vmaAllocation);
            }
            vmaDestroyBuffer(m_vk->allocator(), m_vkBuffer, m_vmaAllocation);
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

        VK_CHECK(vmaCreateBuffer(m_vk->allocator(),
                                 &m_info,
                                 &allocInfo,
                                 &m_vkBuffer,
                                 &m_vmaAllocation,
                                 nullptr));

        if (m_mappability != Mappability::none)
        {
            // Leave the buffer constantly mapped and let the OS/drivers handle
            // the rest.
            VK_CHECK(
                vmaMapMemory(m_vk->allocator(), m_vmaAllocation, &m_contents));
        }
        else
        {
            m_contents = nullptr;
        }
    }
    else
    {
        m_vkBuffer = VK_NULL_HANDLE;
        m_vmaAllocation = VK_NULL_HANDLE;
        m_contents = nullptr;
    }
}

void Buffer::flushContents(size_t updatedSizeInBytes)
{
    vmaFlushAllocation(m_vk->allocator(),
                       m_vmaAllocation,
                       0,
                       updatedSizeInBytes);
}

void Buffer::invalidateContents(size_t updatedSizeInBytes)
{
    vmaInvalidateAllocation(m_vk->allocator(),
                            m_vmaAllocation,
                            0,
                            updatedSizeInBytes);
}

void BufferPool::setTargetSize(size_t size)
{
    assert(m_currentBuffer == nullptr); // Call releaseCurrentBuffer() first.

    // Buffers always get bound, even if unused, so make sure they aren't empty
    // and we get a valid Vulkan handle.
    size = std::max<size_t>(size, 1);

    if (m_usageFlags & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
    {
        // Uniform blocks must be multiples of 256 bytes in size.
        size = std::max<size_t>(size, 256);
        assert(size % 256 == 0);
    }

    m_targetSize = size;
}

vkutil::Buffer* BufferPool::currentBuffer()
{
    if (m_currentBuffer == nullptr)
    {
        if (!m_pool.empty() &&
            m_pool.front().lastFrameNumber <= m_vk->safeFrameNumber())
        {
            // Recycle the oldest buffer in the pool.
            m_currentBuffer = std::move(m_pool.front().buffer);
            if (m_currentBuffer->info().size != m_targetSize)
            {
                m_currentBuffer->resizeImmediately(m_targetSize);
            }
            m_pool.pop_front();

            // Trim the pool in case it's grown out of control (meaning it was
            // advanced multiple times in a single frame).
            constexpr static size_t POOL_MAX_COUNT = 8;
            while (m_pool.size() > POOL_MAX_COUNT &&
                   m_pool.front().lastFrameNumber <= m_vk->safeFrameNumber())
            {
                m_pool.pop_front();
            }
        }
        else
        {
            // There wasn't a free buffer in the pool. Create a new one.
            m_currentBuffer =
                m_vk->makeBuffer({.size = m_targetSize, .usage = m_usageFlags},
                                 Mappability::writeOnly);
        }
    }
    m_currentBufferFrameNumber = m_vk->currentFrameNumber();
    return m_currentBuffer.get();
}

void* BufferPool::mapCurrentBuffer(size_t dirtySize)
{
    m_pendingFlushSize = dirtySize;
    return currentBuffer()->contents();
}

void BufferPool::unmapCurrentBuffer()
{
    assert(m_pendingFlushSize > 0);
    currentBuffer()->flushContents(m_pendingFlushSize);
    m_pendingFlushSize = 0;
}

void BufferPool::releaseCurrentBuffer()
{
    if (m_currentBuffer != nullptr)
    {
        // Return the current buffer to the pool.
        m_pool.emplace_back(std::move(m_currentBuffer),
                            m_currentBufferFrameNumber);
        assert(m_currentBuffer == nullptr);
    }

    // The current buffer's frameNumber will update when it gets accessed.
    m_currentBufferFrameNumber = 0;
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

        if (vmaCreateImage(m_vk->allocator(),
                           &m_info,
                           &allocInfo,
                           &m_vkImage,
                           &m_vmaAllocation,
                           nullptr) == VK_SUCCESS)
        {
            return;
        }
    }

    VmaAllocationCreateInfo allocInfo = {
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    VK_CHECK(vmaCreateImage(m_vk->allocator(),
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
        vmaDestroyImage(m_vk->allocator(), m_vkImage, m_vmaAllocation);
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
    VK_CHECK(
        m_vk->CreateImageView(m_vk->device, &m_info, nullptr, &m_vkImageView));
}

TextureView::~TextureView()
{
    m_vk->DestroyImageView(m_vk->device, m_vkImageView, nullptr);
}

Framebuffer::Framebuffer(rcp<VulkanContext> vk,
                         const VkFramebufferCreateInfo& info) :
    RenderingResource(std::move(vk)), m_info(info)
{
    m_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    VK_CHECK(m_vk->CreateFramebuffer(m_vk->device,
                                     &m_info,
                                     nullptr,
                                     &m_vkFramebuffer));
}

Framebuffer::~Framebuffer()
{
    m_vk->DestroyFramebuffer(m_vk->device, m_vkFramebuffer, nullptr);
}
} // namespace rive::gpu::vkutil
