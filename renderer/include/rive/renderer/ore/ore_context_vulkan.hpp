/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/renderer/ore/ore_context.hpp"

#include <functional>
#include <utility>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace rive::ore
{

// Key used to cache VkRenderPass objects (one per unique format+load/store
// combination). Stored by value — small enough for linear scan.
//
// `colorHasResolve[i]` flags MSAA color attachments that should resolve
// into a single-sample target at end-of-pass. The render pass declares
// a parallel set of resolve attachments and the framebuffer must
// include the resolve image views (handled in beginRenderPass). A
// resolve attachment is meaningful only when `sampleCount > 1`; a
// single-sample render pass with `colorHasResolve[i] = true` produces
// an invalid Vulkan render pass.
struct VKRenderPassKey
{
    TextureFormat colorFormats[4] = {};
    uint32_t colorCount = 0;
    LoadOp colorLoadOps[4] = {};
    StoreOp colorStoreOps[4] = {};
    bool colorHasResolve[4] = {};
    TextureFormat depthFormat = TextureFormat::depth24plusStencil8;
    LoadOp depthLoadOp = LoadOp::dontCare;
    StoreOp depthStoreOp = StoreOp::discard;
    bool hasDepth = false;
    uint32_t sampleCount = 1;

    bool operator==(const VKRenderPassKey& o) const
    {
        if (colorCount != o.colorCount || hasDepth != o.hasDepth ||
            sampleCount != o.sampleCount)
            return false;
        for (uint32_t i = 0; i < colorCount; ++i)
        {
            if (colorFormats[i] != o.colorFormats[i] ||
                colorLoadOps[i] != o.colorLoadOps[i] ||
                colorStoreOps[i] != o.colorStoreOps[i] ||
                colorHasResolve[i] != o.colorHasResolve[i])
                return false;
        }
        if (hasDepth &&
            (depthFormat != o.depthFormat || depthLoadOp != o.depthLoadOp ||
             depthStoreOp != o.depthStoreOp))
            return false;
        return true;
    }
};

class ContextVulkan : public Context
{
public:
    // The caller is responsible for creating all Vulkan objects and the VMA
    // allocator. Ore does not manage device lifetime.
    static std::unique_ptr<ContextVulkan> Make(
        VkInstance instance,
        VkPhysicalDevice physicalDevice,
        VkDevice device,
        VkQueue queue,
        uint32_t queueFamilyIndex,
        VmaAllocator allocator,
        PFN_vkGetInstanceProcAddr pfnGetInstanceProcAddr);

    ~ContextVulkan() override;

    rcp<Buffer> makeBuffer(const BufferDesc& desc) override;
    rcp<Texture> makeTexture(const TextureDesc& desc) override;
    rcp<TextureView> makeTextureView(const TextureViewDesc& desc) override;
    rcp<Sampler> makeSampler(const SamplerDesc& desc) override;
    rcp<ShaderModule> makeShaderModule(const ShaderModuleDesc& desc) override;
    rcp<BindGroupLayout> makeBindGroupLayout(
        const BindGroupLayoutDesc& desc) override;
    rcp<Pipeline> makePipeline(const PipelineDesc& desc,
                               std::string* outError = nullptr) override;
    rcp<BindGroup> makeBindGroup(const BindGroupDesc& desc) override;

    RenderPass beginRenderPass(const RenderPassDesc& desc,
                               std::string* outError = nullptr) override;

    void beginFrame() override;
    void endFrame() override;
    void waitForGPU() override;

    rcp<TextureView> wrapCanvasTexture(gpu::RenderCanvas* canvas) override;
    rcp<TextureView> wrapRiveTexture(gpu::Texture* gpuTex,
                                     uint32_t width,
                                     uint32_t height) override;

    // External-CB mode: Ore records into the host's VkCommandBuffer (already
    // begun by the caller) instead of owning its own CB. The host retains
    // ownership of BeginCommandBuffer/EndCommandBuffer/QueueSubmit and the
    // frame fence. Contract: by the time beginFrame(externalCb) is called
    // again for a new frame, the host must have waited on the prior frame's
    // completion fence so deferred-destroy lists can be drained safely.
    // Enables cross-engine read-after-write (e.g. Rive renders into a canvas
    // then Ore samples it) that the owned-CB model cannot support, because
    // all recordings land in a single submission.
    void beginFrame(VkCommandBuffer externalCb);

    // Called by Ore resource onRefCntReachedZero() implementations. If the
    // context is currently in external-CB mode, the closure is queued to
    // run after the host has submitted and waited on the current CB. In
    // owned-CB mode the closure runs immediately.
    void vkDeferDestroy(std::function<void()> destroy);

    // Look up or create a VkRenderPass compatible with the given key.
    // Called by makePipeline() (with DONT_CARE ops) and beginRenderPass().
    VkRenderPass getOrCreateRenderPass(const VKRenderPassKey&);

    // Vulkan function dispatch table.
    // Populated by Make(); all call sites use m_vk.Xxx(...) to avoid direct
    // vkXxx() symbols which are stripped when VK_NO_PROTOTYPES is defined.
#define ORE_VK_INSTANCE_COMMANDS(F)                                            \
    F(GetDeviceProcAddr)                                                       \
    F(GetPhysicalDeviceProperties)                                             \
    F(GetPhysicalDeviceFeatures)

#define ORE_VK_DEVICE_COMMANDS(F)                                              \
    F(AllocateCommandBuffers)                                                  \
    F(AllocateDescriptorSets)                                                  \
    F(FreeDescriptorSets)                                                      \
    F(BeginCommandBuffer)                                                      \
    F(CmdBeginRenderPass)                                                      \
    F(CmdBindDescriptorSets)                                                   \
    F(CmdBindIndexBuffer)                                                      \
    F(CmdBindPipeline)                                                         \
    F(CmdBindVertexBuffers)                                                    \
    F(CmdCopyBufferToImage)                                                    \
    F(CmdDraw)                                                                 \
    F(CmdDrawIndexed)                                                          \
    F(CmdEndRenderPass)                                                        \
    F(CmdPipelineBarrier)                                                      \
    F(CmdSetBlendConstants)                                                    \
    F(CmdSetScissor)                                                           \
    F(CmdSetStencilReference)                                                  \
    F(CmdSetViewport)                                                          \
    F(CreateCommandPool)                                                       \
    F(CreateDescriptorPool)                                                    \
    F(CreateDescriptorSetLayout)                                               \
    F(CreateFramebuffer)                                                       \
    F(CreateGraphicsPipelines)                                                 \
    F(CreateImageView)                                                         \
    F(CreatePipelineLayout)                                                    \
    F(CreateRenderPass)                                                        \
    F(CreateSampler)                                                           \
    F(CreateShaderModule)                                                      \
    F(DestroyCommandPool)                                                      \
    F(DestroyDescriptorPool)                                                   \
    F(DestroyDescriptorSetLayout)                                              \
    F(DestroyFramebuffer)                                                      \
    F(DestroyImageView)                                                        \
    F(DestroyPipeline)                                                         \
    F(DestroyPipelineLayout)                                                   \
    F(DestroyRenderPass)                                                       \
    F(DestroySampler)                                                          \
    F(DestroyShaderModule)                                                     \
    F(EndCommandBuffer)                                                        \
    F(CreateFence)                                                             \
    F(DestroyFence)                                                            \
    F(WaitForFences)                                                           \
    F(ResetFences)                                                             \
    F(QueueSubmit)                                                             \
    F(QueueWaitIdle)                                                           \
    F(ResetCommandBuffer)                                                      \
    F(ResetDescriptorPool)                                                     \
    F(UpdateDescriptorSets)

    struct VkFn
    {
#define DECLARE_VK_CMD(CMD) PFN_vk##CMD CMD = nullptr;
        ORE_VK_INSTANCE_COMMANDS(DECLARE_VK_CMD)
        ORE_VK_DEVICE_COMMANDS(DECLARE_VK_CMD)
#undef DECLARE_VK_CMD
    };

    ContextVulkan(const ContextVulkan&) = delete;
    ContextVulkan& operator=(const ContextVulkan&) = delete;

private:
    friend class RenderPass;
    friend class BindGroup;
    friend class Texture;

    ContextVulkan() = default;

    VkFn m_vk;
    VkPhysicalDevice m_vkPhysicalDevice = VK_NULL_HANDLE;
    VkDevice m_vkDevice = VK_NULL_HANDLE;
    VkQueue m_vkQueue = VK_NULL_HANDLE;
    uint32_t m_vkQueueFamily = 0;
    VmaAllocator m_vmaAllocator = VK_NULL_HANDLE;
    VkCommandPool m_vkCommandPool = VK_NULL_HANDLE;
    VkCommandBuffer m_vkCommandBuffer = VK_NULL_HANDLE;
    // True while the current frame is recording into a host-provided CB.
    // endFrame() skips End/Submit/Wait when set; next beginFrame() drains
    // deferred destroys.
    bool m_vkExternalCmdBuf = false;
    VkDescriptorPool m_vkDescriptorPool = VK_NULL_HANDLE;
    VkFence m_vkFrameFence = VK_NULL_HANDLE;
    // Framebuffers whose destruction is deferred until the next beginFrame()
    // fence-wait (they are still referenced by the in-flight command buffer
    // until the fence signals).
    std::vector<VkFramebuffer> m_vkDeferredFramebuffers;
    // Staging buffers whose destruction is deferred until endFrame() submits
    // and waits on the fence. Texture::upload() records copy commands into
    // the frame command buffer, so the staging buffer must stay alive until
    // the command buffer finishes executing.
    struct DeferredVmaBuffer
    {
        VkBuffer buffer;
        VmaAllocation allocation;
    };
    std::vector<DeferredVmaBuffer> m_vkDeferredStagingBuffers;
    // Generic deferred-destroy closures (pipelines, samplers, etc.) for
    // external-CB mode, where Ore cannot vkDestroy* an object the moment its
    // refcount reaches zero because the host's CB still references it.
    std::vector<std::function<void()>> m_vkDeferredDestroys;
    // Persistent descriptor pool for long-lived BindGroup objects. Created
    // with VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT so individual
    // sets can be freed in BindGroup::onRefCntReachedZero() without affecting
    // other allocations.
    VkDescriptorPool m_vkPersistentDescriptorPool = VK_NULL_HANDLE;

    // Lazy-created shared empty `VkDescriptorSetLayout`. Substituted for null
    // entries in `PipelineDesc::bindGroupLayouts[]` so Vulkan's
    // `VkPipelineLayoutCreateInfo::pSetLayouts` array is fully populated with
    // valid handles (Vulkan spec VUID-VkPipelineLayoutCreateInfo-
    // graphicsPipelineLibrary-06753 forbids `VK_NULL_HANDLE` slots without
    // the graphicsPipelineLibrary feature). Destroyed at Context teardown via
    // `vkDeferDestroy` if needed.
    VkDescriptorSetLayout m_vkEmptyDSL = VK_NULL_HANDLE;
    VkDescriptorSetLayout vkGetOrCreateEmptyDSL();
    // Render pass cache — typically <10 entries, keyed on format+load/store
    // ops.
    std::vector<std::pair<VKRenderPassKey, VkRenderPass>> m_vkRenderPassCache;

    // Pending UNDEFINED → SHADER_READ_ONLY_OPTIMAL layout transitions for
    // textures bound as sampled images in a BindGroup before their VkImage
    // layout has been initialized by upload() or a render pass.  A render-
    // target-capable texture that is sampled without first being rendered
    // into would otherwise violate VUID-vkCmdDraw-None-09600 — the descriptor
    // declares SHADER_READ_ONLY_OPTIMAL but the image is still
    // VK_IMAGE_LAYOUT_UNDEFINED from creation.  Recorded in makeBindGroup()
    // and flushed onto the frame command buffer at beginFrame() / at the
    // start of beginRenderPass(), before any draw can reference the
    // descriptor.  Matches WebGPU's lazy-initialize-on-first-use semantic.
    // The rcp<Texture> pins the image alive until the barrier is recorded.
    struct VkPendingImageTransition
    {
        rcp<Texture> texture;
        VkImageAspectFlags aspectMask;
    };
    std::vector<VkPendingImageTransition> m_vkPendingInitialTransitions;
    void vkFlushPendingInitialTransitions();

    // Drain framebuffers/staging buffers and deferred destroy closures from
    // the previous frame. Caller is responsible for ensuring the prior GPU
    // work has completed (via our fence or the host's).
    void vkDrainDeferred();
};

} // namespace rive::ore
