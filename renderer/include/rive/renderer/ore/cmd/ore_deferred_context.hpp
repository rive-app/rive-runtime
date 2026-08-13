/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/renderer/ore/cmd/ore_command_buffer.hpp"
#include "rive/renderer/ore/cmd/ore_deferred_resource.hpp"
#include "rive/renderer/ore/cmd/ore_make_recording.hpp"
#include "rive/renderer/ore/cmd/ore_make_replay.hpp"
#include "rive/renderer/ore/cmd/ore_render_pass_recording.hpp"
#include "rive/renderer/ore/cmd/ore_replay.hpp"
#include "rive/renderer/cmd/id_allocator.hpp"
#include "rive/renderer/cmd/foreign_image_registry.hpp"
#include "rive/renderer/cmd/live_recorder_registry.hpp"
#include "rive/renderer/ore/ore_context.hpp"
#include "utils/lite_rtti.hpp"
#include <cassert>
#include <unordered_map>
#include <vector>

// Ore Context used while recording in deferred mode. make* and beginRenderPass
// record into one ordered stream with no GPU work; replayFrame materializes it
// on a real context. Stream order makes id reuse safe, so the allocator can
// recycle handles without a recycled id aliasing a live resource.
namespace rive::ore::cmd
{

class DeferredOreContext : public Context
{
public:
    // The replay device's capabilities, carried as data so recording holds no
    // device. A host that also passes the real context keeps the sessionless
    // GM wrap fallback; every capability answer comes from the caps.
    explicit DeferredOreContext(const ReplayCaps& caps,
                                Context* real = nullptr) :
        Context(nullptr), m_caps(caps), m_real(real)
    {
        adoptCapsFeatures();
        m_render.realHandleProvider = [this](rive::gpu::GPUResource* r) {
            return realHandleFor(r);
        };
        m_render.bindRecordingThread();
        rive::cmd::registerRecorder(&m_render);
        rive::cmd::registerRecorder(&m_ids);
    }

    // real may be null at construction and bound later via bindReal.
    explicit DeferredOreContext(Context* real) :
        DeferredOreContext(real != nullptr ? ReplayCaps::from(*real)
                                           : ReplayCaps{},
                           real)
    {}

    ~DeferredOreContext() override
    {
        // Unregister first so a late finalizer release no-ops instead of
        // writing into a dead recorder.
        rive::cmd::unregisterRecorder(&m_render);
        rive::cmd::unregisterRecorder(&m_ids);
        // Drain cross thread destroys before the stream dies.
        m_render.drainDestroys();
    }

    // make*: record a create and return a deferred object that records its own
    // writes and destruction into the same stream.

    rcp<Buffer> makeBuffer(const BufferDesc& desc) override
    {
        auto a = m_ids.alloc();
        recordMakeBuffer(m_render, a.id, a.generation, desc);
        return make_rcp<DeferredBuffer>(a.id,
                                        a.generation,
                                        &m_render,
                                        &m_ids,
                                        desc.size,
                                        desc.usage);
    }

    rcp<Texture> makeTexture(const TextureDesc& desc) override
    {
        auto a = m_ids.alloc();
        recordMakeTexture(m_render, a.id, a.generation, desc);
        return make_rcp<DeferredTexture>(a.id,
                                         a.generation,
                                         &m_render,
                                         &m_ids,
                                         desc);
    }

    rcp<TextureView> makeTextureView(const TextureViewDesc& desc) override
    {
        auto a = m_ids.alloc();
        recordMakeTextureView(m_render,
                              a.id,
                              a.generation,
                              desc,
                              handleFor(desc.texture));
        return make_rcp<DeferredTextureView>(a.id,
                                             a.generation,
                                             &m_render,
                                             &m_ids,
                                             ref_rcp(desc.texture),
                                             desc);
    }

    rcp<Sampler> makeSampler(const SamplerDesc& desc) override
    {
        auto a = m_ids.alloc();
        recordMakeSampler(m_render, a.id, a.generation, desc);
        return make_rcp<DeferredSampler>(a.id, a.generation, &m_render, &m_ids);
    }

