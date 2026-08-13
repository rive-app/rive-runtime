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

// Every POD below orders its fields widest first, so the layout carries no
// implicit padding and a recording never depends on what the compiler left in
// the gaps. Any trailing pad is named, and the asserts fail if a new field
// reopens a hole.
struct BufferDescPOD
{
    BlobRef data;  // initial contents, or absent
    BlobRef label; // null-terminated, or absent
    uint32_t size;
    BufferUsage usage;
    bool immutable;
    uint8_t pad[2];
};
static_assert(sizeof(BufferDescPOD) == 40, "wire POD must be padding-free");

struct TextureDescPOD
{
    BlobRef label;
    uint32_t width;
    uint32_t height;
    uint32_t depthOrArrayLayers;
    uint32_t numMipmaps;
    uint32_t sampleCount;
    TextureFormat format;
    TextureType type;
    bool renderTarget;
    uint8_t pad[1];
};
static_assert(sizeof(TextureDescPOD) == 40, "wire POD must be padding-free");

struct SamplerDescPOD
{
    BlobRef label;
    float minLod;
    float maxLod;
    uint32_t maxAnisotropy;
    Filter minFilter;
    Filter magFilter;
    Filter mipmapFilter;
    WrapMode wrapU;
    WrapMode wrapV;
    WrapMode wrapW;
    CompareFunction compare;
    uint8_t pad[5];
};
static_assert(sizeof(SamplerDescPOD) == 40, "wire POD must be padding-free");

// The size fields are recovered from each blob's size at replay.
struct ShaderModuleDescPOD
{
    BlobRef code;
    BlobRef label;
    BlobRef hlslSource;     // D3D11 runtime-compile source, or absent
    BlobRef hlslEntryPoint; // null-terminated, or absent
    BlobRef bindingMapBytes;
    BlobRef texSamplerPairBytes;
    BlobRef glFixupBytes;
    uint32_t shaderAssetId;
    ShaderLanguage language;
    ShaderStage stage;
    uint8_t pad[2];
};
static_assert(sizeof(ShaderModuleDescPOD) == 120,
              "wire POD must be padding-free");

struct BindGroupLayoutDescPOD
{
    BlobRef entries; // entryCount * sizeof(BindGroupLayoutEntry), or absent
    BlobRef label;
    uint32_t groupIndex;
    uint32_t entryCount;
};
static_assert(sizeof(BindGroupLayoutDescPOD) == 40,
              "wire POD must be padding-free");

struct TextureViewDescPOD
{
    ResourceHandle texture;
    uint32_t baseMipLevel;
    uint32_t mipCount;
    uint32_t baseLayer;
    uint32_t layerCount;
    TextureViewDimension dimension;
    TextureAspect aspect;
    uint8_t pad[2];
};
static_assert(sizeof(TextureViewDescPOD) == 24,
              "wire POD must be padding-free");

struct VertexBufferLayoutPOD
{
    BlobRef attributes; // attributeCount * sizeof(VertexAttribute)
    uint32_t stride;
    uint32_t attributeCount;
    VertexStepMode stepMode;
    uint8_t pad[7];
};
static_assert(sizeof(VertexBufferLayoutPOD) == 32,
              "wire POD must be padding-free");

// Module and layout references are client handles.
struct PipelineDescPOD
{
    BlobRef vertexEntryPoint; // null-terminated string
    BlobRef fragmentEntryPoint;
    BlobRef vertexBuffers; // vertexBufferCount * sizeof(VertexBufferLayoutPOD)
    BlobRef bindGroupLayouts; // bindGroupLayoutCount * sizeof(ResourceHandle)
    BlobRef label;

    ResourceHandle vertexModule;
    ResourceHandle fragmentModule;
    uint32_t vertexBufferCount;
    uint32_t colorCount;
    uint32_t sampleCount;
    uint32_t bindGroupLayoutCount;

    DepthStencilState depthStencil;

    ColorTargetState colorTargets[4];
    StencilFaceState stencilFront;
    StencilFaceState stencilBack;

    PrimitiveTopology topology;
    IndexFormat indexFormat;
    CullMode cullMode;
    FaceWinding winding;
    uint8_t stencilReadMask;
    uint8_t stencilWriteMask;
    uint8_t pad[6];
};
static_assert(sizeof(PipelineDescPOD) == 176, "wire POD must be padding-free");

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
    BlobRef ubos;     // uboCount * sizeof(UBOEntryPOD)
    BlobRef textures; // textureCount * sizeof(TexEntryPOD)
    BlobRef samplers; // samplerCount * sizeof(SampEntryPOD)
    BlobRef label;
    ResourceHandle layout;
    uint32_t uboCount;
    uint32_t textureCount;
    uint32_t samplerCount;
};
static_assert(sizeof(BindGroupDescPOD) == 80, "wire POD must be padding-free");

} // namespace rive::ore::cmd
