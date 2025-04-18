/*
 * Copyright 2023 Rive
 */

#include "rive/renderer/vulkan/vkutil.hpp"

#include "rive/rive_types.hpp"
#include "rive/renderer/vulkan/vulkan_context.hpp"
#include <vk_mem_alloc.h>

namespace rive::gpu::vkutil
{
Resource::Resource(rcp<VulkanContext> vk) : GPUResource(std::move(vk)) {}

VulkanContext* Resource::vk() const
{
    return static_cast<VulkanContext*>(m_manager.get());
}

Buffer::Buffer(rcp<VulkanContext> vk,
               const VkBufferCreateInfo& info,
               Mappability mappability) :
    Resource(std::move(vk)), m_mappability(mappability), m_info(info)
{
    m_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    init();
}

Buffer::~Buffer() { resizeImmediately(0); }

void Buffer::resizeImmediately(VkDeviceSize sizeInBytes)
{
    if (m_info.size != sizeInBytes)
    {
        if (m_vmaAllocation != VK_NULL_HANDLE)
        {
            if (m_mappability != Mappability::none)
            {
                vmaUnmapMemory(vk()->allocator(), m_vmaAllocation);
            }
            vmaDestroyBuffer(vk()->allocator(), m_vkBuffer, m_vmaAllocation);
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

        VK_CHECK(vmaCreateBuffer(vk()->allocator(),
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
                vmaMapMemory(vk()->allocator(), m_vmaAllocation, &m_contents));
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

void Buffer::flushContents(VkDeviceSize updatedSizeInBytes)
{
    vmaFlushAllocation(vk()->allocator(),
                       m_vmaAllocation,
                       0,
                       updatedSizeInBytes);
}

void Buffer::invalidateContents(VkDeviceSize updatedSizeInBytes)
{
    vmaInvalidateAllocation(vk()->allocator(),
                            m_vmaAllocation,
                            0,
                            updatedSizeInBytes);
}

BufferPool::BufferPool(rcp<VulkanContext> vk,
                       VkBufferUsageFlags usageFlags,
                       VkDeviceSize size) :
    GPUResourcePool(std::move(vk), MAX_POOL_SIZE),
    m_usageFlags(usageFlags),
    m_targetSize(size)
{}

inline VulkanContext* BufferPool::vk() const
{
    return static_cast<VulkanContext*>(m_manager.get());
}

void BufferPool::setTargetSize(VkDeviceSize size)
{
    // Buffers always get bound, even if unused, so make sure they aren't empty
    // and we get a valid Vulkan handle.
    size = std::max<VkDeviceSize>(size, 1);

    if (m_usageFlags & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
    {
        // Uniform blocks must be multiples of 256 bytes in size.
        size = std::max<VkDeviceSize>(size, 256);
        assert(size % 256 == 0);
    }

    m_targetSize = size;
}

rcp<vkutil::Buffer> BufferPool::acquire()
{
    auto buffer = static_rcp_cast<vkutil::Buffer>(GPUResourcePool::acquire());
    if (buffer == nullptr)
    {
        buffer = vk()->makeBuffer(
            {
                .size = m_targetSize,
                .usage = m_usageFlags,
            },
            Mappability::writeOnly);
    }
    else if (buffer->info().size != m_targetSize)
    {
        buffer->resizeImmediately(m_targetSize);
    }
    return buffer;
}

Texture::Texture(rcp<VulkanContext> vulkanContext,
                 const VkImageCreateInfo& info) :
    Resource(std::move(vulkanContext)), m_info(info)
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

        if (vmaCreateImage(vk()->allocator(),
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

    VK_CHECK(vmaCreateImage(vk()->allocator(),
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
        vmaDestroyImage(vk()->allocator(), m_vkImage, m_vmaAllocation);
    }
}

TextureView::TextureView(rcp<VulkanContext> vulkanContext,
                         rcp<Texture> textureRef,
                         const VkImageViewCreateInfo& info) :
    Resource(std::move(vulkanContext)),
    m_textureRefOrNull(std::move(textureRef)),
    m_info(info)
{
    assert(m_textureRefOrNull == nullptr || info.image == *m_textureRefOrNull);
    m_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    VK_CHECK(
        vk()->CreateImageView(vk()->device, &m_info, nullptr, &m_vkImageView));
}

TextureView::~TextureView()
{
    vk()->DestroyImageView(vk()->device, m_vkImageView, nullptr);
}

Framebuffer::Framebuffer(rcp<VulkanContext> vulkanContext,
                         const VkFramebufferCreateInfo& info) :
    Resource(std::move(vulkanContext)), m_info(info)
{
    m_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    VK_CHECK(vk()->CreateFramebuffer(vk()->device,
                                     &m_info,
                                     nullptr,
                                     &m_vkFramebuffer));
}

Framebuffer::~Framebuffer()
{
    vk()->DestroyFramebuffer(vk()->device, m_vkFramebuffer, nullptr);
}
} // namespace rive::gpu::vkutil
