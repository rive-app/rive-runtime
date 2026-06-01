/*
 * Copyright 2025 Rive
 */

#pragma once

#include <cstdarg>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>
#include "rive/refcnt.hpp"
#include "rive/renderer/ore/ore_types.hpp"
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_texture.hpp"
#include "rive/renderer/ore/ore_sampler.hpp"
#include "rive/renderer/ore/ore_shader_module.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_render_pass.hpp"

namespace rive::gpu
{
class RenderCanvas;
class Texture;
} // namespace rive::gpu

namespace rive::ore
{

// RSTB asset target ID. Wire format, must match editor export. 4 is unused.
enum class ShaderTarget : uint8_t
{
    wgsl = 0,
    glsl = 1,
    msl = 2,
    hlsl = 3,
    spirv = 5,
};

// Backend-agnostic Ore graphics context.
//
// Held only via std::unique_ptr<Context>. Concrete subclasses live in
// per-backend headers and own the GPU-API state and dispatch:
//
//   ore_context_metal.hpp    → ContextMetal::Make(id<MTLDevice>,
//                                                 id<MTLCommandQueue>)
//   ore_context_gl.hpp       → ContextGL::Make()
//   ore_context_d3d11.hpp    → ContextD3D11::Make(ID3D11Device*,
//                                                 ID3D11DeviceContext*)
//   ore_context_d3d12.hpp    → ContextD3D12::Make(ID3D12Device*,
//                                                 ID3D12CommandQueue*)
//   ore_context_wgpu.hpp     → ContextWGPU::Make(wgpu::Device, wgpu::Queue,
//                                                wgpu::BackendType)
//   ore_context_vulkan.hpp   → ContextVulkan::Make(VkInstance, ...,
//   VmaAllocator,
//                                                  PFN_vkGetInstanceProcAddr)
//
// Backend-specific public methods (the typed external-CL beginFrame
// overloads, Vulkan render-pass cache lookup, WGPU GLES detection, etc.)
// live on the per-backend subclasses, not on this base — so this header
// pulls in no GPU-API headers and stays free of #ifdef branches.
//
// To call a backend-specific method, hold the subclass type at the call
// site (the subclass `Make` factory returns std::unique_ptr<Subclass>,
// which converts implicitly to std::unique_ptr<Context> for cross-backend
// use). Code that only needs the cross-backend API takes Context*.
class Context
{
public:
    virtual ~Context() = default;

    // Resource factories.
    virtual rcp<Buffer> makeBuffer(const BufferDesc& desc) = 0;
    virtual rcp<Texture> makeTexture(const TextureDesc& desc) = 0;
    virtual rcp<TextureView> makeTextureView(const TextureViewDesc& desc) = 0;
    virtual rcp<Sampler> makeSampler(const SamplerDesc& desc) = 0;
    virtual rcp<ShaderModule> makeShaderModule(
        const ShaderModuleDesc& desc) = 0;
    virtual rcp<BindGroupLayout> makeBindGroupLayout(
        const BindGroupLayoutDesc& desc) = 0;
    virtual rcp<Pipeline> makePipeline(const PipelineDesc& desc,
                                       std::string* outError = nullptr) = 0;
    virtual rcp<BindGroup> makeBindGroup(const BindGroupDesc& desc) = 0;

    virtual std::unique_ptr<RenderPass> beginRenderPass(
        const RenderPassDesc& desc,
        std::string* outError = nullptr) = 0;

    struct FrameDescriptor
    {
        // Because ore is currently imidiate mode, the command buffer must be
        // passed in on begin frame instead of end frame.
        void* externalCommandBuffer = nullptr;
        uint64_t safeFrameNumber;
        uint64_t currentFrameNumber;
    };

    virtual void beginFrame(const FrameDescriptor&) = 0;
    virtual void endFrame() = 0;

    // Block until the most recent endFrame() submission completes on the
    // GPU. Call this before destroying Ore resources (textures, views,
    // pipelines) that were used in the current frame. Not needed if
    // resources stay alive until the next beginFrame(), which waits
    // automatically.
    virtual void waitForGPU() = 0;

    virtual rcp<TextureView> wrapCanvasTexture(gpu::RenderCanvas* canvas) = 0;
    virtual rcp<TextureView> wrapRiveTexture(gpu::Texture* gpuTex,
                                             uint32_t width,
                                             uint32_t height) = 0;

    // Which RSTB shader variant this backend consumes.
    virtual ShaderTarget shaderTarget() const = 0;

    // ------------------------------------------------------------------------
    // Cross-cutting state and accessors. Non-virtual; live on this base
    // because they are uniform across backends.
    // ------------------------------------------------------------------------

    const Features& features() const { return m_features; }

    // Active render pass tracking — used by Lua bindings to auto-finish
    // stale passes and by backends that enforce one-encoder-at-a-time.
    RenderPass* activeRenderPass() const { return m_activeRenderPass; }
    void setActiveRenderPass(RenderPass* pass) { m_activeRenderPass = pass; }

