/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/renderer/ore/cmd/ore_command_buffer.hpp"
#include "rive/renderer/ore/cmd/ore_commands.hpp"
#include "rive/renderer/ore/ore_context.hpp"
#include "rive/renderer/rive_render_image.hpp"
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_texture.hpp"
#include <cassert>
#include <functional>
#include <vector>

// Consumer half of the ordered ore stream: the resident table plus the
// lifecycle replay arms. Pass arms live in ore_replay.hpp. The table is dense
// and bounded by the live high water mark; a stale destroy for a recycled id
// is dropped by the generation check.
namespace rive::ore::cmd
{

// Slot type tag: one allocator serves every resource type, so an id race
// under churn can put a differently typed object in a referenced slot. A
// kind checked lookup turns that into an unresolved dependency instead of a
// garbage cast.
enum class OreKind : uint8_t
{
    none,
    buffer,
    texture,
    textureView,
    sampler,
    shaderModule,
    bindGroupLayout,
    pipeline,
    bindGroup,
};

struct OreResident
{
    std::vector<rcp<rive::gpu::GPUResource>> objects;
    std::vector<uint32_t> generations;
    std::vector<OreKind> kinds;

    void set(ResourceHandle id,
             rcp<rive::gpu::GPUResource> obj,
             uint32_t generation,
             OreKind kind)
    {
        if (id > objects.size())
        {
            // The producer mints ids sequentially, so a fresh id may only
            // append. Anything further ahead is a corrupt stream.
            assert(false);
            return;
        }
        if (id == objects.size())
        {
            objects.push_back(std::move(obj));
            generations.push_back(generation);
            kinds.push_back(kind);
            return;
        }
        objects[id] = std::move(obj);
        generations[id] = generation;
        kinds[id] = kind;
    }
    void destroy(ResourceHandle id, uint32_t generation)
    {
        if (id < objects.size() && generations[id] == generation)
        {
            objects[id] = nullptr;
        }
    }
    rive::gpu::GPUResource* get(ResourceHandle id) const
    {
        return id < objects.size() ? objects[id].get() : nullptr;
    }
    rive::gpu::GPUResource* getAs(ResourceHandle id, OreKind kind) const
    {
        return id < objects.size() && kinds[id] == kind ? objects[id].get()
                                                        : nullptr;
    }
    // Make replay skips live slots, so repeated replays do not recompile.
    bool alive(ResourceHandle id, uint32_t generation) const
    {
        return id < objects.size() && objects[id] != nullptr &&
               generations[id] == generation;
    }
};

// A flagged id is an already real resource; any other id indexes the resident
// table.
using OreHandleResolve = std::function<rive::gpu::GPUResource*(ResourceHandle)>;

// Kind checked resolve used by makes and passes; real flagged ids stay
// unchecked since the real side table is typed by construction.
using OreKindResolve =
    std::function<rive::gpu::GPUResource*(ResourceHandle, OreKind)>;

// Resolves a shared canvas id to its real RenderCanvas at replay.
using OreCanvasResolve = std::function<rive::gpu::RenderCanvas*(uint32_t)>;

// Resolves a 2D image id to its resident RenderImage at replay.
using OreImageResolve = std::function<rive::RenderImage*(uint32_t)>;

// Resolve a stream reference: a real flagged id hits the caller's side table,
// a bare id the resident table.
inline rive::gpu::GPUResource* resolveOre(const OreResident& session,
                                          const OreHandleResolve& real,
                                          ResourceHandle h,
                                          OreKind kind)
{
    if (h == kInvalidHandle)
    {
        return nullptr;
    }
    if (h & kRealResourceFlag)
    {
        return real ? real(h) : nullptr;
    }
    return session.getAs(h, kind);
}

// Returns false for a pass command so the caller's pass switch takes it.
inline bool replayOreLifecycle(Context& ctx,
                               OreResident& table,
                               CommandType type,
                               OreCommandReader& reader,
                               const OreKindResolve& resolve,
                               const OreCanvasResolve& canvasAt = {},
                               const OreImageResolve& imageAt = {})
{
    auto blob = [&](BlobRef ref) -> Span<const uint8_t> {
        return ref.absent() ? Span<const uint8_t>(nullptr, 0)
                            : reader.blobAt(ref.offset, ref.size);
    };
    auto cstr = [&](BlobRef ref) -> const char* {
        return ref.absent() ? nullptr
                            : reinterpret_cast<const char*>(blob(ref).data());
    };
    auto bytesOf = [&](BlobRef ref) -> const void* {
        return ref.absent() ? nullptr : blob(ref).data();
    };

    // A dependency that should resolve but comes back null was churned under a
    // straddling frame. Skip the make since a null crashes some backends.
    bool unresolvedDep = false;
    auto req = [&](ResourceHandle h, OreKind kind) -> rive::gpu::GPUResource* {
        auto* r = resolve(h, kind);
        if (r == nullptr && h != kInvalidHandle)
        {
            unresolvedDep = true;
        }
        return r;
    };
    // The null slot keeps the dense table aligned with minted ids; skipping
    // the set would discard every later make behind the hole.
    auto skipUnresolvedMake =
        [&](ResourceHandle id, uint32_t generation, const char* what) -> bool {
        RIVE_WARN_THROTTLED("rive ore replay: skip make %s id=%u gen=%u "
                            "(unresolved dep, churn)\n",
                            what,
                            id,
                            generation);
        table.set(id, nullptr, generation, OreKind::none);
        return true; // empty slot, downstream draws drop
    };

    switch (type)
    {
        case CommandType::makeBuffer:
        {
            auto m = reader.read<MakeResourcePOD>();
            auto pod = reader.read<BufferDescPOD>();
            if (table.alive(m.id, m.generation))
            {
                return true;
            }
            BufferDesc d{};
            d.usage = pod.usage;
            d.size = pod.size;
            d.immutable = pod.immutable;
            d.data = bytesOf(pod.data);
            d.label = cstr(pod.label);
            table.set(m.id, ctx.makeBuffer(d), m.generation, OreKind::buffer);
            return true;
        }
        case CommandType::makeTexture:
        {
            auto m = reader.read<MakeResourcePOD>();
            auto pod = reader.read<TextureDescPOD>();
            if (table.alive(m.id, m.generation))
            {
                return true;
            }
            TextureDesc d{};
            d.width = pod.width;
            d.height = pod.height;
            d.depthOrArrayLayers = pod.depthOrArrayLayers;
            d.format = pod.format;
            d.type = pod.type;
            d.renderTarget = pod.renderTarget;
            d.numMipmaps = pod.numMipmaps;
            d.sampleCount = pod.sampleCount;
            d.label = cstr(pod.label);
            table.set(m.id, ctx.makeTexture(d), m.generation, OreKind::texture);
            return true;
        }
        case CommandType::makeSampler:
        {
            auto m = reader.read<MakeResourcePOD>();
            auto pod = reader.read<SamplerDescPOD>();
            if (table.alive(m.id, m.generation))
            {
                return true;
            }
            SamplerDesc d{};
            d.minFilter = pod.minFilter;
            d.magFilter = pod.magFilter;
            d.mipmapFilter = pod.mipmapFilter;
            d.wrapU = pod.wrapU;
            d.wrapV = pod.wrapV;
            d.wrapW = pod.wrapW;
            d.compare = pod.compare;
            d.minLod = pod.minLod;
            d.maxLod = pod.maxLod;
            d.maxAnisotropy = pod.maxAnisotropy;
            d.label = cstr(pod.label);
            table.set(m.id, ctx.makeSampler(d), m.generation, OreKind::sampler);
            return true;
        }
        case CommandType::makeShaderModule:
        {
            auto m = reader.read<MakeResourcePOD>();
            auto pod = reader.read<ShaderModuleDescPOD>();
            if (table.alive(m.id, m.generation))
            {
                return true;
            }
            ShaderModuleDesc d{};
            d.code = bytesOf(pod.code);
            d.codeSize = static_cast<uint32_t>(blob(pod.code).size());
            d.language = pod.language;
            d.stage = pod.stage;
            d.label = cstr(pod.label);
            d.hlslSource = cstr(pod.hlslSource);
            d.hlslSourceSize =
                static_cast<uint32_t>(blob(pod.hlslSource).size());
            d.hlslEntryPoint = cstr(pod.hlslEntryPoint);
            d.bindingMapBytes =
                static_cast<const uint8_t*>(bytesOf(pod.bindingMapBytes));
            d.bindingMapSize =
                static_cast<uint32_t>(blob(pod.bindingMapBytes).size());
            d.glFixupBytes =
                static_cast<const uint8_t*>(bytesOf(pod.glFixupBytes));
            d.glFixupSize =
                static_cast<uint32_t>(blob(pod.glFixupBytes).size());
            d.shaderAssetId = pod.shaderAssetId;
            table.set(m.id,
                      ctx.makeShaderModule(d),
                      m.generation,
                      OreKind::shaderModule);
            return true;
        }
        case CommandType::makeBindGroupLayout:
        {
            auto m = reader.read<MakeResourcePOD>();
            auto pod = reader.read<BindGroupLayoutDescPOD>();
            if (table.alive(m.id, m.generation))
            {
                return true;
            }
            BindGroupLayoutDesc d{};
            d.groupIndex = pod.groupIndex;
            d.entryCount = pod.entryCount;
            d.entries = reinterpret_cast<const BindGroupLayoutEntry*>(
                bytesOf(pod.entries));
            d.label = cstr(pod.label);
            table.set(m.id,
                      ctx.makeBindGroupLayout(d),
                      m.generation,
                      OreKind::bindGroupLayout);
            return true;
        }
        case CommandType::makeTextureView:
        {
            auto m = reader.read<MakeResourcePOD>();
            auto pod = reader.read<TextureViewDescPOD>();
            if (table.alive(m.id, m.generation))
            {
                return true;
            }
            TextureViewDesc d{};
            d.texture =
                static_cast<Texture*>(req(pod.texture, OreKind::texture));
            d.dimension = pod.dimension;
            d.aspect = pod.aspect;
            d.baseMipLevel = pod.baseMipLevel;
            d.mipCount = pod.mipCount;
            d.baseLayer = pod.baseLayer;
            d.layerCount = pod.layerCount;
            if (unresolvedDep)
                return skipUnresolvedMake(m.id, m.generation, "textureView");
            table.set(m.id,
                      ctx.makeTextureView(d),
                      m.generation,
                      OreKind::textureView);
            return true;
        }
        case CommandType::makePipeline:
        {
            auto m = reader.read<MakeResourcePOD>();
            auto pod = reader.read<PipelineDescPOD>();
            if (table.alive(m.id, m.generation))
            {
                return true;
            }
            Span<const uint8_t> vbBlob = blob(pod.vertexBuffers);
            const VertexBufferLayoutPOD* vbPods =
                reinterpret_cast<const VertexBufferLayoutPOD*>(vbBlob.data());
            std::vector<VertexBufferLayout> vbs(pod.vertexBufferCount);
            for (uint32_t i = 0; i < pod.vertexBufferCount; ++i)
            {
                vbs[i].stride = vbPods[i].stride;
                vbs[i].stepMode = vbPods[i].stepMode;
                vbs[i].attributeCount = vbPods[i].attributeCount;
                vbs[i].attributes = reinterpret_cast<const VertexAttribute*>(
                    blob(vbPods[i].attributes).data());
            }
            Span<const uint8_t> bglBlob = blob(pod.bindGroupLayouts);
            const ResourceHandle* bglHandles =
                reinterpret_cast<const ResourceHandle*>(bglBlob.data());
            std::vector<BindGroupLayout*> bgls(pod.bindGroupLayoutCount);
            for (uint32_t i = 0; i < pod.bindGroupLayoutCount; ++i)
            {
                bgls[i] = static_cast<BindGroupLayout*>(
                    req(bglHandles[i], OreKind::bindGroupLayout));
            }

            PipelineDesc d{};
            d.vertexModule = static_cast<ShaderModule*>(
                req(pod.vertexModule, OreKind::shaderModule));
            d.vertexEntryPoint = cstr(pod.vertexEntryPoint);
            d.fragmentModule = static_cast<ShaderModule*>(
                req(pod.fragmentModule, OreKind::shaderModule));
            d.fragmentEntryPoint = cstr(pod.fragmentEntryPoint);
            d.vertexBuffers = vbs.empty() ? nullptr : vbs.data();
            d.vertexBufferCount = pod.vertexBufferCount;
            d.topology = pod.topology;
            d.indexFormat = pod.indexFormat;
            d.cullMode = pod.cullMode;
            d.winding = pod.winding;
            for (uint32_t i = 0; i < 4; ++i)
            {
                d.colorTargets[i] = pod.colorTargets[i];
            }
            d.colorCount = pod.colorCount;
            d.depthStencil = pod.depthStencil;
            d.stencilFront = pod.stencilFront;
            d.stencilBack = pod.stencilBack;
            d.stencilReadMask = pod.stencilReadMask;
            d.stencilWriteMask = pod.stencilWriteMask;
            d.sampleCount = pod.sampleCount;
            d.bindGroupLayouts = bgls.empty() ? nullptr : bgls.data();
            d.bindGroupLayoutCount = pod.bindGroupLayoutCount;
            d.label = cstr(pod.label);
            if (unresolvedDep)
                return skipUnresolvedMake(m.id, m.generation, "pipeline");
            std::string pipelineError;
            auto realPipeline = ctx.makePipeline(d, &pipelineError);
            if (realPipeline == nullptr)
            {
                RIVE_WARN_THROTTLED(
                    "rive ore replay: makePipeline id=%u gen=%u failed: %s\n",
                    m.id,
                    m.generation,
                    pipelineError.c_str());
            }
            table.set(m.id,
                      std::move(realPipeline),
                      m.generation,
                      OreKind::pipeline);
            return true;
        }
        case CommandType::makeBindGroup:
        {
            auto m = reader.read<MakeResourcePOD>();
            auto pod = reader.read<BindGroupDescPOD>();
            if (table.alive(m.id, m.generation))
            {
                return true;
            }
            const UBOEntryPOD* uboPods =
                reinterpret_cast<const UBOEntryPOD*>(blob(pod.ubos).data());
            std::vector<BindGroupDesc::UBOEntry> ubos(pod.uboCount);
            for (uint32_t i = 0; i < pod.uboCount; ++i)
            {
                ubos[i].slot = uboPods[i].slot;
                ubos[i].buffer = static_cast<Buffer*>(
                    req(uboPods[i].buffer, OreKind::buffer));
                ubos[i].offset = uboPods[i].offset;
                ubos[i].size = uboPods[i].size;
            }
            const TexEntryPOD* texPods =
                reinterpret_cast<const TexEntryPOD*>(blob(pod.textures).data());
            std::vector<BindGroupDesc::TexEntry> texs(pod.textureCount);
            for (uint32_t i = 0; i < pod.textureCount; ++i)
            {
                texs[i].slot = texPods[i].slot;
                texs[i].view = static_cast<TextureView*>(
                    req(texPods[i].view, OreKind::textureView));
            }
            const SampEntryPOD* sampPods =
                reinterpret_cast<const SampEntryPOD*>(
                    blob(pod.samplers).data());
            std::vector<BindGroupDesc::SampEntry> samps(pod.samplerCount);
            for (uint32_t i = 0; i < pod.samplerCount; ++i)
            {
                samps[i].slot = sampPods[i].slot;
                samps[i].sampler = static_cast<Sampler*>(
                    req(sampPods[i].sampler, OreKind::sampler));
            }

            BindGroupDesc d{};
            d.layout = static_cast<BindGroupLayout*>(
                req(pod.layout, OreKind::bindGroupLayout));
            d.ubos = ubos.empty() ? nullptr : ubos.data();
            d.uboCount = pod.uboCount;
            d.textures = texs.empty() ? nullptr : texs.data();
            d.textureCount = pod.textureCount;
            d.samplers = samps.empty() ? nullptr : samps.data();
            d.samplerCount = pod.samplerCount;
            d.label = cstr(pod.label);
            if (unresolvedDep)
                return skipUnresolvedMake(m.id, m.generation, "bindGroup");
            auto realBindGroup = ctx.makeBindGroup(d);
            if (realBindGroup == nullptr)
            {
                RIVE_WARN_THROTTLED(
                    "rive ore replay: makeBindGroup id=%u gen=%u returned "
                    "null\n",
                    m.id,
                    m.generation);
            }
            table.set(m.id,
                      std::move(realBindGroup),
                      m.generation,
                      OreKind::bindGroup);
            return true;
        }
        case CommandType::bufferUpdate:
        {
            auto pod = reader.read<BufferUpdatePOD>();
            Span<const uint8_t> b = blob(pod.bytes);
            if (auto* buf = static_cast<Buffer*>(table.get(pod.handle)))
            {
                buf->update(b.data(),
                            static_cast<uint32_t>(b.size()),
                            pod.offset);
            }
            return true;
        }
        case CommandType::textureUpload:
        {
            auto pod = reader.read<TextureUploadPOD>();
            Span<const uint8_t> b = blob(pod.bytes);
            TextureDataDesc d{};
            d.data = b.empty() ? nullptr : b.data();
            d.bytesPerRow = pod.bytesPerRow;
            d.rowsPerImage = pod.rowsPerImage;
            d.mipLevel = pod.mipLevel;
            d.layer = pod.layer;
            d.x = pod.x;
            d.y = pod.y;
            d.z = pod.z;
            d.width = pod.width;
            d.height = pod.height;
            d.depth = pod.depth;
            if (auto* tex = static_cast<Texture*>(table.get(pod.handle)))
            {
                tex->upload(d);
            }
            return true;
        }
        case CommandType::wrapCanvasView:
        {
            // The consumer performs the real wrap reserved at record time.
            auto pod = reader.read<WrapCanvasViewPOD>();
            if (table.alive(pod.id, pod.generation))
            {
                return true;
            }
            if (pod.mode ==
                static_cast<uint32_t>(WrapCanvasViewMode::imageView))
            {
                // A null image, still decoding or churned, leaves the slot
                // empty so downstream draws drop instead of binding a null.
                rive::RenderImage* image =
                    imageAt ? imageAt(pod.canvasId) : nullptr;
                auto* riveImage =
                    image ? lite_rtti_cast<rive::RiveRenderImage*>(image)
                          : nullptr;
                rive::gpu::Texture* tex =
                    riveImage ? riveImage->getTexture() : nullptr;
                rcp<TextureView> wrapped;
                if (tex != nullptr)
                {
                    wrapped = ctx.wrapRiveTexture(tex,
                                                  image->width(),
                                                  image->height());
                }
                else
                {
                    skipUnresolvedMake(pod.id, pod.generation, "wrapImageView");
                }
                table.set(pod.id,
                          std::move(wrapped),
                          pod.generation,
                          OreKind::textureView);
                return true;
            }
            rive::gpu::RenderCanvas* canvas =
                canvasAt ? canvasAt(pod.canvasId) : nullptr;
            assert(canvas != nullptr);
            rcp<TextureView> wrapped;
            if (canvas != nullptr)
            {
                wrapped = pod.mode == static_cast<uint32_t>(
                                          WrapCanvasViewMode::sampleView)
                              ? ctx.wrapCanvasSampleView(canvas)
                              : ctx.wrapCanvasTexture(canvas);
            }
            table.set(pod.id,
                      std::move(wrapped),
                      pod.generation,
                      OreKind::textureView);
            return true;
        }
        case CommandType::destroyResource:
        {
            auto pod = reader.read<DestroyResourcePOD>();
            table.destroy(pod.handle, pod.generation);
            return true;
        }
        default:
            return false; // a pass command
    }
}

// Consumes one command's payload without executing it, for tooling.
inline void skipOreCommand(CommandType type, OreCommandReader& reader)
{
    reader.skip(orePayloadSizeOf(type));
}

} // namespace rive::ore::cmd
