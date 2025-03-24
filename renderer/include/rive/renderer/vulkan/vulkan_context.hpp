/*
 * Copyright 2024 Rive
 */

#pragma once

#include "rive/renderer/vulkan/vkutil.hpp"
#include <deque>

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

    // EXT_rasterization_order_attachment_access.
    bool rasterizationOrderColorAttachmentAccess = false;
};

// Wraps a VkDevice, function dispatch table, and VMA library instance.
//
// Provides methods to allocate vkutil::RenderingResource objects, and manages
// their lifecycles via a "resource purgatory", which keeps resources alive
// until command buffers have finished using them.
//
// Provides minor helper utilities, but for the most part, the client is
// expected to make raw Vulkan calls via the provided function pointers.
class VulkanContext : public RefCnt<VulkanContext>
{
public:
    VulkanContext(VkInstance,
                  VkPhysicalDevice,
                  VkDevice,
                  const VulkanFeatures&,
                  PFN_vkGetInstanceProcAddr);

    ~VulkanContext();

    const VkInstance instance;

#define RIVE_VULKAN_INSTANCE_COMMANDS(F)                                       \
    F(GetDeviceProcAddr)                                                       \
    F(GetPhysicalDeviceFormatProperties)                                       \
    F(GetPhysicalDeviceProperties)

#define DECLARE_VULKAN_COMMAND(CMD) const PFN_vk##CMD CMD;
    RIVE_VULKAN_INSTANCE_COMMANDS(DECLARE_VULKAN_COMMAND)

    const VkPhysicalDevice physicalDevice;
    const VkDevice device;
    const VulkanFeatures features;

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

    RIVE_VULKAN_DEVICE_COMMANDS(DECLARE_VULKAN_COMMAND)
#undef DECLARE_VULKAN_COMMAND

    const VmaAllocator vmaAllocator;

    uint64_t currentFrameIdx() const { return m_currentFrameIdx; }
    bool supportsD24S8() const { return m_supportsD24S8; }
    bool isFormatSupportedWithFeatureFlags(VkFormat, VkFormatFeatureFlagBits);

    // Called at the beginning of a new frame. This is where we purge
    // m_resourcePurgatory, so the client is responsible to guarantee that all
    // command buffers from frame "N + 1 - gpu::kBufferRingSize" have finished
    // executing before calling this method.
    void onNewFrameBegun();

    // Called when a vkutil::RenderingResource has been fully released (refCnt
    // reaches 0). The resource won't actually be deleted until the current
    // frame's command buffer has finished executing.
    void onRenderingResourceReleased(const vkutil::RenderingResource* resource);

    // Called prior to the client beginning its shutdown cycle, and after all
    // command buffers from all frames have finished executing. After shutting
    // down, we delete vkutil::RenderingResources immediately instead of going
    // through m_resourcePurgatory.
    void shutdown();

    // Resource allocation.
    rcp<vkutil::Buffer> makeBuffer(const VkBufferCreateInfo&,
                                   vkutil::Mappability);
    rcp<vkutil::Texture> makeTexture(const VkImageCreateInfo&);
    rcp<vkutil::TextureView> makeTextureView(rcp<vkutil::Texture>);
    rcp<vkutil::TextureView> makeTextureView(rcp<vkutil::Texture> texture,
                                             const VkImageViewCreateInfo&);
    rcp<vkutil::TextureView> makeExternalTextureView(
        const VkImageUsageFlags,
        const VkImageViewCreateInfo&);
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

    struct TextureAccess
    {
        VkPipelineStageFlags pipelineStages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        VkAccessFlags accessMask = VK_ACCESS_NONE;
        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    };

    const TextureAccess& simpleImageMemoryBarrier(
        VkCommandBuffer commandBuffer,
        const TextureAccess& srcAccess,
        const TextureAccess& dstAccess,
        VkImage image,
        VkDependencyFlags dependencyFlags = 0)
    {
        imageMemoryBarrier(commandBuffer,
                           srcAccess.pipelineStages,
                           dstAccess.pipelineStages,
                           dependencyFlags,
                           {
                               .srcAccessMask = srcAccess.accessMask,
                               .dstAccessMask = dstAccess.accessMask,
                               .oldLayout = srcAccess.layout,
                               .newLayout = dstAccess.layout,
                               .image = image,
                           });
        return dstAccess;
    }

    void bufferMemoryBarrier(VkCommandBuffer,
                             VkPipelineStageFlags srcStageMask,
                             VkPipelineStageFlags dstStageMask,
                             VkDependencyFlags,
                             VkBufferMemoryBarrier);

    void blitSubRect(VkCommandBuffer commandBuffer,
                     VkImage src,
                     VkImage dst,
                     const IAABB&);

private:
    // Temporary storage for vkutil::RenderingResource instances that have been
    // fully released, but need to persist until in-flight command buffers have
    // finished referencing their underlying Vulkan objects.
    std::deque<vkutil::ZombieResource<const vkutil::RenderingResource>>
        m_resourcePurgatory;

    uint64_t m_currentFrameIdx = 0;
    bool m_shutdown = false; // Indicates that we are in a shutdown cycle.

    // Vulkan spec: must support one of D24S8 and D32S8.
    bool m_supportsD24S8 = false;
};
} // namespace rive::gpu
