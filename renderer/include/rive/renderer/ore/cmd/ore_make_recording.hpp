/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/renderer/ore/cmd/ore_command_buffer.hpp"
#include "rive/renderer/ore/cmd/ore_commands.hpp"
#include "rive/renderer/ore/ore_types.hpp"
#include <vector>

// Records the make* family into the ordered Ore command stream. The caller
// owns id allocation so id and generation ride explicitly in the header.
// Replay lives in ore_make_replay.hpp.
namespace rive::ore::cmd
{

inline void recordMakeBuffer(OreCommandBuffer& cb,
                             ResourceHandle id,
                             uint32_t generation,
                             const BufferDesc& desc)
{
    BufferDescPOD pod{};
    pod.usage = desc.usage;
    pod.size = desc.size;
    pod.immutable = desc.immutable;
    pod.data =
        cb.appendBlobRef(desc.data, desc.data ? desc.size : 0, !desc.data);
    pod.label = cb.appendStringRef(desc.label);
    cb.append(CommandType::makeBuffer, MakeResourcePOD{id, generation});
    cb.appendPayload(pod);
}

inline void recordMakeTexture(OreCommandBuffer& cb,
                              ResourceHandle id,
                              uint32_t generation,
                              const TextureDesc& desc)
{
    TextureDescPOD pod{};
    pod.width = desc.width;
    pod.height = desc.height;
    pod.depthOrArrayLayers = desc.depthOrArrayLayers;
    pod.format = desc.format;
    pod.type = desc.type;
    pod.renderTarget = desc.renderTarget;
    pod.numMipmaps = desc.numMipmaps;
    pod.sampleCount = desc.sampleCount;
    pod.label = cb.appendStringRef(desc.label);
    cb.append(CommandType::makeTexture, MakeResourcePOD{id, generation});
    cb.appendPayload(pod);
}

inline void recordMakeSampler(OreCommandBuffer& cb,
                              ResourceHandle id,
                              uint32_t generation,
                              const SamplerDesc& desc)
{
    SamplerDescPOD pod{};
    pod.minFilter = desc.minFilter;
    pod.magFilter = desc.magFilter;
    pod.mipmapFilter = desc.mipmapFilter;
    pod.wrapU = desc.wrapU;
    pod.wrapV = desc.wrapV;
    pod.wrapW = desc.wrapW;
    pod.compare = desc.compare;
    pod.minLod = desc.minLod;
    pod.maxLod = desc.maxLod;
    pod.maxAnisotropy = desc.maxAnisotropy;
    pod.label = cb.appendStringRef(desc.label);
    cb.append(CommandType::makeSampler, MakeResourcePOD{id, generation});
    cb.appendPayload(pod);
}

inline void recordMakeShaderModule(OreCommandBuffer& cb,
                                   ResourceHandle id,
                                   uint32_t generation,
                                   const ShaderModuleDesc& desc)
{
    ShaderModuleDescPOD pod{};
    pod.code = cb.appendBlobRef(desc.code, desc.codeSize, !desc.code);
    pod.language = desc.language;
    pod.stage = desc.stage;
    pod.label = cb.appendStringRef(desc.label);
    pod.hlslSource = cb.appendBlobRef(desc.hlslSource,
                                      desc.hlslSourceSize,
                                      !desc.hlslSource);
    pod.hlslEntryPoint = cb.appendStringRef(desc.hlslEntryPoint);
    pod.bindingMapBytes = cb.appendBlobRef(desc.bindingMapBytes,
                                           desc.bindingMapSize,
                                           !desc.bindingMapBytes);
    pod.glFixupBytes = cb.appendBlobRef(desc.glFixupBytes,
                                        desc.glFixupSize,
                                        !desc.glFixupBytes);
    pod.shaderAssetId = desc.shaderAssetId;
    cb.append(CommandType::makeShaderModule, MakeResourcePOD{id, generation});
    cb.appendPayload(pod);
}

inline void recordMakeBindGroupLayout(OreCommandBuffer& cb,
                                      ResourceHandle id,
                                      uint32_t generation,
                                      const BindGroupLayoutDesc& desc)
{
    BindGroupLayoutDescPOD pod{};
    pod.groupIndex = desc.groupIndex;
    pod.entryCount = desc.entryCount;
    pod.entries =
        cb.appendBlobRef(desc.entries,
                         desc.entryCount * sizeof(BindGroupLayoutEntry),
                         desc.entries == nullptr);
    pod.label = cb.appendStringRef(desc.label);
    cb.append(CommandType::makeBindGroupLayout,
              MakeResourcePOD{id, generation});
    cb.appendPayload(pod);
}

inline void recordMakeTextureView(OreCommandBuffer& cb,
                                  ResourceHandle id,
                                  uint32_t generation,
                                  const TextureViewDesc& desc,
                                  ResourceHandle textureHandle)
{
    TextureViewDescPOD pod{};
    pod.texture = textureHandle;
    pod.dimension = desc.dimension;
    pod.aspect = desc.aspect;
    pod.baseMipLevel = desc.baseMipLevel;
    pod.mipCount = desc.mipCount;
    pod.baseLayer = desc.baseLayer;
    pod.layerCount = desc.layerCount;
    cb.append(CommandType::makeTextureView, MakeResourcePOD{id, generation});
    cb.appendPayload(pod);
}

inline void recordMakePipeline(OreCommandBuffer& cb,
                               ResourceHandle id,
                               uint32_t generation,
                               const PipelineDesc& desc,
                               ResourceHandle vertexModule,
                               ResourceHandle fragmentModule,
                               Span<const ResourceHandle> bindGroupLayouts)
{
    std::vector<VertexBufferLayoutPOD> vbPods(desc.vertexBufferCount);
    for (uint32_t i = 0; i < desc.vertexBufferCount; ++i)
    {
        const VertexBufferLayout& vb = desc.vertexBuffers[i];
        vbPods[i].stride = vb.stride;
        vbPods[i].stepMode = vb.stepMode;
        vbPods[i].attributeCount = vb.attributeCount;
        vbPods[i].attributes =
            cb.appendBlobRef(vb.attributes,
                             vb.attributeCount * sizeof(VertexAttribute),
                             vb.attributes == nullptr);
    }

    PipelineDescPOD pod{};
    pod.vertexModule = vertexModule;
    pod.vertexEntryPoint = cb.appendStringRef(desc.vertexEntryPoint);
    pod.fragmentModule = fragmentModule;
    pod.fragmentEntryPoint = cb.appendStringRef(desc.fragmentEntryPoint);
    pod.vertexBufferCount = desc.vertexBufferCount;
    pod.vertexBuffers = cb.appendBlobRef(
        vbPods.data(),
        static_cast<uint32_t>(vbPods.size() * sizeof(VertexBufferLayoutPOD)),
        vbPods.empty());
    pod.topology = desc.topology;
    pod.indexFormat = desc.indexFormat;
    pod.cullMode = desc.cullMode;
    pod.winding = desc.winding;
    for (uint32_t i = 0; i < 4; ++i)
    {
        pod.colorTargets[i] = desc.colorTargets[i];
    }
    pod.colorCount = desc.colorCount;
    pod.depthStencil = desc.depthStencil;
    pod.stencilFront = desc.stencilFront;
    pod.stencilBack = desc.stencilBack;
    pod.stencilReadMask = desc.stencilReadMask;
    pod.stencilWriteMask = desc.stencilWriteMask;
    pod.sampleCount = desc.sampleCount;
    pod.bindGroupLayoutCount = static_cast<uint32_t>(bindGroupLayouts.size());
    pod.bindGroupLayouts = cb.appendBlobRef(
        bindGroupLayouts.data(),
        static_cast<uint32_t>(bindGroupLayouts.size() * sizeof(ResourceHandle)),
        bindGroupLayouts.empty());
    pod.label = cb.appendStringRef(desc.label);
    cb.append(CommandType::makePipeline, MakeResourcePOD{id, generation});
    cb.appendPayload(pod);
}

inline void recordMakeBindGroup(OreCommandBuffer& cb,
                                ResourceHandle id,
                                uint32_t generation,
                                const BindGroupDesc& desc,
                                ResourceHandle layout,
                                Span<const ResourceHandle> uboBuffers,
                                Span<const ResourceHandle> texViews,
                                Span<const ResourceHandle> sampSamplers)
{
    std::vector<UBOEntryPOD> ubos(desc.uboCount);
    for (uint32_t i = 0; i < desc.uboCount; ++i)
    {
        ubos[i].slot = desc.ubos[i].slot;
        ubos[i].buffer = i < uboBuffers.size() ? uboBuffers[i] : kInvalidHandle;
        ubos[i].offset = desc.ubos[i].offset;
        ubos[i].size = desc.ubos[i].size;
    }
    std::vector<TexEntryPOD> texs(desc.textureCount);
    for (uint32_t i = 0; i < desc.textureCount; ++i)
    {
        texs[i].slot = desc.textures[i].slot;
        texs[i].view = i < texViews.size() ? texViews[i] : kInvalidHandle;
    }
    std::vector<SampEntryPOD> samps(desc.samplerCount);
    for (uint32_t i = 0; i < desc.samplerCount; ++i)
    {
        samps[i].slot = desc.samplers[i].slot;
        samps[i].sampler =
            i < sampSamplers.size() ? sampSamplers[i] : kInvalidHandle;
    }

    BindGroupDescPOD pod{};
    pod.layout = layout;
    pod.uboCount = desc.uboCount;
    pod.ubos = cb.appendBlobRef(
        ubos.data(),
        static_cast<uint32_t>(ubos.size() * sizeof(UBOEntryPOD)),
        ubos.empty());
    pod.textureCount = desc.textureCount;
    pod.textures = cb.appendBlobRef(
        texs.data(),
        static_cast<uint32_t>(texs.size() * sizeof(TexEntryPOD)),
        texs.empty());
    pod.samplerCount = desc.samplerCount;
    pod.samplers = cb.appendBlobRef(
        samps.data(),
        static_cast<uint32_t>(samps.size() * sizeof(SampEntryPOD)),
        samps.empty());
    pod.label = cb.appendStringRef(desc.label);
    cb.append(CommandType::makeBindGroup, MakeResourcePOD{id, generation});
    cb.appendPayload(pod);
}

inline void recordBufferUpdate(OreCommandBuffer& cb,
                               ResourceHandle handle,
                               const void* data,
                               uint32_t size,
                               uint32_t offset)
{
    BufferUpdatePOD pod{};
    pod.handle = handle;
    pod.offset = offset;
    pod.bytes = cb.appendBlobRef(data, size, data == nullptr);
    cb.append(CommandType::bufferUpdate, pod);
}

inline void recordTextureUpload(OreCommandBuffer& cb,
                                ResourceHandle handle,
                                const TextureDataDesc& desc)
{
    uint32_t rows = desc.rowsPerImage ? desc.rowsPerImage : desc.height;
    uint32_t size = desc.bytesPerRow * rows;
    TextureUploadPOD pod{};
    pod.handle = handle;
    pod.bytesPerRow = desc.bytesPerRow;
    pod.rowsPerImage = desc.rowsPerImage;
    pod.mipLevel = desc.mipLevel;
    pod.layer = desc.layer;
    pod.x = desc.x;
    pod.y = desc.y;
    pod.z = desc.z;
    pod.width = desc.width;
    pod.height = desc.height;
    pod.depth = desc.depth;
    pod.bytes =
        cb.appendBlobRef(desc.data, size, desc.data == nullptr || size == 0);
    cb.append(CommandType::textureUpload, pod);
}

inline void recordWrapCanvasView(
    OreCommandBuffer& cb,
    ResourceHandle id,
    uint32_t generation,
    uint32_t canvasId,
    WrapCanvasViewMode mode = WrapCanvasViewMode::colorView)
{
    cb.append(CommandType::wrapCanvasView,
              WrapCanvasViewPOD{id,
                                generation,
                                canvasId,
                                static_cast<uint32_t>(mode)});
}

inline void recordDestroyResource(OreCommandBuffer& cb,
                                  ResourceHandle handle,
                                  uint32_t generation)
{
    cb.append(CommandType::destroyResource,
              DestroyResourcePOD{handle, generation});
}

} // namespace rive::ore::cmd