    rcp<ShaderModule> makeShaderModule(const ShaderModuleDesc& desc) override
    {
        auto a = m_ids.alloc();
        recordMakeShaderModule(m_render, a.id, a.generation, desc);
        auto obj = make_rcp<DeferredShaderModule>(a.id,
                                                  a.generation,
                                                  &m_render,
                                                  &m_ids);
        // Parse the binding map so record time validation and layout
        // derivation match the real backend.
        if (desc.bindingMapBytes != nullptr && desc.bindingMapSize > 0)
        {
            obj->applyBindingMapFromDesc(desc);
        }
        return obj;
    }

    rcp<BindGroupLayout> makeBindGroupLayout(
        const BindGroupLayoutDesc& desc) override
    {
        auto a = m_ids.alloc();
        recordMakeBindGroupLayout(m_render, a.id, a.generation, desc);
        return make_rcp<DeferredBindGroupLayout>(a.id,
                                                 a.generation,
                                                 &m_render,
                                                 &m_ids,
                                                 desc);
    }

    rcp<Pipeline> makePipeline(const PipelineDesc& desc,
                               std::string* /*outError*/ = nullptr) override
    {
        std::vector<ResourceHandle> bgls(desc.bindGroupLayoutCount);
        for (uint32_t i = 0; i < desc.bindGroupLayoutCount; ++i)
        {
            bgls[i] = handleFor(desc.bindGroupLayouts[i]);
        }
        auto a = m_ids.alloc();
        recordMakePipeline(
            m_render,
            a.id,
            a.generation,
            desc,
            handleFor(desc.vertexModule),
            handleFor(desc.fragmentModule),
            Span<const ResourceHandle>(bgls.data(), bgls.size()));
        return make_rcp<DeferredPipeline>(a.id,
                                          a.generation,
                                          &m_render,
                                          &m_ids,
                                          desc);
    }

    rcp<BindGroup> makeBindGroup(const BindGroupDesc& desc) override
    {
        std::vector<ResourceHandle> ubos(desc.uboCount),
            texs(desc.textureCount), samps(desc.samplerCount);
        for (uint32_t i = 0; i < desc.uboCount; ++i)
        {
            ubos[i] = handleFor(desc.ubos[i].buffer);
        }
        for (uint32_t i = 0; i < desc.textureCount; ++i)
        {
            texs[i] = handleFor(desc.textures[i].view);
        }
        for (uint32_t i = 0; i < desc.samplerCount; ++i)
        {
            samps[i] = handleFor(desc.samplers[i].sampler);
        }
        auto a = m_ids.alloc();
        recordMakeBindGroup(
            m_render,
            a.id,
            a.generation,
            desc,
            handleFor(desc.layout),
            Span<const ResourceHandle>(ubos.data(), ubos.size()),
            Span<const ResourceHandle>(texs.data(), texs.size()),
            Span<const ResourceHandle>(samps.data(), samps.size()));
        return make_rcp<DeferredBindGroup>(a.id,
                                           a.generation,
                                           &m_render,
                                           &m_ids,
                                           desc);
    }

    std::unique_ptr<RenderPass> beginRenderPass(
        const RenderPassDesc& desc,
        std::string* /*outError*/ = nullptr) override
    {
        return std::make_unique<RenderPassRecording>(this, &m_render, desc);
    }

    // Maps a canvas to its shared canvas id for replay. Set by the
    // DeferredSession; when set wrapCanvasTexture never touches the device.
    std::function<uint32_t(gpu::RenderCanvas*)> canvasIdProvider;

    // Late binding for hosts whose real context outlives session creation.
    void bindReal(Context* real)
    {
        m_real = real;
        if (real != nullptr)
        {
            bindCaps(ReplayCaps::from(*real));
        }
    }

    // Descriptor form of the late bind, for hosts whose device lives on
    // another thread or process and only its capabilities travel.
    void bindCaps(const ReplayCaps& caps)
    {
        bool late = !m_caps.featuresKnown && caps.featuresKnown;
        if (late)
        {
            checkUnboundAssumptions(caps);
        }
        m_caps = caps;
        adoptCapsFeatures();
    }

