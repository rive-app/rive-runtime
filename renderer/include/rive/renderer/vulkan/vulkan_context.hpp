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
    uint32_t apiVersion = VK_API_VERSION_1_1;

    // VkPhysicalDeviceFeatures.
    bool independentBlend = false;
    bool fillModeNonSolid = false;
    bool fragmentStoresAndAtomics = false;
    bool shaderClipDistance = false;

    // EXT_rasterization_order_attachment_access.
    bool rasterizationOrderColorAttachmentAccess = false;

    // VK_EXT_fragment_shader_interlock.
    bool fragmentShaderPixelInterlock = false;

    // VK_EXT_color_write_enable.
    bool colorWriteEnable = false;

    // Indicates a nonconformant driver, like MoltenVK.
    bool VK_KHR_portability_subset = false;

    // VkPhysicalDeviceFeatures – texture compression.
    bool textureCompressionBC = false;       // BC1/BC2/BC3/BC7
    bool textureCompressionASTC_LDR = false; // ASTC LDR
    bool textureCompressionETC2 = false;     // ETC2
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
    // Returns null if the driver can't give us a memory allocator. Preferred
    // over the constructor, which can only abort on that failure.
    static rcp<VulkanContext> make(VkInstance,
                                   VkPhysicalDevice,
                                   VkDevice,
                                   const VulkanFeatures&,
                                   PFN_vkGetInstanceProcAddr);

    // Takes ownership of 'vmaAllocator'. A null one means "make me one", which
    // aborts if the driver can't, having nowhere to report it.
    VulkanContext(VkInstance,
                  VkPhysicalDevice,
                  VkDevice,
                  const VulkanFeatures&,
                  PFN_vkGetInstanceProcAddr,
                  VmaAllocator vmaAllocator = VK_NULL_HANDLE);

    ~VulkanContext();

    const VkInstance instance;
    const VkPhysicalDevice physicalDevice;
    const VkDevice device;

#define RIVE_VULKAN_INSTANCE_COMMANDS(F)                                       \
    F(GetDeviceProcAddr)                                                       \
    F(GetPhysicalDeviceFormatProperties)                                       \
    F(GetPhysicalDeviceProperties)                                             \
    F(GetPhysicalDeviceFeatures)                                               \
    F(SetDebugUtilsObjectNameEXT)

#define RIVE_VULKAN_DEVICE_COMMANDS(F)                                         \
    F(AllocateCommandBuffers)                                                  \
    F(AllocateDescriptorSets)                                                  \
    F(BeginCommandBuffer)                                                      \
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
    F(CmdPushConstants)                                                        \
    F(CmdSetBlendConstants)                                                    \
    F(CmdSetColorWriteEnableEXT)                                               \
    F(CmdSetCullMode)                                                          \
    F(CmdSetDepthWriteEnable)                                                  \
    F(CmdSetScissor)                                                           \
    F(CmdSetStencilCompareMask)                                                \
    F(CmdSetStencilOp)                                                         \
    F(CmdSetStencilReference)                                                  \
    F(CmdSetStencilWriteMask)                                                  \
    F(CmdSetViewport)                                                          \
    F(CreateCommandPool)                                                       \
    F(CreateDescriptorPool)                                                    \
    F(CreateDescriptorSetLayout)                                               \
    F(CreateFramebuffer)                                                       \
    F(CreateFence)                                                             \
    F(CreateGraphicsPipelines)                                                 \
    F(CreateImageView)                                                         \
    F(CreatePipelineLayout)                                                    \
    F(CreateRenderPass)                                                        \
    F(CreateSampler)                                                           \
    F(CreateShaderModule)                                                      \
    F(DestroyCommandPool)                                                      \
    F(DestroyDescriptorPool)                                                   \
    F(DestroyDescriptorSetLayout)                                              \
    F(DestroyFence)                                                            \
    F(DestroyFramebuffer)                                                      \
    F(DestroyImageView)                                                        \
    F(DestroyPipeline)                                                         \
    F(DestroyPipelineLayout)                                                   \
    F(DestroyRenderPass)                                                       \
    F(DestroySampler)                                                          \
    F(DestroyShaderModule)                                                     \
    F(EndCommandBuffer)                                                        \
    F(FreeCommandBuffers)                                                      \
    F(FreeDescriptorSets)                                                      \
    F(QueueSubmit)                                                             \
    F(QueueWaitIdle)                                                           \
    F(ResetCommandBuffer)                                                      \
    F(ResetDescriptorPool)                                                     \
    F(ResetFences)                                                             \
    F(UpdateDescriptorSets)                                                    \
    F(WaitForFences)

