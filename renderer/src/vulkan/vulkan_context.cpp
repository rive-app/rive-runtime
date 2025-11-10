/*
 * Copyright 2023 Rive
 */

#include "rive/renderer/vulkan/vulkan_context.hpp"

#include "rive/rive_types.hpp"
#include <vk_mem_alloc.h>

namespace rive::gpu
{
static VmaAllocator make_vma_allocator(
    const VulkanContext* vk,
    PFN_vkGetInstanceProcAddr pfnvkGetInstanceProcAddr)
{
    VmaAllocator vmaAllocator;
    VmaVulkanFunctions vmaVulkanFunctions = {
        .vkGetInstanceProcAddr = pfnvkGetInstanceProcAddr,
        .vkGetDeviceProcAddr = vk->GetDeviceProcAddr,
        .vkGetPhysicalDeviceProperties = vk->GetPhysicalDeviceProperties,
    };

    VmaAllocatorCreateInfo vmaCreateInfo = {
        // We are single-threaded.
        .flags = VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT,
        .physicalDevice = vk->physicalDevice,
        .device = vk->device,
        .pVulkanFunctions = &vmaVulkanFunctions,
        .instance = vk->instance,
        .vulkanApiVersion = vk->features.apiVersion,
    };
    VK_CHECK(vmaCreateAllocator(&vmaCreateInfo, &vmaAllocator));
    return vmaAllocator;
}

VulkanContext::VulkanContext(
    VkInstance instance,
    VkPhysicalDevice physicalDevice_,
    VkDevice device_,
    const VulkanFeatures& features_,
    PFN_vkGetInstanceProcAddr pfnvkGetInstanceProcAddr) :
    instance(instance),
    physicalDevice(physicalDevice_),
    device(device_),
    features(features_),
#define LOAD_VULKAN_INSTANCE_COMMAND(CMD)                                      \
    CMD(reinterpret_cast<PFN_vk##CMD>(                                         \
        pfnvkGetInstanceProcAddr(instance, "vk" #CMD))),
    RIVE_VULKAN_INSTANCE_COMMANDS(LOAD_VULKAN_INSTANCE_COMMAND)
#undef LOAD_VULKAN_INSTANCE_COMMAND
#define LOAD_VULKAN_DEVICE_COMMAND(CMD)                                        \
    CMD(reinterpret_cast<PFN_vk##CMD>(GetDeviceProcAddr(device, "vk" #CMD))),
        RIVE_VULKAN_DEVICE_COMMANDS(LOAD_VULKAN_DEVICE_COMMAND)
#undef LOAD_VULKAN_DEVICE_COMMAND
            m_vmaAllocator(make_vma_allocator(this, pfnvkGetInstanceProcAddr))
{
#ifdef NDEBUG
    // Check that we weren't told the device was more capable than it is
    {
        VkPhysicalDeviceProperties props{};
        GetPhysicalDeviceProperties(physicalDevice, &props);
        assert(
            props.apiVersion >= features.apiVersion &&
            "Supplied API version should not be newer than the physical device");
    }
#endif

    // VK spec says between D24_S8 and D32_S8, one of them must be supported
    m_supportsD24S8 = isFormatSupportedWithFeatureFlags(
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

    // This assert should never fire unless some hardware is breaking the VK
    // spec by reporting that it does not support one of these formats.
    assert((m_supportsD24S8 ||
            isFormatSupportedWithFeatureFlags(
                VK_FORMAT_D32_SFLOAT_S8_UINT,
                VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)) &&
           "No suitable depth format supported!");
}

VulkanContext::~VulkanContext() { vmaDestroyAllocator(m_vmaAllocator); }

bool VulkanContext::isFormatSupportedWithFeatureFlags(
    VkFormat format,
    VkFormatFeatureFlagBits featureFlags)
{
    // Can flesch this out, but currently just checks if format's optimal tiling
    // features include the provided bits.
    VkFormatProperties properties;
    GetPhysicalDeviceFormatProperties(physicalDevice, format, &properties);
    return properties.optimalTilingFeatures & featureFlags;
}

rcp<vkutil::Buffer> VulkanContext::makeBuffer(const VkBufferCreateInfo& info,
                                              vkutil::Mappability mappability)
{
    return rcp(new vkutil::Buffer(ref_rcp(this), info, mappability));
}

rcp<vkutil::Image> VulkanContext::makeImage(const VkImageCreateInfo& info)
{
    return rcp(new vkutil::Image(ref_rcp(this), info));
}

rcp<vkutil::Framebuffer> VulkanContext::makeFramebuffer(
    const VkFramebufferCreateInfo& info)
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
            fprintf(
                stderr,
                "vkutil::image_view_type_for_image_type: VkImageType %u is not "
                "supported\n",
                type);
    }
    RIVE_UNREACHABLE();
}

static VkImageAspectFlags image_aspect_flags_for_format(VkFormat format)
{
    switch (format)
    {
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        default:
            return VK_IMAGE_ASPECT_COLOR_BIT;
    }
    RIVE_UNREACHABLE();
}

rcp<vkutil::ImageView> VulkanContext::makeImageView(rcp<vkutil::Image> image)
{
    const VkImageCreateInfo& texInfo = image->info();

    return makeImageView(
        image,
        {
            .viewType = image_view_type_for_image_type(texInfo.imageType),
            .format = texInfo.format,
            .subresourceRange =
                {
                    .aspectMask = image_aspect_flags_for_format(texInfo.format),
                    .levelCount = texInfo.mipLevels,
                    .layerCount = 1,
                },
        });
}

rcp<vkutil::ImageView> VulkanContext::makeImageView(
    rcp<vkutil::Image> image,
    const VkImageViewCreateInfo& info)
{
    assert(image);
    return rcp(new vkutil::ImageView(ref_rcp(this), std::move(image), info));
}

rcp<vkutil::ImageView> VulkanContext::makeExternalImageView(
    const VkImageViewCreateInfo& info)
{
    return rcp<vkutil::ImageView>(
        new vkutil::ImageView(ref_rcp(this), nullptr, info));
}

rcp<vkutil::Texture2D> VulkanContext::makeTexture2D(
    const VkImageCreateInfo& info)
{
    return rcp<vkutil::Texture2D>(new vkutil::Texture2D(ref_rcp(this), info));
}

void VulkanContext::updateImageDescriptorSets(
    VkDescriptorSet vkDescriptorSet,
    VkWriteDescriptorSet writeSet,
    std::initializer_list<VkDescriptorImageInfo> imageInfos)
{
    writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeSet.dstSet = vkDescriptorSet;
    writeSet.descriptorCount =
        math::lossless_numeric_cast<uint32_t>(imageInfos.size());
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
    writeSet.descriptorCount =
        math::lossless_numeric_cast<uint32_t>(bufferInfos.size());
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

void VulkanContext::imageMemoryBarriers(
    VkCommandBuffer commandBuffer,
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
            imageMemoryBarrier.subresourceRange.aspectMask =
                VK_IMAGE_ASPECT_COLOR_BIT;
        }
        if (imageMemoryBarrier.subresourceRange.levelCount == 0)
        {
            imageMemoryBarrier.subresourceRange.levelCount =
                VK_REMAINING_MIP_LEVELS;
        }
        if (imageMemoryBarrier.subresourceRange.layerCount == 0)
        {
            imageMemoryBarrier.subresourceRange.layerCount =
                VK_REMAINING_ARRAY_LAYERS;
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

const vkutil::ImageAccess& VulkanContext::simpleImageMemoryBarrier(
    VkCommandBuffer commandBuffer,
    const vkutil::ImageAccess& srcAccess,
    const vkutil::ImageAccess& dstAccess,
    VkImage image,
    vkutil::ImageAccessAction imageAccessAction,
    VkDependencyFlags dependencyFlags)
{
    assert(image != VK_NULL_HANDLE);
    if (srcAccess != dstAccess)
    {
        imageMemoryBarrier(
            commandBuffer,
            srcAccess.pipelineStages,
            dstAccess.pipelineStages,
            dependencyFlags,
            {
                .srcAccessMask = srcAccess.accessMask,
                .dstAccessMask = dstAccess.accessMask,
                .oldLayout = imageAccessAction ==
                                     vkutil::ImageAccessAction::preserveContents
                                 ? srcAccess.layout
                                 : VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = dstAccess.layout,
                .image = image,
            });
    }
    return dstAccess;
}

void VulkanContext::bufferMemoryBarrier(
    VkCommandBuffer commandBuffer,
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

void VulkanContext::clearColorImage(VkCommandBuffer commandBuffer,
                                    ColorInt clearColor,
                                    VkImage image,
                                    VkImageLayout imageLayout)
{
    const VkClearColorValue colorClearValue = {
        vkutil::color_clear_rgba32f(clearColor)};

    const VkImageSubresourceRange colorClearRange = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
    };

    CmdClearColorImage(commandBuffer,
                       image,
                       imageLayout,
                       &colorClearValue,
                       1,
                       &colorClearRange);
}

void VulkanContext::blitSubRect(VkCommandBuffer commandBuffer,
                                VkImage srcImage,
                                VkImageLayout srcImageLayout,
                                VkImage dstImage,
                                VkImageLayout dstImageLayout,
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
                 srcImage,
                 srcImageLayout,
                 dstImage,
                 dstImageLayout,
                 1,
                 &imageBlit,
                 VK_FILTER_NEAREST);
}
} // namespace rive::gpu
