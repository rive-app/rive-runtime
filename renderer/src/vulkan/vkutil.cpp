/*
 * Copyright 2023 Rive
 */

#include "rive/renderer/vulkan/vkutil.hpp"

#include "rive/rive_types.hpp"
#include "rive/renderer/vulkan/vulkan_context.hpp"
#include <vk_mem_alloc.h>

namespace rive::gpu::vkutil
{
#define STR_CASE(result)                                                       \
    case VK_##result:                                                          \
        return #result

const char* string_from_vk_result(VkResult result)
{
    switch (result)
    {
        STR_CASE(SUCCESS);
        STR_CASE(NOT_READY);
        STR_CASE(TIMEOUT);
        STR_CASE(EVENT_SET);
        STR_CASE(EVENT_RESET);
        STR_CASE(INCOMPLETE);
        STR_CASE(ERROR_OUT_OF_HOST_MEMORY);
        STR_CASE(ERROR_OUT_OF_DEVICE_MEMORY);
        STR_CASE(ERROR_INITIALIZATION_FAILED);
        STR_CASE(ERROR_DEVICE_LOST);
        STR_CASE(ERROR_MEMORY_MAP_FAILED);
        STR_CASE(ERROR_LAYER_NOT_PRESENT);
        STR_CASE(ERROR_EXTENSION_NOT_PRESENT);
        STR_CASE(ERROR_FEATURE_NOT_PRESENT);
        STR_CASE(ERROR_INCOMPATIBLE_DRIVER);
        STR_CASE(ERROR_TOO_MANY_OBJECTS);
        STR_CASE(ERROR_FORMAT_NOT_SUPPORTED);
        STR_CASE(ERROR_SURFACE_LOST_KHR);
        STR_CASE(SUBOPTIMAL_KHR);
        STR_CASE(ERROR_OUT_OF_DATE_KHR);
        STR_CASE(ERROR_INCOMPATIBLE_DISPLAY_KHR);
        STR_CASE(ERROR_NATIVE_WINDOW_IN_USE_KHR);
        STR_CASE(ERROR_VALIDATION_FAILED_EXT);
        default:
            return "<unknown>";
    }
}
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

Image::Image(rcp<VulkanContext> vulkanContext, const VkImageCreateInfo& info) :
    Resource(std::move(vulkanContext)), m_info(info)
{
    m_info = info;
    m_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;

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

Image::~Image()
{
    if (m_vmaAllocation != VK_NULL_HANDLE)
    {
        vmaDestroyImage(vk()->allocator(), m_vkImage, m_vmaAllocation);
    }
}

ImageView::ImageView(rcp<VulkanContext> vulkanContext,
                     rcp<Image> textureRef,
                     const VkImageViewCreateInfo& info) :
    Resource(std::move(vulkanContext)),
    m_textureRefOrNull(std::move(textureRef)),
    m_info(info)
{
    if (m_info.image == VK_NULL_HANDLE)
    {
        assert(m_textureRefOrNull != nullptr);
        m_info.image = *m_textureRefOrNull;
    }
    else
    {
        assert(m_textureRefOrNull == nullptr ||
               m_info.image == *m_textureRefOrNull);
    }
    m_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    VK_CHECK(
        vk()->CreateImageView(vk()->device, &m_info, nullptr, &m_vkImageView));
}

ImageView::~ImageView()
{
    vk()->DestroyImageView(vk()->device, m_vkImageView, nullptr);
}

Texture2D::Texture2D(rcp<VulkanContext> vk, VkImageCreateInfo info) :
    rive::gpu::Texture(info.extent.width, info.extent.height),
    m_lastAccess({
        .pipelineStages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        .accessMask = VK_ACCESS_NONE,
        .layout = VK_IMAGE_LAYOUT_UNDEFINED,
    })
{
    assert(info.imageType == 0 || info.imageType == VK_IMAGE_TYPE_2D);
    if (info.imageType == 0)
    {
        info.imageType = VK_IMAGE_TYPE_2D;
    }
    if (info.extent.depth == 0)
    {
        info.extent.depth = 1;
    }
    if (info.usage == 0)
    {
        info.usage =
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
    m_image = vk->makeImage(info);
    m_imageView = vk->makeImageView(m_image);
}

void Texture2D::scheduleUpload(const void* imageData,
                               size_t imageDataSizeInBytes)
{
    m_imageUploadBuffer = m_image->vk()->makeBuffer(
        {
            .size = imageDataSizeInBytes,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        },
        vkutil::Mappability::writeOnly);
    memcpy(m_imageUploadBuffer->contents(), imageData, imageDataSizeInBytes);
    m_imageUploadBuffer->flushContents();
}

void Texture2D::applyImageUploadBuffer(VkCommandBuffer commandBuffer)
{
    assert(m_imageUploadBuffer != nullptr);

    VkBufferImageCopy bufferImageCopy = {
        .imageSubresource =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .layerCount = 1,
            },
        .imageExtent = {width(), height(), 1},
    };

    barrier(commandBuffer,
            {
                .pipelineStages = VK_PIPELINE_STAGE_TRANSFER_BIT,
                .accessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            },
            vkutil::ImageAccessAction::invalidateContents);

    m_image->vk()->CmdCopyBufferToImage(commandBuffer,
                                        *m_imageUploadBuffer,
                                        *m_image,
                                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                        1,
                                        &bufferImageCopy);

    generateMipmaps(commandBuffer,
                    {
                        .pipelineStages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        .accessMask = VK_ACCESS_SHADER_READ_BIT,
                        .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    });

    m_imageUploadBuffer = nullptr;
}

void Texture2D::barrier(VkCommandBuffer commandBuffer,
                        const vkutil::ImageAccess& dstAccess,
                        vkutil::ImageAccessAction imageAccessAction,
                        VkDependencyFlags dependencyFlags)
{
    if (m_lastAccess != dstAccess)
    {
        m_lastAccess =
            m_image->vk()->simpleImageMemoryBarrier(commandBuffer,
                                                    m_lastAccess,
                                                    dstAccess,
                                                    *m_image,
                                                    imageAccessAction,
                                                    dependencyFlags);
    }
}

void Texture2D::generateMipmaps(VkCommandBuffer commandBuffer,
                                const ImageAccess& dstAccess)
{
    VulkanContext* vk = m_image->vk();
    uint32_t mipLevels = m_image->info().mipLevels;
    if (mipLevels <= 1)
    {
        barrier(commandBuffer, dstAccess);
        return;
    }

    barrier(commandBuffer,
            {
                .pipelineStages = VK_PIPELINE_STAGE_TRANSFER_BIT,
                .accessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            });

    int2 dstSize, srcSize = {static_cast<int32_t>(width()),
                             static_cast<int32_t>(height())};
    for (uint32_t level = 1; level < mipLevels; ++level, srcSize = dstSize)
    {
        dstSize = simd::max(srcSize >> 1, int2(1));

        VkImageBlit imageBlit = {
            .srcSubresource =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = level - 1,
                    .layerCount = 1,
                },
            .dstSubresource =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = level,
                    .layerCount = 1,
                },
        };

        imageBlit.srcOffsets[0] = {0, 0, 0};
        imageBlit.srcOffsets[1] = {srcSize.x, srcSize.y, 1};

        imageBlit.dstOffsets[0] = {0, 0, 0};
        imageBlit.dstOffsets[1] = {dstSize.x, dstSize.y, 1};

        vk->imageMemoryBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            {
                .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .image = *m_image,
                .subresourceRange =
                    {
                        .baseMipLevel = level - 1,
                        .levelCount = 1,
                    },
            });

        vk->CmdBlitImage(commandBuffer,
                         *m_image,
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         *m_image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         1,
                         &imageBlit,
                         VK_FILTER_LINEAR);
    }

    VkImageMemoryBarrier barriers[] = {
        {
            // The first N - 1 layers are in TRANSFER_READ.
            .srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
            .dstAccessMask = dstAccess.accessMask,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .newLayout = dstAccess.layout,
            .image = *m_image,
            .subresourceRange =
                {
                    .baseMipLevel = 0,
                    .levelCount = mipLevels - 1,
                },
        },
        {
            // The final layer is still in TRANSFER_WRITE.
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = dstAccess.accessMask,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = dstAccess.layout,
            .image = *m_image,
            .subresourceRange =
                {
                    .baseMipLevel = mipLevels - 1,
                    .levelCount = 1,
                },
        },
    };

    vk->imageMemoryBarriers(commandBuffer,
                            VK_PIPELINE_STAGE_TRANSFER_BIT,
                            dstAccess.pipelineStages,
                            0,
                            std::size(barriers),
                            barriers);

    m_lastAccess = dstAccess;
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
