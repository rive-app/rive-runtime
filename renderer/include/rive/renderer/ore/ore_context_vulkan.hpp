/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/renderer/ore/ore_context.hpp"
#include "rive/renderer/vulkan/vulkan_context.hpp"
#include <functional>
#include <utility>
#include <vulkan/vulkan.h>

namespace rive::ore
{

// Refcounted VkDescriptorPool. Dies in one shot when its last ref
// (ContextVulkan or any BindGroupVulkan) is released.
class DescriptorPoolGeneration : public RefCnt<DescriptorPoolGeneration>
{
public:
    static constexpr uint32_t kMaxSetsPerGeneration = 256;

    DescriptorPoolGeneration(rcp<rive::gpu::VulkanContext> vk);
    ~DescriptorPoolGeneration();

    // Returns VK_NULL_HANDLE if this generation is full.
    VkDescriptorSet tryAllocate(VkDescriptorSetLayout dsl);

    bool hasCapacity() const { return m_setsAllocated < kMaxSetsPerGeneration; }

private:
    rcp<rive::gpu::VulkanContext> m_vk;
    VkDescriptorPool m_vkPool = VK_NULL_HANDLE;
    uint32_t m_setsAllocated = 0;
};

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
        const rcp<rive::gpu::VulkanContext> vk);

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

    std::unique_ptr<RenderPass> beginRenderPass(
        const RenderPassDesc& desc,
        std::string* outError = nullptr) override;

    void beginFrame(const FrameDescriptor&) override;
    void endFrame() override;
    void waitForGPU() override;

    rcp<TextureView> wrapCanvasTexture(gpu::RenderCanvas* canvas) override;
    rcp<TextureView> wrapRiveTexture(gpu::Texture* gpuTex,
                                     uint32_t width,
                                     uint32_t height) override;

    ShaderTarget shaderTarget() const override { return ShaderTarget::spirv; }

    VkRenderPass getOrCreateRenderPass(const VKRenderPassKey&);

    // Device-specific VkFormat for ambiguous TextureFormat values (e.g.
    // depth24plusStencil8 picks D24S8 or D32S8 based on probe). Mirrors Dawn.
    VkFormat vkFormatFor(TextureFormat fmt) const;

    ContextVulkan(const ContextVulkan&) = delete;
    ContextVulkan& operator=(const ContextVulkan&) = delete;

private:
    friend class RenderPassVulkan;
    friend class BindGroupVulkan;
    friend class TextureVulkan;

    ContextVulkan(const rcp<rive::gpu::VulkanContext> vk) :
        Context(vk), m_vk(vk)
    {}

    const rcp<rive::gpu::VulkanContext> m_vk;

    VkQueue m_vkQueue = VK_NULL_HANDLE;
    uint32_t m_vkQueueFamily = 0;
    // Probed at Make() time. UNDEFINED until then so a stale read fails loudly.
    VkFormat m_vkDepth24Stencil8Format = VK_FORMAT_UNDEFINED;
    VkCommandPool m_vkCommandPool = VK_NULL_HANDLE;
    VkCommandBuffer m_vkCommandBuffer = VK_NULL_HANDLE;

    VkDescriptorPool m_vkDescriptorPool = VK_NULL_HANDLE;
    VkFence m_vkFrameFence = VK_NULL_HANDLE;
    // True between BeginCommandBuffer and EndCommandBuffer. Gates the
    // auto-reopen path; orphan recordings get flushed at next beginFrame.
    bool m_vkCmdBufRecording = false;
    // Active generation. makeBindGroup rolls a new one when full.
    rcp<DescriptorPoolGeneration> m_currentDescriptorPool;

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

    // Pending image-layout transitions to be emitted before the next render
    // pass / draw.  Recorded in makeBindGroup() (each sampled view needs the
    // image in SHADER_READ_ONLY_OPTIMAL at draw time) and in any path that
    // leaves a texture in a non-default layout (initial UNDEFINED creation,
    // depth/color attachments after a previous pass, etc.).  Flushed at
    // beginFrame() and at the start of beginRenderPass() via a single
    // vkCmdPipelineBarrier — barriers inside a render pass are restricted
    // to declared self-dependencies, so any cross-pass transitions must be
    // emitted between passes.  Mirrors Dawn's PrepareResourcesForSyncScope
    // (src/dawn/native/vulkan/CommandBufferVk.cpp:444):  per-subresource
    // last-known layout is read off the Texture, compared against the
    // required usage at descriptor-write time, and a transition is queued
    // only if the layouts differ.  The rcp<Texture> pins the image alive
    // until the barrier is recorded.
    struct VkPendingImageTransition
    {
        rcp<Texture> texture;
        VkImageAspectFlags aspectMask;
        VkImageLayout oldLayout;
        VkImageLayout newLayout;
    };
    std::vector<VkPendingImageTransition> m_vkPendingInitialTransitions;
    void vkFlushPendingInitialTransitions();
    // Queue a transition for `texture` from its current m_vkLayout to
    // `newLayout` (typically SHADER_READ_ONLY_OPTIMAL for sampled use).
    // No-op when the texture is already in the target layout.  Safe to
    // call outside any active render pass; the transition is emitted by
    // the next vkFlushPendingInitialTransitions().
    void vkQueueTransitionToLayout(Texture* texture,
                                   VkImageAspectFlags aspectMask,
                                   VkImageLayout newLayout);

    // Deferred texture uploads: upload() can be called with no recording
    // CB (verify hooks, scripted shader setup). Drained on the host CB
    // at the next beginFrame / beginRenderPass.
    struct VkPendingTextureUpload
    {
        // Base types so this header doesn't need TextureVulkan / BufferVulkan.
        rcp<Texture> texture;
        rcp<Buffer> stagingBuffer;
        VkBufferImageCopy region;
        VkImageAspectFlags aspectMask;
    };
    std::vector<VkPendingTextureUpload> m_vkPendingTextureUploads;
    void vkQueuePendingTextureUpload(VkPendingTextureUpload pending);
    void vkFlushPendingTextureUploads();
};

} // namespace rive::ore