    // A script must not branch on a capability this context cannot know. It
    // knows one only once a replay device's caps arrive.
    bool featuresKnown() const override { return m_caps.featuresKnown; }

    const ReplayCaps& caps() const { return m_caps; }

    bool isRecording() const override { return true; }

    // Maps a canvas backed image to its shared canvas id. Set by the owning
    // session, null on sessionless GMs.
    rive::cmd::ForeignImageRegistry* canvasRegistry = nullptr;

    // Proxy view a reserved canvas returns at record time. The proxy format
    // must match the real backing or checkPipelineCompat rejects every
    // pipeline bound against it, and RenderPassRecording::setPipeline drops
    // the command rather than appending it. Unbound there is nothing to ask,
    // so the base class default stands and checkUnboundAssumptions fires if a
    // late bound backend turns out to override it.
    static constexpr TextureFormat kUnboundCanvasFormat =
        TextureFormat::rgba8unorm;
    rcp<TextureView> makeReservedCanvasView(ResourceHandle id,
                                            uint32_t generation,
                                            uint32_t width,
                                            uint32_t height)
    {
        TextureDesc texDesc{};
        texDesc.width = width;
        texDesc.height = height;
        texDesc.format = m_caps.canvasTargetFormat;
        texDesc.type = TextureType::texture2D;
        texDesc.renderTarget = true;
        texDesc.numMipmaps = 1;
        texDesc.sampleCount = 1;
        auto proxyTex =
            make_rcp<DeferredTexture>(0u, 0u, nullptr, nullptr, texDesc);
        TextureViewDesc viewDesc{};
        viewDesc.texture = proxyTex.get();
        viewDesc.dimension = TextureViewDimension::texture2D;
        viewDesc.baseMipLevel = 0;
        viewDesc.mipCount = 1;
        viewDesc.baseLayer = 0;
        viewDesc.layerCount = 1;
        return make_rcp<DeferredTextureView>(id,
                                             generation,
                                             &m_render,
                                             &m_ids,
                                             std::move(proxyTex),
                                             viewDesc);
    }

    rcp<TextureView> wrapCanvasTexture(gpu::RenderCanvas* c) override
    {
        if (!canvasIdProvider)
        {
            // Sessionless GM fallback: wrap the real host canvas directly.
            assert(m_real != nullptr);
            return m_real->wrapCanvasTexture(c);
        }
        // Reserve now; the consumer wraps at replay.
        uint32_t canvasId = canvasIdProvider(c);
        auto a = m_ids.alloc();
        recordWrapCanvasView(m_render,
                             a.id,
                             a.generation,
                             canvasId,
                             WrapCanvasViewMode::colorView);
        return makeReservedCanvasView(a.id,
                                      a.generation,
                                      c->width(),
                                      c->height());
    }

    // Same reserve as wrapCanvasTexture but tagged sampleView so the consumer
    // does the backend sampling wrap at replay.
    rcp<TextureView> recordWrapCanvasImage(RenderImage* image,
                                           uint32_t width,
                                           uint32_t height) override
    {
        assert(canvasRegistry != nullptr);
        uint32_t canvasId =
            canvasRegistry->imageDrawId(image) & rive::cmd::kCanvasHandleMask;
        auto a = m_ids.alloc();
        recordWrapCanvasView(m_render,
                             a.id,
                             a.generation,
                             canvasId,
                             WrapCanvasViewMode::sampleView);
        return makeReservedCanvasView(a.id, a.generation, width, height);
    }