#define DECLARE_VULKAN_COMMAND(CMD) const PFN_vk##CMD CMD;
    RIVE_VULKAN_INSTANCE_COMMANDS(DECLARE_VULKAN_COMMAND)
    RIVE_VULKAN_DEVICE_COMMANDS(DECLARE_VULKAN_COMMAND)
#undef DECLARE_VULKAN_COMMAND

    const VkPhysicalDeviceProperties physicalDeviceProperties;
    const VulkanFeatures features;

    VmaAllocator allocator() const { return m_vmaAllocator; }

    bool isFormatSupportedWithFeatureFlags(VkFormat, VkFormatFeatureFlagBits);
    bool supportsD24S8() const { return m_supportsD24S8; }

    // Bumped whenever a vkutil resource fails to allocate. "Init" tries to
    // check each handle it is about to use directly; this is the backstop for
    // the ones it never names, like the staging buffer inside
    // Texture2D::scheduleUpload().
    uint32_t allocationFailureCount() const { return m_allocationFailureCount; }

    // Called by vkutil once a failed allocation's diagnostic has been printed.
    // Aborts by default, since steady-state rendering has nowhere to report the
    // failure; see AllocationFailureScope.
    void reportAllocationFailure()
    {
        ++m_allocationFailureCount;
        if (m_abortsOnAllocationFailure)
        {
            abort();
        }
    }

    // Makes allocation failures recoverable for as long as it is in scope,
    // counting them instead of aborting. "Init" opens one of these since it can
    // fall back on another backend on failure.
    class AllocationFailureScope
    {
    public:
        AllocationFailureScope(VulkanContext* vk) :
            m_vk(vk), m_baseline(vk->m_allocationFailureCount)
        {
            // Nesting would restore the wrong state on the inner scope's exit.
            assert(m_vk->m_abortsOnAllocationFailure);
            m_vk->m_abortsOnAllocationFailure = false;
        }

        AllocationFailureScope(const AllocationFailureScope&) = delete;
        AllocationFailureScope& operator=(const AllocationFailureScope&) =
            delete;

        ~AllocationFailureScope()
        {
            // Catches a nested scope having already restored the flag, and
            // anything else that flipped it behind our back.
            assert(!m_vk->m_abortsOnAllocationFailure);
            m_vk->m_abortsOnAllocationFailure = true;
        }

        // Whether any allocation has failed since this scope began.
        bool anyFailed() const
        {
            return m_vk->m_allocationFailureCount != m_baseline;
        }

    private:
        VulkanContext* const m_vk;
        const uint32_t m_baseline;
    };

    template <typename PFN_vkCreate, typename CreateInfo>
    typename vkutil::CreatedHandle<PFN_vkCreate>::type createHandle(
        const PFN_vkCreate VulkanContext::* vkCreate,
        const CreateInfo* createInfo,
        const char* file,
        int line)
    {
        typename vkutil::CreatedHandle<PFN_vkCreate>::type handle;
        if (!vkutil::vkReportError(
                (this->*vkCreate)(device, createInfo, nullptr, &handle),
                file,
                line))
        {
            reportAllocationFailure();
            // vkCreate*() doesn't define its out parameter when it fails, so
            // drop whatever it may have written.
            return VK_NULL_HANDLE;
        }
        return handle;
    }

    // Resource allocation.
    rcp<vkutil::Buffer> makeBuffer(const VkBufferCreateInfo&,
                                   vkutil::Mappability);
    rcp<vkutil::Image> makeImage(const VkImageCreateInfo&, const char* name);
    // Adopts an externally-owned VkImage; does not free it on destruction.
    rcp<vkutil::Image> makeExternalImage(VkImage existingImage,
                                         const VkImageCreateInfo&,
                                         const char* name);
    rcp<vkutil::ImageView> makeImageView(rcp<vkutil::Image>, const char* name);
    rcp<vkutil::ImageView> makeImageView(rcp<vkutil::Image>,
                                         const VkImageViewCreateInfo&,
                                         const char* name);
    rcp<vkutil::ImageView> makeExternalImageView(const VkImageViewCreateInfo&,
                                                 const char* name);
    rcp<vkutil::Texture2D> makeTexture2D(const VkImageCreateInfo&,
                                         const char* name);
    // Builds a Texture2D over a pre-allocated (typically external) Image.
    rcp<vkutil::Texture2D> makeTexture2D(rcp<vkutil::Image> existingImage,
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

    // Vulkan spec: must support one of D24S8 and D32S8.
    bool m_supportsD24S8 = false;

    uint32_t m_allocationFailureCount = 0;
    bool m_abortsOnAllocationFailure = true;
};
} // namespace rive::gpu
