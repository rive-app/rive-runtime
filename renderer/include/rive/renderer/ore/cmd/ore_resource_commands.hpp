/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/renderer/ore/ore_types.hpp"
#include "rive/renderer/ore/cmd/ore_handle.hpp"
#include <cstdint>

// Recorded form of the ore Context make* calls. References between resources
// are recorded as client handles, so recording order is a valid creation order
// by construction. Variable length data lives in a companion blob arena.
namespace rive::ore::cmd
{

// size == kAbsent means the source field was null, distinct from an empty but
// present payload. Offsets are 64 bit so never-reset streams outlive 4 GiB of
// cumulative appends.
struct BlobRef
{
    uint64_t offset;
    uint32_t size;
    uint32_t pad; // explicit so the wire layout carries no implicit padding
    static constexpr uint32_t kAbsent = ~0u;
    bool absent() const { return size == kAbsent; }
};
constexpr BlobRef kNoBlob = {0, BlobRef::kAbsent, 0};

struct BufferDescPOD
{
    BufferUsage usage;
    uint32_t size;
    bool immutable;
    BlobRef data;  // initial contents, or absent
    BlobRef label; // null-terminated, or absent
};

struct TextureDescPOD
{
    uint32_t width;
    uint32_t height;
    uint32_t depthOrArrayLayers;
    TextureFormat format;
    TextureType type;
    bool renderTarget;
    uint32_t numMipmaps;
    uint32_t sampleCount;
    BlobRef label;
};

struct SamplerDescPOD
{
    Filter minFilter;
    Filter magFilter;
    Filter mipmapFilter;
    WrapMode wrapU;
    WrapMode wrapV;
    WrapMode wrapW;
    CompareFunction compare;
    float minLod;
    float maxLod;
    uint32_t maxAnisotropy;
    BlobRef label;
};

// The size fields are recovered from each blob's size at replay.
struct ShaderModuleDescPOD
{
    BlobRef code;
    ShaderLanguage language;
    ShaderStage stage;
    BlobRef label;
    BlobRef hlslSource;     // D3D11 runtime-compile source, or absent
    BlobRef hlslEntryPoint; // null-terminated, or absent
    BlobRef bindingMapBytes;
    BlobRef texSamplerPairBytes;
    BlobRef glFixupBytes;
    uint32_t shaderAssetId;
};

struct BindGroupLayoutDescPOD
{
    uint32_t groupIndex;
    BlobRef entries; // entryCount * sizeof(BindGroupLayoutEntry), or absent
    uint32_t entryCount;
    BlobRef label;
};

struct TextureViewDescPOD
{
    ResourceHandle texture;
    TextureViewDimension dimension;
    TextureAspect aspect;
    uint32_t baseMipLevel;
    uint32_t mipCount;
    uint32_t baseLayer;
    uint32_t layerCount;
};

struct VertexBufferLayoutPOD
{
    uint32_t stride;
    VertexStepMode stepMode;
    uint32_t attributeCount;
    BlobRef attributes; // attributeCount * sizeof(VertexAttribute)
};

// Module and layout references are client handles.
struct PipelineDescPOD
{
    ResourceHandle vertexModule;
    BlobRef vertexEntryPoint; // null-terminated string
    ResourceHandle fragmentModule;
    BlobRef fragmentEntryPoint;

    BlobRef vertexBuffers; // vertexBufferCount * sizeof(VertexBufferLayoutPOD)
    uint32_t vertexBufferCount;

    PrimitiveTopology topology;
    IndexFormat indexFormat;
    CullMode cullMode;
    FaceWinding winding;

    ColorTargetState colorTargets[4];
    uint32_t colorCount;

    DepthStencilState depthStencil;
    StencilFaceState stencilFront;
    StencilFaceState stencilBack;
    uint8_t stencilReadMask;
    uint8_t stencilWriteMask;

    uint32_t sampleCount;

    BlobRef bindGroupLayouts; // bindGroupLayoutCount * sizeof(ResourceHandle)
    uint32_t bindGroupLayoutCount;

    BlobRef label;
};

// BindGroupDesc entries with resource pointers replaced by client handles.
struct UBOEntryPOD
{
    uint32_t slot;
    ResourceHandle buffer;
    uint32_t offset;
    uint32_t size;
};
struct TexEntryPOD
{
    uint32_t slot;
    ResourceHandle view;
};
struct SampEntryPOD
{
    uint32_t slot;
    ResourceHandle sampler;
};

struct BindGroupDescPOD
{
    ResourceHandle layout;
    BlobRef ubos; // uboCount * sizeof(UBOEntryPOD)
    uint32_t uboCount;
    BlobRef textures; // textureCount * sizeof(TexEntryPOD)
    uint32_t textureCount;
    BlobRef samplers; // samplerCount * sizeof(SampEntryPOD)
    uint32_t samplerCount;
    BlobRef label;
};

} // namespace rive::ore::cmd