    // Decoded image view: the consumer resolves the resident image and wraps
    // its texture at replay.
    rcp<TextureView> recordWrapImageView(uint32_t imageId,
                                         uint32_t width,
                                         uint32_t height) override
    {
        auto a = m_ids.alloc();
        recordWrapCanvasView(m_render,
                             a.id,
                             a.generation,
                             imageId,
                             WrapCanvasViewMode::imageView);
        return makeReservedCanvasView(a.id, a.generation, width, height);
    }
    rcp<TextureView> wrapRiveTexture(gpu::Texture* t,
                                     uint32_t w,
                                     uint32_t h) override
    {
        // Every script GPU op must be deferred. Hitting this while recording
        // means a caller wraps a texture without its own reserve path.
        fprintf(stderr,
                "rive deferred: TRIPWIRE wrapRiveTexture hit immediately "
                "during recording (a script GPU op is not deferred)\n");
        assert(false && "wrapRiveTexture must be deferred while recording");
        if (m_real == nullptr)
        {
            return nullptr;
        }
        return m_real->wrapRiveTexture(t, w, h);
    }
    // Selects the RSTB variant a script loads and records, so unlike features
    // there is no way to refuse: returning nothing loads no shader at all.
    // Web is the only host that binds late and web is GL, so the fallback is
    // an assumption about one host rather than a guess about any device, and
    // checkUnboundAssumptions fires if a late bind ever contradicts it.
    static constexpr ShaderTarget kUnboundShaderTarget = ShaderTarget::glsl;
    ShaderTarget shaderTarget() const override { return m_caps.shaderTarget; }

    // No GPU at record time.

    void beginFrame(const FrameDescriptor&) override {}
    void endFrame() override {}
    void waitForGPU() override {}

    // Resident table the consumer persists across frames.
    using RealTable = OreResident;

    // Creates are idempotent, so a frame can replay repeatedly without
    // recompiling. Persistent table for streaming, throwaway for single shot.
    void replayFrame(Context& realCtx,
                     RealTable& table,
                     const OreCanvasResolve& canvasAt = {})
    {
        // A host that declared caps recorded against them; a replay device
        // that disagrees executes a stream recorded for other hardware.
        assert(!m_caps.featuresKnown || m_caps.matchesReplayDevice(realCtx));
        replayOreStream(
            realCtx,
            m_render,
            table,
            [this](ResourceHandle h) { return resolveReal(h); },
            canvasAt);
    }

    // Single shot replay against a throwaway table, used by goldens.
    void replay(Context& realCtx)
    {
        RealTable table;
        replayFrame(realCtx, table);
    }

    struct StreamBytes
    {
        size_t commands, blobs;
        size_t total() const { return commands + blobs; }
    };
    StreamBytes streamBytes() const
    {
        return {m_render.commandBytes().size(), m_render.blobBytes().size()};
    }

    // What this context references r by, the lookup every recorded cross
    // reference resolves through. A deferred object of ours answers for
    // itself: a live object cannot be stale about its own handle, so no
    // address the allocator recycles can ever speak for the dead object that
    // used to occupy it. Anything else is a real resource, retained by the
    // frame and addressed by a flagged index.
    ResourceHandle handleFor(Buffer* b)
    {
        return handleForAs<DeferredBuffer>(b);
    }
    ResourceHandle handleFor(Texture* t)
    {
        return handleForAs<DeferredTexture>(t);
    }
    ResourceHandle handleFor(TextureView* v)
    {
        return handleForAs<DeferredTextureView>(v);
    }
    ResourceHandle handleFor(Sampler* s)
    {
        return handleForAs<DeferredSampler>(s);
    }
    ResourceHandle handleFor(ShaderModule* m)
    {
        return handleForAs<DeferredShaderModule>(m);
    }
    ResourceHandle handleFor(BindGroupLayout* l)
    {
        return handleForAs<DeferredBindGroupLayout>(l);
    }
    ResourceHandle handleFor(Pipeline* p)
    {
        return handleForAs<DeferredPipeline>(p);
    }
    ResourceHandle handleFor(BindGroup* g)
    {
        return handleForAs<DeferredBindGroup>(g);
    }

    // The consumer keeps its resident table. Real bindings are re-captured
    // each frame, keeping the retained set bounded by what one frame binds.
    void resetFrame()
    {
        m_render.reset();
        // Cross thread destroys drain on the recording thread, landing at the
        // new frame's stream head.
        m_render.drainDestroys();
        m_realPtrToHandle.clear();
        m_realResources.clear();
    }

    // Real bindings captured by this frame's stream, indexed by unflagged id.
    const std::vector<rcp<rive::gpu::GPUResource>>& realResources() const
    {
        return m_realResources;
    }

