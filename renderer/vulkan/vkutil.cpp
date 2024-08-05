/*
 * Copyright 2023 Rive
 */

#include "rive/pls/vulkan/vkutil.hpp"

#include "rive/rive_types.hpp"
#include "rive/pls/vulkan/pls_render_context_vulkan_impl.hpp"
#include <vk_mem_alloc.h>

namespace rive::pls::vkutil
{
Allocator::Allocator(VkInstance instance,
                     VkPhysicalDevice physicalDevice,
                     VkDevice device,
                     uint32_t vulkanApiVersion) :
    m_device(device)
{
    VmaAllocatorCreateInfo vmaCreateInfo = {
        // This all runs in one thread.
        .flags = VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT,
        .physicalDevice = physicalDevice,
        .device = m_device,
        .instance = instance,
        .vulkanApiVersion = vulkanApiVersion,
    };

    VK_CHECK(vmaCreateAllocator(&vmaCreateInfo, &m_vmaAllocator));
}

Allocator::~Allocator()
{
    // At this point we can't be waiting for PLSTextureVulkanImpl's fences. We just
    // delete the allocator.
    assert(m_plsImplVulkan == nullptr);
    vmaDestroyAllocator(m_vmaAllocator);
}

rcp<Buffer> Allocator::makeBuffer(const VkBufferCreateInfo& info, Mappability mappability)
{
    return rcp(new Buffer(ref_rcp(this), info, mappability));
}

rcp<Texture> Allocator::makeTexture(const VkImageCreateInfo& info)
{
    return rcp(new Texture(ref_rcp(this), info));
}

rcp<Framebuffer> Allocator::makeFramebuffer(const VkFramebufferCreateInfo& info)
{
    return rcp(new Framebuffer(ref_rcp(this), info));
}

static VkImageViewType image_view_type_for_image_type(VkImageType type)
{
    switch (type)
    {
        case VK_IMAGE_TYPE_2D:
            return VK_IMAGE_VIEW_TYPE_2D;
        default:
            fprintf(stderr,
                    "vkutil::image_view_type_for_image_type: VkImageType %u is not "
                    "supported\n",
                    type);
    }
    RIVE_UNREACHABLE();
}

rcp<TextureView> Allocator::makeTextureView(rcp<Texture> texture)
{
    return makeTextureView(
        texture,
        {
            .image = *texture,
            .viewType = image_view_type_for_image_type(texture->info().imageType),
            .format = texture->info().format,
            .subresourceRange =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .levelCount = texture->info().mipLevels,
                    .layerCount = 1,
                },
        });
}

rcp<TextureView> Allocator::makeTextureView(rcp<Texture> textureRefOrNull,
                                            const VkImageViewCreateInfo& info)
{
    return rcp(new TextureView(ref_rcp(this), std::move(textureRefOrNull), info));
}

void RenderingResource::onRefCntReachedZero() const
{
    if (plsImplVulkan() != nullptr)
    {
        // PLSRenderContextVulkanImpl will hold off on deleting "this" until any
        // in-flight command buffers have finished (potentially) referencing our
        // underlying Vulkan objects.
        plsImplVulkan()->onRenderingResourceReleased(this);
    }
    else
    {
        delete this;
    }
}

Buffer::Buffer(rcp<Allocator> allocator, const VkBufferCreateInfo& info, Mappability mappability) :
    RenderingResource(std::move(allocator)), m_mappability(mappability), m_info(info)
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
            vmaUnmapMemory(m_allocator->vmaAllocator(), m_vmaAllocation);
            vmaDestroyBuffer(m_allocator->vmaAllocator(), m_vkBuffer, m_vmaAllocation);
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

        VK_CHECK(vmaCreateBuffer(m_allocator->vmaAllocator(),
                                 &m_info,
                                 &allocInfo,
                                 &m_vkBuffer,
                                 &m_vmaAllocation,
                                 nullptr));

        // Leave the buffer constantly mapped and let the OS/drivers handle the
        // rest.
        VK_CHECK(vmaMapMemory(m_allocator->vmaAllocator(), m_vmaAllocation, &m_contents));
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
    vmaFlushAllocation(m_allocator->vmaAllocator(), m_vmaAllocation, 0, updatedSizeInBytes);
}