    // Called at the top of every backend's beginRenderPass(). If a prior pass
    // is still open, finish it — matches the Lua binding's auto-finish
    // contract and means backends that enforce one-encoder-at-a-time (Metal,
    // D3D12) won't assert when a second beginRenderPass happens within the
    // same command buffer. Does not clear m_activeRenderPass, because the
    // pointer identity is owned by the Lua wrapper that called setActive…().
    inline void finishActiveRenderPass()
    {
        if (m_activeRenderPass && !m_activeRenderPass->isFinished())
        {
            m_activeRenderPass->finish();
        }
    }

    // Last validation error — set by setPipeline() / setBindGroup() when
    // format or layout checks fail. Cleared at beginFrame(). The Lua layer
    // reads this after native calls and fires luaL_error() if non-empty.
    const std::string& lastError() const { return m_lastError; }
    void clearLastError() { m_lastError.clear(); }

    // Populate m_lastError with a printf-style message. Used by factory
    // methods to report construction failures to the Lua layer in lieu of
    // fprintf(stderr) / assert — which either spam the console or abort
    // the process.
    void setLastError(const char* fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
        __attribute__((format(printf, 2, 3)))
#endif
    {
        va_list args;
        va_start(args, fmt);
        char buf[1024];
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        m_lastError = buf;
    }

    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    Context(Context&&) = delete;
    Context& operator=(Context&&) = delete;

protected:
    Context(rcp<rive::gpu::GPUResourceManager> manager) :
        m_manager(std::move(manager))
    {}

    Features m_features;

    // Non-null while a RenderPass created by this context is still open.
    // beginRenderPass() auto-finishes any previous open pass so backends
    // that enforce one-encoder-at-a-time (Metal, D3D12) don't assert.
    RenderPass* m_activeRenderPass = nullptr;

    // Last validation error from setPipeline() / setBindGroup().
    std::string m_lastError;

    // Back-pointer to the GPUResourceManager for GPUResource lifecycle
    // this is actually owned by the render context impl that created the given
    // ore context but its held here for convenience of ore resources that need
    // to defer cleanup until a safe frame is reached.
    rcp<rive::gpu::GPUResourceManager> m_manager;
};

// ============================================================================
// RenderPass inline helpers — defined here rather than in ore_render_pass.hpp
// because they depend on Pipeline::desc() / TextureView::texture()->format(),
// which are pulled in by this header's full set of Ore includes.
// ============================================================================

inline void RenderPass::populateAttachmentMetadata(const RenderPassDesc& desc)
{
    m_colorCount = desc.colorCount;
    for (uint32_t i = 0; i < desc.colorCount; ++i)
    {
        TextureView* view = desc.colorAttachments[i].view;
        if (!view || !view->texture())
            continue;
        m_colorFormats[i] = view->texture()->format();
        m_sampleCount = view->texture()->sampleCount();
    }
    if (desc.depthStencil.view && desc.depthStencil.view->texture())
    {
        m_depthFormat = desc.depthStencil.view->texture()->format();
        m_hasDepth = true;
        // If no colour attachments drove sampleCount, take it from depth.
        if (desc.colorCount == 0)
        {
            m_sampleCount = desc.depthStencil.view->texture()->sampleCount();
        }
    }
}

inline bool RenderPass::checkPipelineCompat(const Pipeline* pipeline) const
{
    if (!pipeline)
        return true;
    const PipelineDesc& pd = pipeline->desc();

    if (pd.colorCount != m_colorCount)
    {
        if (m_context)
            m_context->setLastError(
                "setPipeline: pipeline has %u color targets but render pass "
                "was begun with %u",
                pd.colorCount,
                m_colorCount);
        return false;
    }
    for (uint32_t i = 0; i < m_colorCount; ++i)
    {
        if (pd.colorTargets[i].format != m_colorFormats[i])
        {
            if (m_context)
                m_context->setLastError(
                    "setPipeline: color target %u format mismatch "
                    "(pipeline=%u, pass=%u)",
                    i,
                    static_cast<unsigned>(pd.colorTargets[i].format),
                    static_cast<unsigned>(m_colorFormats[i]));
            return false;
        }
    }
    if (pd.sampleCount != m_sampleCount)
    {
        if (m_context)
            m_context->setLastError(
                "setPipeline: sample count mismatch (pipeline=%u, pass=%u)",
                pd.sampleCount,
                m_sampleCount);
        return false;
    }
    // DepthStencilState::format == rgba8unorm is the "no depth" sentinel
    // (see ore_types.hpp:443). Treat that as "pipeline has no depth."
    const bool pipelineHasDepth =
        pd.depthStencil.format != TextureFormat::rgba8unorm;
    if (pipelineHasDepth != m_hasDepth)
    {
        if (m_context)
            m_context->setLastError(
                "setPipeline: depth attachment %s (pipeline=%d, pass=%d)",
                pipelineHasDepth ? "pipeline expects depth but pass has none"
                                 : "pipeline has no depth but pass provides it",
                (int)pipelineHasDepth,
                (int)m_hasDepth);
        return false;
    }
    if (pipelineHasDepth && pd.depthStencil.format != m_depthFormat)
    {
        if (m_context)
            m_context->setLastError(
                "setPipeline: depth format mismatch (pipeline=%u, pass=%u)",
                static_cast<unsigned>(pd.depthStencil.format),
                static_cast<unsigned>(m_depthFormat));
        return false;
    }
    return true;
}

} // namespace rive::ore
