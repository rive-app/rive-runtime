/*
 * Copyright 2024 Rive
 */

#pragma once

#include "rive/renderer/gpu_resource.hpp"
#include "rive/renderer/vulkan/vkutil.hpp"

VK_DEFINE_HANDLE(VmaAllocator);

namespace rive::gpu
{
// Specifies the Vulkan API version and which relevant features have been
// enabled. The client should ensure the features get enabled if they are
// supported.
struct VulkanFeatures
{
    uint32_t apiVersion = VK_API_VERSION_1_0;

    // VkPhysicalDeviceFeatures.
    bool independentBlend = false;
    bool fillModeNonSolid = false;
    bool fragmentStoresAndAtomics = false;
    bool shaderClipDistance = false;

    // EXT_rasterization_order_attachment_access.
    bool rasterizationOrderColorAttachmentAccess = false;

    // VK_EXT_fragment_shader_interlock.
    bool fragmentShaderPixelInterlock = false;

    // Indicates a nonconformant driver, like MoltenVK.
    bool VK_KHR_portability_subset;
};

// Wraps a VkDevice, function dispatch table, and VMA library instance.
//
// Provides methods to allocate vkutil::RenderingResource objects, and manages
// their lifecycles via a "resource purgatory", which keeps resources alive
// until command buffers have finished using them.
//
// Provides minor helper utilities, but for the most part, the client is
// expected to make raw Vulkan calls via the provided function pointers.
class VulkanContext : public GPUResourceManager
{
public:
    VulkanContext(VkInstance,
                  VkPhysicalDevice,
                  VkDevice,
                  const VulkanFeatures&,
                  PFN_vkGetInstanceProcAddr);

    ~VulkanContext();

    const VkInstance instance;
    const VkPhysicalDevice physicalDevice;
    const VkDevice device;
    const VulkanFeatures features;

#define RIVE_VULKAN_INSTANCE_COMMANDS(F)                                       \
    F(GetDeviceProcAddr)                                                       \
    F(GetPhysicalDeviceFormatProperties)                                       \
    F(GetPhysicalDeviceProperties)                                             \
    F(SetDebugUtilsObjectNameEXT)

#define RIVE_VULKAN_DEVICE_COMMANDS(F)                                         \
    F(AllocateDescriptorSets)                                                  \
    F(CmdBeginRenderPass)                                                      \
    F(CmdBindDescriptorSets)                                                   \
    F(CmdBindIndexBuffer)                                                      \
    F(CmdBindPipeline)                                                         \
    F(CmdBindVertexBuffers)                                                    \
    F(CmdBlitImage)                                                            \
    F(CmdClearColorImage)                                                      \
    F(CmdCopyBufferToImage)                                                    \
    F(CmdDraw)                                                                 \
    F(CmdDrawIndexed)                                                          \
    F(CmdEndRenderPass)                                                        \
    F(CmdFillBuffer)                                                           \
    F(CmdNextSubpass)                                                          \
    F(CmdPipelineBarrier)                                                      \
    F(CmdSetScissor)                                                           \
    F(CmdSetViewport)                                                          \
    F(CreateDescriptorPool)                                                    \
    F(CreateDescriptorSetLayout)                                               \
    F(CreateFramebuffer)                                                       \
    F(CreateGraphicsPipelines)                                                 \
    F(CreateImageView)                                                         \
    F(CreatePipelineLayout)                                                    \
    F(CreateRenderPass)                                                        \
    F(CreateSampler)                                                           \
    F(CreateShaderModule)                                                      \
    F(DestroyDescriptorPool)                                                   \
    F(DestroyDescriptorSetLayout)                                              \
    F(DestroyFramebuffer)                                                      \
    F(DestroyImageView)                                                        \
    F(DestroyPipeline)                                                         \
    F(DestroyPipelineLayout)                                                   \
    F(DestroyRenderPass)                                                       \
    F(DestroySampler)                                                          \
    F(DestroyShaderModule)                                                     \
    F(ResetDescriptorPool)                                                     \
    F(UpdateDescriptorSets)

#define DECLARE_VULKAN_COMMAND(CMD) const PFN_vk##CMD CMD;
    RIVE_VULKAN_INSTANCE_COMMANDS(DECLARE_VULKAN_COMMAND)
    RIVE_VULKAN_DEVICE_COMMANDS(DECLARE_VULKAN_COMMAND)
#undef DECLARE_VULKAN_COMMAND

