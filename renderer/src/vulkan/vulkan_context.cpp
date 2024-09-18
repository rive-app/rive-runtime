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
        // We are single-threaded.
        .flags = VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT,
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

void VulkanContext::memoryBarrier(VkCommandBuffer commandBuffer,
                                  VkPipelineStageFlags srcStageMask,
                                  VkPipelineStageFlags dstStageMask,
                                  VkDependencyFlags dependencyFlags,
                                  VkMemoryBarrier memoryBarrier)
{
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    CmdPipelineBarrier(commandBuffer,
                       srcStageMask,
                       dstStageMask,
                       dependencyFlags,
                       1,
                       &memoryBarrier,
                       0,
                       nullptr,
                       0,
                       nullptr);
}

void VulkanContext::imageMemoryBarriers(VkCommandBuffer commandBuffer,
                                        VkPipelineStageFlags srcStageMask,
                                        VkPipelineStageFlags dstStageMask,
                                        VkDependencyFlags dependencyFlags,
                                        uint32_t count,
                                        VkImageMemoryBarrier* imageMemoryBarriers)
{
    for (uint32_t i = 0; i < count; ++i)
    {
        auto& imageMemoryBarrier = imageMemoryBarriers[i];
        imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        if (imageMemoryBarrier.subresourceRange.aspectMask == 0)
        {
            imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }
        if (imageMemoryBarrier.subresourceRange.levelCount == 0)
        {
            imageMemoryBarrier.subresourceRange.levelCount = 1;
        }
        if (imageMemoryBarrier.subresourceRange.layerCount == 0)
        {
            imageMemoryBarrier.subresourceRange.layerCount = 1;
        }
    }
    CmdPipelineBarrier(commandBuffer,
                       srcStageMask,
                       dstStageMask,
                       dependencyFlags,
                       0,
                       nullptr,
                       0,
                       nullptr,
                       count,
                       imageMemoryBarriers);
}

void VulkanContext::bufferMemoryBarrier(VkCommandBuffer commandBuffer,
                                        VkPipelineStageFlags srcStageMask,
                                        VkPipelineStageFlags dstStageMask,
                                        VkDependencyFlags dependencyFlags,
                                        VkBufferMemoryBarrier bufferMemoryBarrier)
{
    bufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    if (bufferMemoryBarrier.size == 0)
    {
        bufferMemoryBarrier.size = VK_WHOLE_SIZE;
    }
    CmdPipelineBarrier(commandBuffer,
                       srcStageMask,
                       dstStageMask,
                       dependencyFlags,
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
    if (blitBounds.empty())
    {
        return;
    }

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