Texture::Texture(rcp<Allocator> allocator, const VkImageCreateInfo& info) :
    RenderingResource(std::move(allocator)), m_info(info)
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

        if (vmaCreateImage(m_allocator->vmaAllocator(),
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

    VK_CHECK(vmaCreateImage(m_allocator->vmaAllocator(),
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
        vmaDestroyImage(m_allocator->vmaAllocator(), m_vkImage, m_vmaAllocation);
    }
}

TextureView::TextureView(rcp<Allocator> allocator,
                         rcp<Texture> textureRefOrNull,
                         const VkImageViewCreateInfo& info) :
    RenderingResource(std::move(allocator)),
    m_textureRefOrNull(std::move(textureRefOrNull)),
    m_info(info)
{
    m_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    VK_CHECK(vkCreateImageView(device(), &m_info, nullptr, &m_vkImageView));
}

TextureView::~TextureView() { vkDestroyImageView(device(), m_vkImageView, nullptr); }

Framebuffer::Framebuffer(rcp<Allocator> allocator, const VkFramebufferCreateInfo& info) :
    RenderingResource(std::move(allocator)), m_info(info)
{
    m_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    VK_CHECK(vkCreateFramebuffer(device(), &m_info, nullptr, &m_vkFramebuffer));
}

Framebuffer::~Framebuffer() { vkDestroyFramebuffer(device(), m_vkFramebuffer, nullptr); }

static VkAccessFlags access_flags_for_layout(VkImageLayout layout)
{
    switch (layout)
    {
        case VK_IMAGE_LAYOUT_UNDEFINED:
            return VK_ACCESS_NONE;
        case VK_IMAGE_LAYOUT_GENERAL:
            return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            return VK_ACCESS_TRANSFER_WRITE_BIT;
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            return VK_ACCESS_TRANSFER_READ_BIT;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            return VK_ACCESS_SHADER_READ_BIT;
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
            return VK_ACCESS_NONE;
        default:
            fprintf(stderr,
                    "vkutil::insert_image_memory_barrier: layout 0x%x is not "
                    "supported\n",
                    layout);
    }
    RIVE_UNREACHABLE();
}

static VkAccessFlags pipeline_stage_for_layout(VkImageLayout layout)
{
    switch (layout)
    {
        case VK_IMAGE_LAYOUT_UNDEFINED:
            return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        case VK_IMAGE_LAYOUT_GENERAL:
            return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            return VK_PIPELINE_STAGE_TRANSFER_BIT;
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            return VK_PIPELINE_STAGE_TRANSFER_BIT;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            return VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
            return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        default:
            fprintf(stderr,
                    "vkutil::insert_image_memory_barrier: layout 0x%x is not "
                    "supported\n",
                    layout);
    }
    RIVE_UNREACHABLE();
}

void update_image_descriptor_sets(VkDevice vkDevice,
                                  VkDescriptorSet vkDescriptorSet,
                                  VkWriteDescriptorSet writeSet,
                                  std::initializer_list<VkDescriptorImageInfo> imageInfos)
{
    writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeSet.dstSet = vkDescriptorSet;
    writeSet.descriptorCount = std::size(imageInfos);
    writeSet.pImageInfo = imageInfos.begin();
    vkUpdateDescriptorSets(vkDevice, 1, &writeSet, 0, nullptr);
}

void update_buffer_descriptor_sets(VkDevice vkDevice,
                                   VkDescriptorSet vkDescriptorSet,
                                   VkWriteDescriptorSet writeSet,
                                   std::initializer_list<VkDescriptorBufferInfo> bufferInfos)
{
    writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeSet.dstSet = vkDescriptorSet;
    writeSet.descriptorCount = std::size(bufferInfos);
    writeSet.pBufferInfo = bufferInfos.begin();
    vkUpdateDescriptorSets(vkDevice, 1, &writeSet, 0, nullptr);
}

void insert_image_memory_barrier(VkCommandBuffer commandBuffer,
                                 VkImage image,
                                 VkImageLayout oldLayout,
                                 VkImageLayout newLayout,
                                 uint32_t mipLevel,
                                 uint32_t levelCount)
{
    VkImageMemoryBarrier imageMemoryBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = access_flags_for_layout(oldLayout),
        .dstAccessMask = access_flags_for_layout(newLayout),
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = mipLevel,
                .levelCount = levelCount,
                .layerCount = VK_REMAINING_ARRAY_LAYERS,
            },
    };

    vkCmdPipelineBarrier(commandBuffer,
                         pipeline_stage_for_layout(oldLayout),
                         pipeline_stage_for_layout(newLayout),
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &imageMemoryBarrier);
}

static VkAccessFlags pipeline_stage_for_buffer_access(VkAccessFlags access)
{
    switch (access)
    {
        case VK_ACCESS_TRANSFER_WRITE_BIT:
            return VK_PIPELINE_STAGE_TRANSFER_BIT;
        case VK_ACCESS_HOST_READ_BIT:
            return VK_PIPELINE_STAGE_HOST_BIT;
        default:
            fprintf(stderr,
                    "vkutil::insert_buffer_memory_barrier: access %u is not "
                    "supported\n",
                    access);
    }
    RIVE_UNREACHABLE();
}

void insert_buffer_memory_barrier(VkCommandBuffer commandBuffer,
                                  VkAccessFlags srcAccessMask,
                                  VkAccessFlags dstAccessMask,
                                  VkBuffer buffer,
                                  VkDeviceSize offset,
                                  VkDeviceSize size)
{
    VkBufferMemoryBarrier bufferMemoryBarrier = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask = srcAccessMask,
        .dstAccessMask = dstAccessMask,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = buffer,
        .offset = offset,
        .size = size,
    };

    vkCmdPipelineBarrier(commandBuffer,
                         pipeline_stage_for_buffer_access(srcAccessMask),
                         pipeline_stage_for_buffer_access(dstAccessMask),
                         0,
                         0,
                         nullptr,
                         1,
                         &bufferMemoryBarrier,
                         0,
                         nullptr);
}
} // namespace rive::pls::vkutil