    VmaAllocator allocator() const { return m_vmaAllocator; }

    const VkPhysicalDeviceProperties& physicalDeviceProperties() const
    {
        return m_physicalDeviceProperties;
    }

    bool isFormatSupportedWithFeatureFlags(VkFormat, VkFormatFeatureFlagBits);
    bool supportsD24S8() const { return m_supportsD24S8; }

    // Resource allocation.
    rcp<vkutil::Buffer> makeBuffer(const VkBufferCreateInfo&,
                                   vkutil::Mappability);
    rcp<vkutil::Image> makeImage(const VkImageCreateInfo&, const char* name);
    rcp<vkutil::ImageView> makeImageView(rcp<vkutil::Image>, const char* name);
    rcp<vkutil::ImageView> makeImageView(rcp<vkutil::Image>,
                                         const VkImageViewCreateInfo&,
                                         const char* name);
    rcp<vkutil::ImageView> makeExternalImageView(const VkImageViewCreateInfo&,
                                                 const char* name);
    rcp<vkutil::Texture2D> makeTexture2D(const VkImageCreateInfo&,
                                         const char* name);
    rcp<vkutil::Framebuffer> makeFramebuffer(const VkFramebufferCreateInfo&);

    // Helpers.
    void updateImageDescriptorSets(
        VkDescriptorSet,
        VkWriteDescriptorSet,
        std::initializer_list<VkDescriptorImageInfo>);
    void updateBufferDescriptorSets(
        VkDescriptorSet,
        VkWriteDescriptorSet,
        std::initializer_list<VkDescriptorBufferInfo>);

    void memoryBarrier(VkCommandBuffer,
                       VkPipelineStageFlags srcStageMask,
                       VkPipelineStageFlags dstStageMask,
                       VkDependencyFlags,
                       VkMemoryBarrier);

    void imageMemoryBarriers(VkCommandBuffer,
                             VkPipelineStageFlags srcStageMask,
                             VkPipelineStageFlags dstStageMask,
                             VkDependencyFlags,
                             uint32_t count,
                             VkImageMemoryBarrier*);

    void imageMemoryBarrier(VkCommandBuffer commandBuffer,
                            VkPipelineStageFlags srcStageMask,
                            VkPipelineStageFlags dstStageMask,
                            VkDependencyFlags dependencyFlags,
                            VkImageMemoryBarrier imageMemoryBarrier)
    {
        imageMemoryBarriers(commandBuffer,
                            srcStageMask,
                            dstStageMask,
                            dependencyFlags,
                            1,
                            &imageMemoryBarrier);
    }

    const vkutil::ImageAccess& simpleImageMemoryBarrier(
        VkCommandBuffer,
        const vkutil::ImageAccess& srcAccess,
        const vkutil::ImageAccess& dstAccess,
        VkImage,
        vkutil::ImageAccessAction = vkutil::ImageAccessAction::preserveContents,
        VkDependencyFlags = 0);

    void bufferMemoryBarrier(VkCommandBuffer,
                             VkPipelineStageFlags srcStageMask,
                             VkPipelineStageFlags dstStageMask,
                             VkDependencyFlags,
                             VkBufferMemoryBarrier);

    void clearColorImage(VkCommandBuffer, ColorInt, VkImage, VkImageLayout);

    void blitSubRect(VkCommandBuffer commandBuffer,
                     VkImage srcImage,
                     VkImageLayout srcImageLayout,
                     VkImage dstImage,
                     VkImageLayout dstImageLayout,
                     const IAABB&);

    void setDebugNameIfEnabled(uint64_t handle,
                               VkObjectType objectType,
                               const char* name);

private:
    const VmaAllocator m_vmaAllocator;

    VkPhysicalDeviceProperties m_physicalDeviceProperties;

    // Vulkan spec: must support one of D24S8 and D32S8.
    bool m_supportsD24S8 = false;
};
} // namespace rive::gpu
