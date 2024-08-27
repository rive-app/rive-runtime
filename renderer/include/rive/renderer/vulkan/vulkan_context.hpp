/*
 * Copyright 2024 Rive
 */

#pragma once

#include "rive/renderer/vulkan/vkutil.hpp"
#include <deque>

namespace rive::gpu
{
// Specifies the Vulkan API version and which relevant features have been enabled.
// The client should ensure the features get enabled if they are supported.
struct VulkanFeatures
{
    uint32_t vulkanApiVersion = VK_API_VERSION_1_0;

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
// their lifecycles via a "resource purgatory", which keeps resources alive until
// command buffers have finished using them.
//
// Provides minor helper utilities, but for the most part, the client is expected
// to make raw Vulkan calls via the provided function pointers.
class VulkanContext : public RefCnt<VulkanContext>
{
public:
    VulkanContext(VkInstance,
                  VkPhysicalDevice,
                  VkDevice,
                  const VulkanFeatures&,
                  PFN_vkGetInstanceProcAddr,
                  PFN_vkGetDeviceProcAddr);

    ~VulkanContext();

    const VkInstance instance;
    const VkPhysicalDevice physicalDevice;
    const VkDevice device;
    const VulkanFeatures features;
    const VmaAllocator vmaAllocator;

#define RIVE_VULKAN_INSTANCE_COMMANDS(F) F(GetPhysicalDeviceProperties)

#define RIVE_VULKAN_DEVICE_COMMANDS(F)                                                             \
    F(AllocateDescriptorSets)                                                                      \
    F(CmdBeginRenderPass)                                                                          \
    F(CmdBindDescriptorSets)                                                                       \
    F(CmdBindIndexBuffer)                                                                          \
    F(CmdBindPipeline)                                                                             \
    F(CmdBindVertexBuffers)                                                                        \
    F(CmdBlitImage)                                                                                \
    F(CmdClearColorImage)                                                                          \
    F(CmdCopyBufferToImage)                                                                        \
    F(CmdDraw)                                                                                     \
    F(CmdDrawIndexed)                                                                              \
    F(CmdEndRenderPass)                                                                            \
    F(CmdPipelineBarrier)                                                                          \
    F(CmdSetScissor)                                                                               \
    F(CmdSetViewport)                                                                              \
    F(CreateDescriptorPool)                                                                        \
    F(CreateDescriptorSetLayout)                                                                   \
    F(CreateFence)                                                                                 \
    F(CreateFramebuffer)                                                                           \
    F(CreateGraphicsPipelines)                                                                     \
    F(CreateImageView)                                                                             \
    F(CreatePipelineLayout)                                                                        \
    F(CreateRenderPass)                                                                            \
    F(CreateSampler)                                                                               \
    F(CreateShaderModule)                                                                          \
    F(DestroyDescriptorPool)                                                                       \
    F(DestroyDescriptorSetLayout)                                                                  \
    F(DestroyFence)                                                                                \
    F(DestroyFramebuffer)                                                                          \
    F(DestroyImageView)                                                                            \
    F(DestroyPipeline)                                                                             \
    F(DestroyPipelineLayout)                                                                       \
    F(DestroyRenderPass)                                                                           \
    F(DestroySampler)                                                                              \
    F(DestroyShaderModule)                                                                         \
    F(ResetDescriptorPool)                                                                         \
    F(ResetFences)                                                                                 \
    F(UpdateDescriptorSets)                                                                        \
    F(WaitForFences)

#define DECLARE_VULKAN_COMMAND(CMD) const PFN_vk##CMD CMD;
    RIVE_VULKAN_INSTANCE_COMMANDS(DECLARE_VULKAN_COMMAND)
    RIVE_VULKAN_DEVICE_COMMANDS(DECLARE_VULKAN_COMMAND)
#undef DECLARE_VULKAN_COMMAND

    uint64_t currentFrameIdx() const { return m_currentFrameIdx; }

    // Called at the beginning of a new frame. This is where we purge
    // m_resourcePurgatory, so the client is responsible to guarantee that all
    // command buffers from frame "N + 1 - gpu::kBufferRingSize" have finished
    // executing before calling this method.
    void onNewFrameBegun();

    // Called when a vkutil::RenderingResource has been fully released (refCnt
    // reaches 0). The resource won't actually be deleted until the current frame's
    // command buffer has finished executing.
    void onRenderingResourceReleased(const vkutil::RenderingResource* resource);

    // Called prior to the client beginning its shutdown cycle, and after all
    // command buffers from all frames have finished executing. After shutting
    // down, we delete vkutil::RenderingResources immediately instead of going
    // through m_resourcePurgatory.
    void shutdown();

    // Resource allocation.
    rcp<vkutil::Buffer> makeBuffer(const VkBufferCreateInfo&, vkutil::Mappability);
    rcp<vkutil::Texture> makeTexture(const VkImageCreateInfo&);
    rcp<vkutil::TextureView> makeTextureView(rcp<vkutil::Texture>);
    rcp<vkutil::TextureView> makeTextureView(rcp<vkutil::Texture> texture,
                                             const VkImageViewCreateInfo&);
    rcp<vkutil::TextureView> makeExternalTextureView(const VkImageUsageFlags,
                                                     const VkImageViewCreateInfo&);
    rcp<vkutil::Framebuffer> makeFramebuffer(const VkFramebufferCreateInfo&);

    // Helpers.
    void updateImageDescriptorSets(VkDescriptorSet,
                                   VkWriteDescriptorSet,
                                   std::initializer_list<VkDescriptorImageInfo>);
    void updateBufferDescriptorSets(VkDescriptorSet,
                                    VkWriteDescriptorSet,
                                    std::initializer_list<VkDescriptorBufferInfo>);
    void insertImageMemoryBarrier(VkCommandBuffer,
                                  VkImage,
                                  VkImageLayout oldLayout,
                                  VkImageLayout newLayout,
                                  uint32_t mipLevel = 0,
                                  uint32_t levelCount = 1);
    void insertBufferMemoryBarrier(VkCommandBuffer,
                                   VkAccessFlags srcAccessMask,
                                   VkAccessFlags dstAccessMask,
                                   VkBuffer,
                                   VkDeviceSize offset = 0,
                                   VkDeviceSize size = VK_WHOLE_SIZE);
    void blitSubRect(VkCommandBuffer commandBuffer, VkImage src, VkImage dst, const IAABB&);

private:
    VmaVulkanFunctions& initVmaVulkanFunctions(PFN_vkGetInstanceProcAddr fn_vkGetInstanceProcAddr,
                                               PFN_vkGetDeviceProcAddr fn_vkGetDeviceProcAddr)
    {
        return m_vmaVulkanFunctions = {
                   .vkGetInstanceProcAddr = fn_vkGetInstanceProcAddr,
                   .vkGetDeviceProcAddr = fn_vkGetDeviceProcAddr,
               };
    }

    VmaVulkanFunctions m_vmaVulkanFunctions;

    // Temporary storage for vkutil::RenderingResource instances that have been
    // fully released, but need to persist until in-flight command buffers have
    // finished referencing their underlying Vulkan objects.
    std::deque<vkutil::ZombieResource<const vkutil::RenderingResource>> m_resourcePurgatory;

    uint64_t m_currentFrameIdx = 0;
    bool m_shutdown = false; // Indicates that we are in a shutdown cycle.
};
} // namespace rive::gpu
