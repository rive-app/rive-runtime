/*
 * Copyright 2025 Rive
 */

#pragma once

#include <vulkan/vulkan.h>
#include "rive/renderer/vulkan/vulkan_context.hpp"
#include "rive/renderer/gpu.hpp"

namespace rive::gpu
{
class DrawPipelineLayoutVulkan;
class PipelineManagerVulkan;

// Rive-specific options for configuring a render pass.
enum class RenderPassOptionsVulkan
{
    none = 0,

    // No need to attach the COLOR texture as an input attachment. There are
    // no advanced blend modes so we can use built-in hardware blending.
    fixedFunctionColorOutput = 1 << 0,

    // rasterOrdering mode only: Store all transient attachments to memory
    // so the render pass can be interrupted and restarted.
    rasterOrderingInterruptible = 1 << 1,

    // rasterOrdering mode only: This render pass is resuming after an
    // interrupt; load all transient attachments from memory.
    rasterOrderingResume = 1 << 2,

    // Atomic mode only: Use an offscreen texture to render color, but also
    // attach the real target texture at the COALESCED_ATOMIC_RESOLVE index,
    // and render to it directly in the atomic resolve step.
    atomicCoalescedResolveAndTransfer = 1 << 3,

    // MSAA only, while using LoadAction::preserveRenderTarget: We have to
    // initialize the (transient) MSAA color attachment from an offscreen
    // texture bound as an input attachment, because the final render target
    // itself can't be bound as an input attachment.
    msaaSeedFromOffscreenTexture = 1 << 4,

    // MSAA will be resolved manually in a shader instead of setting up the
    // render pass with a resolve attachment.
    msaaManualResolve = 1 << 5,
};
RIVE_MAKE_ENUM_BITSET(RenderPassOptionsVulkan);

constexpr static int RENDER_PASS_OPTION_COUNT = 6;

// This masks out RenderPassOptions that don't affect layout, allowing different
// render passes to reference the same VkPipelineLayout where possible.
// e.g., RenderPassOptions that deal with interrupting a render pass don't
// affect layout.
constexpr static RenderPassOptionsVulkan RENDER_PASS_OPTIONS_LAYOUT_MASK =
    ~(RenderPassOptionsVulkan::rasterOrderingInterruptible |
      RenderPassOptionsVulkan::rasterOrderingResume);

class RenderPassVulkan
{
public:
    constexpr static uint64_t FORMAT_BIT_COUNT = 9;
    constexpr static uint64_t LOAD_OP_BIT_COUNT = 2;
    constexpr static uint64_t KEY_NO_INTERLOCK_MODE_BIT_COUNT =
        FORMAT_BIT_COUNT + RENDER_PASS_OPTION_COUNT + LOAD_OP_BIT_COUNT;
    constexpr static uint64_t KEY_BIT_COUNT =
        KEY_NO_INTERLOCK_MODE_BIT_COUNT + gpu::INTERLOCK_MODE_BIT_COUNT;
    static_assert(KEY_BIT_COUNT <= 32);

    // Shader unique keys also include the interlock mode, so we don't always
    // need it in the render pass key.
    static uint32_t KeyNoInterlockMode(RenderPassOptionsVulkan,
                                       VkFormat renderTargetFormat,
                                       gpu::LoadAction);

    static uint32_t Key(gpu::InterlockMode,
                        RenderPassOptionsVulkan,
                        VkFormat renderTargetFormat,
                        gpu::LoadAction);

    RenderPassVulkan(PipelineManagerVulkan*,
                     gpu::InterlockMode,
                     RenderPassOptionsVulkan,
                     VkFormat renderTargetFormat,
                     gpu::LoadAction);
    ~RenderPassVulkan();

    RenderPassVulkan(const RenderPassVulkan&) = delete;
    RenderPassVulkan& operator=(const RenderPassVulkan&) = delete;

    const DrawPipelineLayoutVulkan* drawPipelineLayout() const
    {
        return m_drawPipelineLayout;
    }

    operator VkRenderPass() const { return m_renderPass; }

private:
    const rcp<VulkanContext> m_vk;
    // Raw pointer into impl->m_drawPipelineLayouts. RenderContextVulkanImpl
    // ensures the pipline layouts outlive this RenderPass instance.
    const DrawPipelineLayoutVulkan* m_drawPipelineLayout = nullptr;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
};
} // namespace rive::gpu
