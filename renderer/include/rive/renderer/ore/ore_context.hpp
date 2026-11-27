/*
 * Copyright 2025 Rive
 */

#pragma once

#include <algorithm>
#include <atomic>
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
#include "rive/renderer/ore/ore_compute_pass.hpp"
#include "rive/renderer/ore/cmd/ore_command_buffer.hpp"

namespace rive
{
class RenderImage;
}

namespace rive::gpu
{
class RenderCanvas;
class RenderTarget;
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
    for (const OpenPass& open : m_openPasses)
    {
      open.pass->m_finished = true;
      open.pass->m_context = nullptr;
    }
  }

  // Resource factories.
  virtual rcp<Buffer> makeBuffer(const BufferDesc& desc) = 0;
  virtual rcp<Texture> makeTexture(const TextureDesc& desc) = 0;
  // Zero counts span the rest of the texture. Ranges past the texture are
  // rejected here so scripts get the error at record time on every backend.
  rcp<TextureView> makeTextureView(TextureViewDesc desc)
  {
    Texture* tex = desc.texture;
    if (tex == nullptr)
    {
      setLastError("makeTextureView: texture is null");
      return nullptr;
    }
    uint32_t mips = tex->numMipmaps();
    uint32_t layers = tex->arrayLayers();
    if (desc.baseMipLevel >= mips)
    {
      setLastError(
          "makeTextureView: baseMipLevel %u exceeds %u levels",
          desc.baseMipLevel,
          mips);
      return nullptr;
    }
    if (desc.baseLayer >= layers)
    {
      setLastError(
          "makeTextureView: baseLayer %u exceeds %u layers",
          desc.baseLayer,
          layers);
      return nullptr;
    }
    if (desc.mipCount == 0)
    {
      desc.mipCount = mips - desc.baseMipLevel;
    }
    if (desc.layerCount == 0)
    {
      desc.layerCount = layers - desc.baseLayer;
    }
    if (desc.mipCount > mips - desc.baseMipLevel)
    {
      setLastError(
          "makeTextureView: mip range [%u, %u) exceeds %u levels",
          desc.baseMipLevel,
          desc.baseMipLevel + desc.mipCount,
          mips);
      return nullptr;
    }
    if (desc.layerCount > layers - desc.baseLayer)
    {
      setLastError(
          "makeTextureView: layer range [%u, %u) exceeds %u layers",
          desc.baseLayer,
          desc.baseLayer + desc.layerCount,
          layers);
      return nullptr;
    }
    return makeTextureViewImpl(desc);
  }
  virtual rcp<Sampler> makeSampler(const SamplerDesc& desc) = 0;
  virtual rcp<ShaderModule> makeShaderModule(const ShaderModuleDesc& desc) = 0;
  virtual rcp<BindGroupLayout> makeBindGroupLayout(
      const BindGroupLayoutDesc& desc) = 0;
  virtual rcp<Pipeline> makePipeline(
      const PipelineDesc& desc,
      std::string* outError = nullptr) = 0;
  virtual rcp<BindGroup> makeBindGroup(const BindGroupDesc& desc) = 0;

  virtual std::unique_ptr<RenderPass> beginRenderPass(
      const RenderPassDesc& desc,
      std::string* outError = nullptr) = 0;

  // Compute, where features().computeShaders. `desc` is checked here, so a
  // backend only builds.
  rcp<ComputePipeline> makeComputePipeline(
      const ComputePipelineDesc& desc,
      std::string* outError = nullptr)
  {
    std::string error;
    if (!validateComputePipelineDesc(this, desc, &error))
    {
      refuse("makeComputePipeline", error.c_str(), outError);
      return nullptr;
    }
    return makeComputePipelineImpl(desc, outError);
  }

  virtual std::unique_ptr<ComputePass> beginComputePass(
      const ComputePassDesc& /*desc*/,
      std::string* outError = nullptr)
  {
    refuse("beginComputePass", kNoCompute, outError);
    return nullptr;
  }

  // Receives a desc whose ranges are already checked and non-zero.
  virtual rcp<TextureView> makeTextureViewImpl(const TextureViewDesc& desc) = 0;

  // Receives a desc validateComputePipelineDesc accepted.
  virtual rcp<ComputePipeline> makeComputePipelineImpl(
      const ComputePipelineDesc& /*desc*/,
      std::string* outError)
  {
    refuse("makeComputePipeline", kNoCompute, outError);
    return nullptr;
  }

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

  // What a script sees of the host's render target; it builds its
  // pipelines against these. Zero width means no target is exposed.
  struct TargetDesc
  {
    uint32_t width = 0;
    uint32_t height = 0;
    TextureFormat format = TextureFormat::rgba8unorm;
    uint32_t sampleCount = 1;

    bool operator==(const TargetDesc& o) const
    {
      return width == o.width && height == o.height && format == o.format &&
             sampleCount == o.sampleCount;
    }
    bool operator!=(const TargetDesc& o) const { return !(*this == o); }

    // The single sampled 8 bit target a window host exposes.
    static TargetDesc color8(uint32_t width, uint32_t height, bool bgra)
    {
      return {
          width,
          height,
          bgra ? TextureFormat::bgra8unorm : TextureFormat::rgba8unorm,
          1};
    }
  };

  // Backends that cannot render into a host's own render target return
  // null, which drops the passes into it.
  virtual rcp<TextureView> wrapRenderTarget(gpu::RenderTarget*)
  {
    return nullptr;
  }

  // The host render target scripts draw into under Rive's content. Only the
  // recording context hands one out, wrapped again each frame at replay.
  virtual rcp<TextureView> targetView() { return nullptr; }

  // False where the host can never expose its target, so scripts get nil
  // and keep a gpuCanvas instead.
  virtual bool exposesTarget() const { return false; }

  // Set by the host around each Ore frame it replays.
  void setRenderTarget(gpu::RenderTarget* target) { m_renderTarget = target; }
  gpu::RenderTarget* renderTarget() const { return m_renderTarget; }

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
  virtual rcp<TextureView> recordWrapCanvasImage(
      gpu::RenderCanvas* /*canvas*/,
      uint32_t /*width*/,
      uint32_t /*height*/)
  {
    return nullptr;
  }

  // Recording form of Image:view on a decoded image. Only the deferred
  // context implements this.
  virtual rcp<TextureView> recordWrapImageView(
      uint32_t /*imageId*/,
      uint32_t /*width*/,
      uint32_t /*height*/)
  {
    return nullptr;
  }

  // Recording form of Image:view on a foreign image — one this recorder did
  // not create, such as a file asset decoded through the immediate factory.
  // It has no mint id, so the registry assigns one and the consumer resolves
  // it there at replay. Only the deferred context implements this.
  virtual rcp<TextureView> recordWrapForeignImageView(
      RenderImage* /*image*/,
      uint32_t /*width*/,
      uint32_t /*height*/)
  {
    return nullptr;
  }

  // Sampling wrap of a 2D canvas for Image:view.
  virtual rcp<TextureView> wrapCanvasSampleView(gpu::RenderCanvas* canvas)
  {
    auto* image = canvas->renderImage();
    return wrapRiveTexture(
        image->getTexture(),
        canvas->width(),
        canvas->height());
  }

  virtual rcp<TextureView> wrapRiveTexture(
      gpu::Texture* gpuTex,
      uint32_t width,
      uint32_t height) = 0;

  // Which RSTB shader variant this backend consumes.
  virtual ShaderTarget shaderTarget() const = 0;

  // Whether makeShaderModule accepts RSTB bytes that belong to no file asset,
  // such as the shaders the runtime embeds for its own passes.
  virtual bool runsEmbeddedShaders() const { return true; }

  // Whether features() describes a device that will actually run the work.
  // Only a recording context with no replay device bound yet answers false:
  // its m_features still holds Features' own initializers, which read as a
  // real low end device and are indistinguishable from one. A caller that
  // would branch on a capability must ask this first, because a recorded
  // branch replays on the device it guessed wrong about.
  virtual bool featuresKnown() const { return true; }

  // Whether makes with deferBuildCheck can return before the driver is done.
  virtual bool defersBuildChecks() const { return false; }

  // Settles a deferBuildCheck pipeline, blocking on the driver if wait.
  virtual BuildStatus pipelineStatus(
      Pipeline* /*pipeline*/,
      bool /*wait*/,
      std::string* /*outError*/)
  {
    return BuildStatus::ready;
  }

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

  // Per pass GPU timing, off by default and free while off. Seeded from the
  // RIVE_ORE_GPU_PROFILE env var. Only Vulkan measures so far; the other
  // backends never publish rows.
  bool gpuProfiling() const
  {
    return m_gpuProfiling.load(std::memory_order_relaxed);
  }
  void setGpuProfiling(bool on)
  {
    m_gpuProfiling.store(on, std::memory_order_relaxed);
  }

  // True when the backend replays the accumulated pendingFrame at endFrame.
  // False falls back to per pass inline replay, which is byte identical.
  virtual bool usesDeferredFrameReplay() const { return false; }

  // Per frame stream deferred passes record into; the backend drains it at
  // endFrame.
  cmd::OreCommandBuffer& pendingFrame() { return m_pendingFrame; }

  // Recorded passes begun and not finished, outermost first. A nested pass
  // finishes on its own and moves ahead of the pass around it on its stream.
  struct OpenPass
  {
    uint64_t token; // lets a script call reclaim only what it began
    Pass* pass;
    cmd::OreCommandBuffer* stream;
    size_t beginOffset;
    // Storage buffers its groups bound, held from an update until it finishes.
    std::vector<rcp<Buffer>> storage;

    bool binds(const Buffer* buffer) const
    {
      for (const rcp<Buffer>& bound : storage)
      {
        if (bound.get() == buffer)
        {
          return true;
        }
      }
      return false;
    }
  };
  // Tokens start at 1, so 0 reclaims every pass.
  uint64_t nextPassToken() const { return m_nextPassToken; }
  bool hasOpenPasses() const { return !m_openPasses.empty(); }

  // Call before the pass appends its begin.
  void beginOpenPass(Pass* pass, cmd::OreCommandBuffer& stream)
  {
    m_openPasses.push_back(
        {m_nextPassToken++, pass, &stream, stream.commandBytes().size(), {}});
  }

  // Call when open `pass` binds `group`.
  void noteBoundStorage(const Pass* pass, const BindGroup* group)
  {
    for (size_t i = m_openPasses.size(); i-- > 0;)
    {
      OpenPass& open = m_openPasses[i];
      if (open.pass != pass)
      {
        continue;
      }
      for (const rcp<Buffer>& buffer : group->m_retainedBuffers)
      {
        if (buffer->usage() == BufferUsage::storage &&
            !open.binds(buffer.get()))
        {
          open.storage.push_back(buffer);
        }
      }
      return;
    }
  }

  // No pass can take a storage copy inside: moves the one `stream` recorded
  // at `offset`, of buffer `handle`, ahead of the innermost pass open there.
  void hoistAheadOfOpenPass(
      cmd::OreCommandBuffer& stream,
      size_t offset,
      cmd::ResourceHandle handle)
  {
    for (size_t i = m_openPasses.size(); i-- > 0;)
    {
      OpenPass& open = m_openPasses[i];
      if (open.stream != &stream)
      {
        continue;
      }
      // With its buffer made inside the pass, the copy takes what a nested
      // pass would: every command there but the pass's own, in order. The
      // passes around take the copy ahead as this one finishes.
      open.beginOffset = stream.makesBuffer(open.beginOffset, offset, handle)
                             ? stream.hoistNestedPass(open.beginOffset, offset)
                             : open.beginOffset + stream.moveTailAhead(
                                                      open.beginOffset,
                                                      offset);
      return;
    }
  }

  // Innermost first, before the caller records its own finish. With
  // `stream`, from the first one inside `pass` that records there, since a
  // command would land in its range.
  void finishNestedPasses(
      Pass* pass,
      const cmd::OreCommandBuffer* stream = nullptr)
  {
    size_t first = m_openPasses.size();
    for (size_t i = m_openPasses.size();
         i-- > 0 && m_openPasses[i].pass != pass;)
    {
      if (stream == nullptr || m_openPasses[i].stream == stream)
      {
        first = i;
      }
    }
    while (m_openPasses.size() > first)
    {
      finishInnermostPass();
    }
  }

  // Call after the pass appended its finish.
  void finishOpenPass(Pass* pass)
  {
    for (size_t i = m_openPasses.size(); i-- > 0;)
    {
      if (m_openPasses[i].pass != pass)
      {
        continue;
      }
      OpenPass entry = m_openPasses[i];
      m_openPasses.erase(m_openPasses.begin() + i);
      // The pass around it on its stream, past any on other streams.
      for (size_t j = i; j-- > 0;)
      {
        OpenPass& outer = m_openPasses[j];
        if (outer.stream == entry.stream)
        {
          outer.beginOffset = entry.stream->hoistNestedPass(
              outer.beginOffset,
              entry.beginOffset);
          break;
        }
      }
      return;
    }
  }

  // Innermost first. Returns how many were finished.
  size_t finishOpenPassesFrom(uint64_t token)
  {
    size_t count = 0;
    while (!m_openPasses.empty() && m_openPasses.back().token >= token)
    {
      finishInnermostPass();
      count++;
    }
    return count;
  }

  // True, with the last error set, while an open pass binds `buffer`: its
  // update runs ahead of the open passes, out of order for one that uses it.
  bool refusesStorageUpdate(const Buffer* buffer)
  {
    for (const OpenPass& open : m_openPasses)
    {
      if (open.binds(buffer))
      {
        setLastError("Buffer.update: a storage buffer cannot be updated "
                     "while a pass that binds it is open");
        return true;
      }
    }
    return false;
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
    // Sized to the message: shader compiler logs overflow any fixed
    // buffer, and a truncated error is what the author reads.
    va_list args;
    va_start(args, fmt);
    va_list sizing;
    va_copy(sizing, args);
    int size = vsnprintf(nullptr, 0, fmt, sizing);
    va_end(sizing);
    if (size < 0)
    {
      va_end(args);
      m_lastError = fmt;
      return;
    }
    std::vector<char> buf(static_cast<size_t>(size) + 1);
    vsnprintf(buf.data(), buf.size(), fmt, args);
    va_end(args);
    m_lastError.assign(buf.data(), static_cast<size_t>(size));
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
  Context(rcp<rive::gpu::GPUResourceManager> manager)
      : m_manager(std::move(manager))
  {
#ifndef NO_GETENV
    m_deferredRecording = getenv("RIVE_ORE_DEFER") != nullptr;
    m_gpuProfiling.store(
        getenv("RIVE_ORE_GPU_PROFILE") != nullptr,
        std::memory_order_relaxed);
#endif
  }

  Features m_features;

  static constexpr const char*
      kNoCompute = "this backend does not run compute shaders";

  // True, with the last error set, for a buffer this backend has yet to
  // implement.
  bool refusesBuffer(const BufferDesc& desc)
  {
    if (desc.usage != BufferUsage::storage)
      return false;
    setLastError("makeBuffer: this backend has no storage buffers");
    return true;
  }

  // To the caller when it asked for the text, else the last error.
  void refuse(const char* call, const char* why, std::string* outError)
  {
    if (outError != nullptr)
      *outError = why;
    else
      setLastError("%s: %s", call, why);
  }

  struct GpuPassTiming
  {
    std::string label;
    double milliseconds;
  };

  // The pass's label, or its attachment shape when the script gave none.
  static std::string gpuPassLabel(const RenderPassDesc& desc);

  // Backends hand in each resolved frame's passes here, in any order and
  // with repeated labels. Prints the per label average once every
  // kGpuProfileReportFrames frames.
  static constexpr uint32_t kGpuProfileReportFrames = 120;
  void publishGpuPassTimings(const std::vector<GpuPassTiming>& rows);

  // A finish that fails to unregister would loop forever, so pop it.
  void finishInnermostPass()
  {
    Pass* pass = m_openPasses.back().pass;
    pass->finish();
    if (!m_openPasses.empty() && m_openPasses.back().pass == pass)
    {
      m_openPasses.pop_back();
    }
  }

  std::vector<OpenPass> m_openPasses;
  uint64_t m_nextPassToken = 1;

  // Last validation error from setPipeline() / setBindGroup().
  std::string m_lastError;

  bool m_deferredRecording = false;

  gpu::RenderTarget* m_renderTarget = nullptr;
  // Replay reads it on a worker while the host may toggle it.
  std::atomic<bool> m_gpuProfiling{false};
  std::unordered_map<std::string, double> m_gpuProfileTotals;
  uint32_t m_gpuProfileFrames = 0;

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
  bool embeddedShaders = true;

  static ReplayCaps from(const Context& real)
  {
    ReplayCaps caps;
    caps.features = real.features();
    caps.featuresKnown = real.featuresKnown();
    caps.shaderTarget = real.shaderTarget();
    caps.canvasTargetFormat = real.canvasTargetFormat();
    caps.embeddedShaders = real.runsEmbeddedShaders();
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

inline void RenderPass::storeBindGroup(
    uint32_t groupIndex,
    BindGroup* group,
    const uint32_t* offsets,
    uint32_t offsetCount)
{
  m_boundGroups[groupIndex] = ref_rcp(group);
  const uint32_t count = std::min(offsetCount, kMaxDynamicOffsets);
  m_groupOffsetCounts[groupIndex] = count;
  std::copy_n(offsets, count, m_groupOffsets[groupIndex]);
  m_unappliedGroups |= static_cast<uint8_t>(1u << groupIndex);
}

inline uint8_t RenderPass::movedGroups(
    const Pipeline* previous,
    const Pipeline* next) const
{
  uint8_t moved = 0;
  if (previous == next)
    return moved;
  for (uint32_t g = 0; g < kMaxBindGroups; ++g)
  {
    if (m_boundGroups[g] != nullptr &&
        (previous == nullptr ||
         previous->m_groupSlots[g] != next->m_groupSlots[g]))
      moved |= static_cast<uint8_t>(1u << g);
  }
  return moved;
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
  const bool pipelineHasDepth = pd.depthStencil.format !=
                                TextureFormat::rgba8unorm;
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

inline void ComputePass::setPipeline(ComputePipeline* pipeline)
{
  if (m_finished || pipeline == nullptr)
    return;
  m_pipeline = ref_rcp(pipeline);
  applyPipeline(pipeline);
}

inline void ComputePass::dispatchWorkgroups(uint32_t x, uint32_t y, uint32_t z)
{
  if (m_finished || m_context == nullptr)
    return;
  std::string error;
  if (m_pipeline == nullptr)
    error = "no pipeline is set";
  else
    validateDispatch(m_context, x, y, z, &error);
  if (!error.empty())
  {
    m_context->setLastError("dispatchWorkgroups: %s", error.c_str());
    return;
  }
  // A zero count runs nothing on WebGPU, and Metal refuses it.
  if (x != 0 && y != 0 && z != 0)
    dispatch(x, y, z);
}

inline void ComputePass::finish()
{
  if (m_finished)
    return;
  m_finished = true;
  end();
  m_pipeline.reset();
}

} // namespace rive::ore
