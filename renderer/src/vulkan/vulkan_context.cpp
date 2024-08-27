/*
 * Copyright 2023 Rive
 */

#include "rive/renderer/vulkan/vulkan_context.hpp"

#include "rive/rive_types.hpp"

namespace rive::gpu
{
static VmaAllocator make_vma_allocator(VmaAllocatorCreateInfo vmaCreateInfo)
{
    VmaAllocator vmaAllocator;
    VK_CHECK(vmaCreateAllocator(&vmaCreateInfo, &vmaAllocator));
    return vmaAllocator;
}

VulkanContext::VulkanContext(VkInstance instance,
                             VkPhysicalDevice physicalDevice_,
                             VkDevice device_,
                             const VulkanFeatures& features_,
                             PFN_vkGetInstanceProcAddr fp_vkGetInstanceProcAddr,
                             PFN_vkGetDeviceProcAddr fp_vkGetDeviceProcAddr) :
    instance(instance),
    physicalDevice(physicalDevice_),
    device(device_),
    features(features_),
    vmaAllocator(make_vma_allocator({
        .flags = VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT, // We are single-threaded.
        .physicalDevice = physicalDevice,
        .device = device,
        .pVulkanFunctions =
            &initVmaVulkanFunctions(fp_vkGetInstanceProcAddr, fp_vkGetDeviceProcAddr),
        .instance = instance,
        .vulkanApiVersion = features.vulkanApiVersion,
    }))
// clang-format off
#define LOAD_VULKAN_INSTANCE_COMMAND(CMD)                                                          \
    , CMD(reinterpret_cast<PFN_vk##CMD>(fp_vkGetInstanceProcAddr(instance, "vk" #CMD)))
#define LOAD_VULKAN_DEVICE_COMMAND(CMD)                                                            \
    , CMD(reinterpret_cast<PFN_vk##CMD>(fp_vkGetDeviceProcAddr(device, "vk" #CMD)))
    RIVE_VULKAN_INSTANCE_COMMANDS(LOAD_VULKAN_INSTANCE_COMMAND)
    RIVE_VULKAN_DEVICE_COMMANDS(LOAD_VULKAN_DEVICE_COMMAND)
#undef LOAD_VULKAN_DEVICE_COMMAND
#undef LOAD_VULKAN_INSTANCE_COMMAND
// clang-format on
{}

VulkanContext::~VulkanContext()
{
    assert(m_shutdown);
    assert(m_resourcePurgatory.empty());
    vmaDestroyAllocator(vmaAllocator);
}

void VulkanContext::onNewFrameBegun()
{
    ++m_currentFrameIdx;

    // Delete all resources that are no longer referenced by an in-flight command
    // buffer.
    while (!m_resourcePurgatory.empty() &&
           m_resourcePurgatory.front().expirationFrameIdx <= m_currentFrameIdx)
    {
        m_resourcePurgatory.pop_front();
    }
}

void VulkanContext::onRenderingResourceReleased(const vkutil::RenderingResource* resource)
{
    assert(resource->vulkanContext() == this);
    if (!m_shutdown)
    {
        // Hold this resource until it is no longer referenced by an in-flight
        // command buffer.
        m_resourcePurgatory.emplace_back(resource, m_currentFrameIdx);
    }
    else
    {
        // We're in a shutdown cycle. Delete immediately.
        delete resource;
    }
}

void VulkanContext::shutdown()
{
    m_shutdown = true;

    // Validate m_resourcePurgatory: We shouldn't have any resources queued up with
    // larger expirations than "gpu::kBufferRingSize" frames.
    for (size_t i = 0; i < gpu::kBufferRingSize; ++i)
    {
        onNewFrameBegun();
    }
    assert(m_resourcePurgatory.empty());

    // Explicitly delete the resources anyway, just to be safe for release mode.
    m_resourcePurgatory.clear();
}

rcp<vkutil::Buffer> VulkanContext::makeBuffer(const VkBufferCreateInfo& info,
                                              vkutil::Mappability mappability)
{
    return rcp(new vkutil::Buffer(ref_rcp(this), info, mappability));
}

rcp<vkutil::Texture> VulkanContext::makeTexture(const VkImageCreateInfo& info)
{
    return rcp(new vkutil::Texture(ref_rcp(this), info));
}

rcp<vkutil::Framebuffer> VulkanContext::makeFramebuffer(const VkFramebufferCreateInfo& info)
{
    return rcp(new vkutil::Framebuffer(ref_rcp(this), info));
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

rcp<vkutil::TextureView> VulkanContext::makeTextureView(rcp<vkutil::Texture> texture)
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

rcp<vkutil::TextureView> VulkanContext::makeTextureView(rcp<vkutil::Texture> texture,
                                                        const VkImageViewCreateInfo& info)
{
    assert(texture);
    auto usage = texture->info().usage;
    return rcp(new vkutil::TextureView(ref_rcp(this), std::move(texture), usage, info));
}

rcp<vkutil::TextureView> VulkanContext::makeExternalTextureView(const VkImageUsageFlags flags,
                                                                const VkImageViewCreateInfo& info)
{
    return rcp<vkutil::TextureView>(new vkutil::TextureView(ref_rcp(this), nullptr, flags, info));
}

void VulkanContext::updateImageDescriptorSets(
    VkDescriptorSet vkDescriptorSet,
    VkWriteDescriptorSet writeSet,
    std::initializer_list<VkDescriptorImageInfo> imageInfos)
{
    writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeSet.dstSet = vkDescriptorSet;
    writeSet.descriptorCount = math::lossless_numeric_cast<uint32_t>(imageInfos.size());
    writeSet.pImageInfo = imageInfos.begin();
    UpdateDescriptorSets(device, 1, &writeSet, 0, nullptr);
}

void VulkanContext::updateBufferDescriptorSets(
    VkDescriptorSet vkDescriptorSet,
    VkWriteDescriptorSet writeSet,
    std::initializer_list<VkDescriptorBufferInfo> bufferInfos)
{
    writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeSet.dstSet = vkDescriptorSet;
    writeSet.descriptorCount = math::lossless_numeric_cast<uint32_t>(bufferInfos.size());
    writeSet.pBufferInfo = bufferInfos.begin();
    UpdateDescriptorSets(device, 1, &writeSet, 0, nullptr);
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

void VulkanContext::insertImageMemoryBarrier(VkCommandBuffer commandBuffer,
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

    CmdPipelineBarrier(commandBuffer,
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

void VulkanContext::insertBufferMemoryBarrier(VkCommandBuffer commandBuffer,
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

    CmdPipelineBarrier(commandBuffer,
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

void VulkanContext::blitSubRect(VkCommandBuffer commandBuffer,
                                VkImage src,
                                VkImage dst,
                                const IAABB& blitBounds)
{
    VkImageBlit imageBlit = {
        .srcSubresource =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .layerCount = 1,
            },
        .dstSubresource =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .layerCount = 1,
            },
    };

    imageBlit.srcOffsets[0] = {blitBounds.left, blitBounds.top, 0};
    imageBlit.srcOffsets[1] = {blitBounds.right, blitBounds.bottom, 1};

    imageBlit.dstOffsets[0] = {blitBounds.left, blitBounds.top, 0};
    imageBlit.dstOffsets[1] = {blitBounds.right, blitBounds.bottom, 1};

    CmdBlitImage(commandBuffer,
                 src,
                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                 dst,
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                 1,
                 &imageBlit,
                 VK_FILTER_NEAREST);
}
} // namespace rive::gpu
