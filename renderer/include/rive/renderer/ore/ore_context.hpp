/*
 * Copyright 2025 Rive
 */

#pragma once

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "rive/refcnt.hpp"
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/rive_render_image.hpp"
#include "rive/renderer/ore/ore_types.hpp"
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_texture.hpp"
#include "rive/renderer/ore/ore_sampler.hpp"
#include "rive/renderer/ore/ore_shader_module.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_render_pass.hpp"
#include "rive/renderer/ore/cmd/ore_command_buffer.hpp"

namespace rive
{
class RenderImage;
}

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
    // A pass still open has nothing left to record into, so it reads as
    // finished and its finish and destructor stay quiet.
    virtual ~Context()
    {
        for (const OpenRenderPass& open : m_openRenderPasses)
        {
            open.pass->m_finished = true;
            open.pass->m_context = nullptr;
        }
    }

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

    // Color format makeRenderCanvas allocates. A backend that allocates
    // anything other than rgba8 must override or canvas draws fail the
    // pipeline compat check at replay.
    //
    // An override also has to reach the recorder before any pass records
    // against a canvas, which a host that binds its real context late cannot
    // do. DeferredOreContext tripwires on a late bind whose override
    // disagrees with the default it already recorded.
    virtual TextureFormat canvasTargetFormat() const
    {
        return TextureFormat::rgba8unorm;
    }

    // True only for the deferred recording context. Callers that would touch
    // the driver immediately take a recording path instead.
    virtual bool isRecording() const { return false; }

    // Recording form of Image:view on a canvas backed image. Only the
    // deferred context implements this, gated by isRecording.
    virtual rcp<TextureView> recordWrapCanvasImage(RenderImage* /*image*/,
                                                   uint32_t /*width*/,
                                                   uint32_t /*height*/)
    {
        return nullptr;
    }

    // Recording form of Image:view on a decoded image. Only the deferred
    // context implements this.
    virtual rcp<TextureView> recordWrapImageView(uint32_t /*imageId*/,
                                                 uint32_t /*width*/,
                                                 uint32_t /*height*/)
    {
        return nullptr;
    }

    // Sampling wrap of a 2D canvas for Image:view. GL overrides to insert its
    // Y flip mirror.
    virtual rcp<TextureView> wrapCanvasSampleView(gpu::RenderCanvas* canvas)
    {
        auto* image = canvas->renderImage();
        return wrapRiveTexture(image->getTexture(),
                               canvas->width(),
                               canvas->height());
    }

    virtual rcp<TextureView> wrapRiveTexture(gpu::Texture* gpuTex,
                                             uint32_t width,
                                             uint32_t height) = 0;

    // Which RSTB shader variant this backend consumes.
    virtual ShaderTarget shaderTarget() const = 0;

    // Whether features() describes a device that will actually run the work.
    // Only a recording context with no replay device bound yet answers false:
    // its m_features still holds Features' own initializers, which read as a
    // real low end device and are indistinguishable from one. A caller that
    // would branch on a capability must ask this first, because a recorded
    // branch replays on the device it guessed wrong about.
    virtual bool featuresKnown() const { return true; }

    // ------------------------------------------------------------------------
    // Cross-cutting state and accessors. Non-virtual; live on this base
    // because they are uniform across backends.
    // ------------------------------------------------------------------------

    // Only meaningful when featuresKnown().
    const Features& features() const { return m_features; }

    // When on, the render pass entry point records and replays instead of
    // issuing immediately. Seeded from the RIVE_ORE_DEFER env var.
    bool deferredRecording() const { return m_deferredRecording; }
    void setDeferredRecording(bool deferred) { m_deferredRecording = deferred; }

    // True when the backend replays the accumulated pendingFrame at endFrame.
    // False falls back to per pass inline replay, which is byte identical.
    virtual bool usesDeferredFrameReplay() const { return false; }

    // Per frame stream deferred passes record into; the backend drains it at
    // endFrame.
    cmd::OreCommandBuffer& pendingFrame() { return m_pendingFrame; }

    // Recorded passes begun and not finished, outermost first. A nested pass
    // finishes on its own and moves ahead of the pass it was begun inside.
    struct OpenRenderPass
    {
        uint64_t token; // lets a script call reclaim only what it began
        RenderPass* pass;
        cmd::OreCommandBuffer* stream;
        size_t beginOffset;
    };
    // Tokens start at 1, so 0 reclaims every pass.
    uint64_t nextRenderPassToken() const { return m_nextRenderPassToken; }
    bool hasOpenRenderPasses() const { return !m_openRenderPasses.empty(); }

    // Call before the pass appends its begin.
    void beginOpenRenderPass(RenderPass* pass, cmd::OreCommandBuffer& stream)
    {
        m_openRenderPasses.push_back({m_nextRenderPassToken++,
                                      pass,
                                      &stream,
                                      stream.commandBytes().size()});
    }

    // Innermost first, before the caller records its own finish.
    void finishNestedRenderPasses(RenderPass* pass)
    {
        while (!m_openRenderPasses.empty() &&
               m_openRenderPasses.back().pass != pass)
        {
            finishInnermostRenderPass();
        }
    }

    // Call after the pass appended its finish.
    void finishOpenRenderPass(RenderPass* pass)
    {
        for (size_t i = m_openRenderPasses.size(); i-- > 0;)
        {
            if (m_openRenderPasses[i].pass != pass)
            {
                continue;
            }
            OpenRenderPass entry = m_openRenderPasses[i];
            m_openRenderPasses.erase(m_openRenderPasses.begin() + i);
            if (i > 0 && m_openRenderPasses[i - 1].stream == entry.stream)
            {
                OpenRenderPass& outer = m_openRenderPasses[i - 1];
                outer.beginOffset =
                    entry.stream->hoistNestedRenderPass(outer.beginOffset,
                                                        entry.beginOffset);
            }
            return;
        }
    }

    // Innermost first. Returns how many were finished.
    size_t finishOpenRenderPassesFrom(uint64_t token)
    {
        size_t count = 0;
        while (!m_openRenderPasses.empty() &&
               m_openRenderPasses.back().token >= token)
        {
            finishInnermostRenderPass();
            count++;
        }
        return count;
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

    // Shared by baked id, so one bind group does not cost two descriptor
    // sets on Vulkan and D3D12. Strong refs, released with the context.
    rcp<BindGroupLayout> findInternedBindGroupLayout(uint64_t layoutId) const
    {
        auto it = m_internedLayouts.find(layoutId);
        return it == m_internedLayouts.end() ? nullptr : it->second;
    }

    void internBindGroupLayout(uint64_t layoutId, rcp<BindGroupLayout> layout)
    {
        m_internedLayouts[layoutId] = std::move(layout);
    }

    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    Context(Context&&) = delete;
    Context& operator=(Context&&) = delete;

protected:
    Context(rcp<rive::gpu::GPUResourceManager> manager) :
        m_manager(std::move(manager))
    {
#ifndef NO_GETENV
        m_deferredRecording = getenv("RIVE_ORE_DEFER") != nullptr;
#endif
    }

    Features m_features;

    // A finish that fails to unregister would loop forever, so pop it.
    void finishInnermostRenderPass()
    {
        RenderPass* pass = m_openRenderPasses.back().pass;
        pass->finish();
        if (!m_openRenderPasses.empty() &&
            m_openRenderPasses.back().pass == pass)
        {
            m_openRenderPasses.pop_back();
        }
    }

    std::vector<OpenRenderPass> m_openRenderPasses;
    uint64_t m_nextRenderPassToken = 1;

    // Last validation error from setPipeline() / setBindGroup().
    std::string m_lastError;

    bool m_deferredRecording = false;

    std::unordered_map<uint64_t, rcp<BindGroupLayout>> m_internedLayouts;

    cmd::OreCommandBuffer m_pendingFrame;

    // Back-pointer to the GPUResourceManager for GPUResource lifecycle
    // this is actually owned by the render context impl that created the given
    // ore context but its held here for convenience of ore resources that need
    // to defer cleanup until a safe frame is reached.
    rcp<rive::gpu::GPUResourceManager> m_manager;
};

// The replay device's capabilities carried as data, so a recording thread can
// answer scripts truthfully without holding the device. A host with the real
// context in hand captures it with from(); a host whose device lives on
// another thread or process ships these values across once instead.
struct ReplayCaps
{
    Features features{};
    bool featuresKnown = false;
    ShaderTarget shaderTarget = ShaderTarget::glsl;
    TextureFormat canvasTargetFormat = TextureFormat::rgba8unorm;

    static ReplayCaps from(const Context& real)
    {
        ReplayCaps caps;
        caps.features = real.features();
        caps.featuresKnown = real.featuresKnown();
        caps.shaderTarget = real.shaderTarget();
        caps.canvasTargetFormat = real.canvasTargetFormat();
        return caps;
    }

    // Whether the device about to replay is the one this recording was
    // declared for. A stream recorded against other caps replays the wrong
    // shader variant and canvas format, wrong in ways replay cannot detect.
    bool matchesReplayDevice(const Context& real) const
    {
        return shaderTarget == real.shaderTarget() &&
               canvasTargetFormat == real.canvasTargetFormat();
    }
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