    const OreCommandBuffer& stream() const { return m_render; }

private:
    // The replay device's capabilities are the ones a recording script has to
    // see: it is the device the recorded branch will run on. Copied rather
    // than forwarded so features() stays a non-virtual field read, and the
    // copy cannot go stale because a backend measures its Features once, in
    // its Make.
    void adoptCapsFeatures()
    {
        if (m_caps.featuresKnown)
        {
            m_features = m_caps.features;
        }
    }

    // Everything answered before a late bind was answered without a device.
    // features() refused rather than guessing, but the shader target and the
    // canvas format had to answer something, and a script has already loaded
    // and recorded against both. If the caps that just arrived disagree, the
    // stream is already wrong in a way replay cannot detect: a mismatched
    // canvas format makes checkPipelineCompat drop every setPipeline, leaving
    // draws with no pipeline bound.
    void checkUnboundAssumptions(const ReplayCaps& incoming)
    {
        if (incoming.shaderTarget != m_caps.shaderTarget)
        {
            fprintf(stderr,
                    "rive deferred: TRIPWIRE late bound backend consumes "
                    "shader target %u, but recording already loaded %u\n",
                    static_cast<unsigned>(incoming.shaderTarget),
                    static_cast<unsigned>(m_caps.shaderTarget));
            assert(false && "late bind changed the recorded shader target");
        }
        if (incoming.canvasTargetFormat != m_caps.canvasTargetFormat)
        {
            fprintf(stderr,
                    "rive deferred: TRIPWIRE late bound backend allocates "
                    "canvases as format %u, but recording already reserved "
                    "canvas views as %u\n",
                    static_cast<unsigned>(incoming.canvasTargetFormat),
                    static_cast<unsigned>(m_caps.canvasTargetFormat));
            assert(false && "late bind changed the recorded canvas format");
        }
    }

    // Resolves a flagged real id to its retained real object.
    rive::gpu::GPUResource* resolveReal(ResourceHandle h)
    {
        ResourceHandle i = h & kRealResourceMask;
        return i < m_realResources.size() ? m_realResources[i].get() : nullptr;
    }

    // A deferred object reports its own handle, but only if it records into
    // a stream this context writes: a reference is an index into the table
    // that stream feeds, and a foreign one would name whatever this context
    // happens to hold at that id. Those take the real path, as they did when
    // this was a map lookup that simply failed to find them.
    template <typename DeferredT, typename T> ResourceHandle handleForAs(T* r)
    {
        if (r == nullptr)
        {
            return kInvalidHandle;
        }
        if (auto* d = rive::lite_rtti_cast<DeferredT*>(r))
        {
            if (d->recordsInto(&m_render))
            {
                return d->clientHandle();
            }
        }
        return realHandleFor(r);
    }

    // An already real resource gets a flagged id and is retained so it lives
    // to replay.
    ResourceHandle realHandleFor(rive::gpu::GPUResource* r)
    {
        if (r == nullptr)
        {
            return kInvalidHandle;
        }
        auto rit = m_realPtrToHandle.find(r);
        if (rit != m_realPtrToHandle.end())
        {
            return rit->second;
        }
        // The unflagged index must fit under the flag bit.
        assert(m_realResources.size() <= kRealResourceMask);
        ResourceHandle h = kRealResourceFlag |
                           static_cast<ResourceHandle>(m_realResources.size());
        m_realResources.push_back(rive::ref_rcp(r));
        m_realPtrToHandle.emplace(r, h);
        return h;
    }

    // What the recording answers about the replay device; the only capability
    // source. m_real exists solely for the sessionless GM wrap fallback.
    ReplayCaps m_caps;
    Context* m_real;
    OreCommandBuffer m_render; // the one ordered stream
    // Reusable id space shared by every resource type.
    rive::IdAllocator<ResourceHandle> m_ids;
    // Already real resources this frame bound, retained until replay and
    // addressed by flagged id. Cleared every resetFrame.
    PtrHandleMap m_realPtrToHandle;
    std::vector<rcp<rive::gpu::GPUResource>> m_realResources;
};

} // namespace rive::ore::cmd
