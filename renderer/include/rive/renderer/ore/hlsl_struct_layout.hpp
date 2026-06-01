/*
 * Copyright 2026 Rive
 */

#pragma once

// HLSL struct-layout sidecar (RSTB target = 17).
//
// Carries struct + resource layout extracted from SPIRV-Cross at HLSL bake
// time, so Unreal RHI can build `FShaderParametersMetadata` from baked
// HLSL without re-parsing or duplicating SPIRV-Cross's type-name logic.
//
// Wire format is purely numeric (no per-member type-name strings) — the
// consumer reconstructs `float4` / `float2x3` / `Texture2D<float4>` from
// (base_type, num_rows, num_columns, texture_view_dim, …). Struct names
// ARE preserved because Unreal references nested structs by name.
//
// Parser and register stripper are always compiled. Dead-code elimination
// strips them when no caller references them, so non-Unreal binaries pay
// nothing for the support. Compiling unconditionally lets unit tests
// exercise the parser without depending on the `for_unreal` premake gate.
//
// Wire format (little-endian, strings are u16 length + bytes, no NUL):
//
// clang-format off
//   Header (12 bytes):
//     magic            u32LE = 0x544C5348 (bytes 'H','S','L','T' on disk)
//     version          u16  = 2
//     reserved         u16  = 0
//     struct_count     u16
//     resource_count   u16
//
//   Struct table (struct_count records, topologically sorted so a member's
//   struct_index always points to an earlier index):
//     name             str
//     declared_size    u32  // bytes (HLSL alignment)
//     member_count     u16
//     for each member:
//       name           str
//       offset         u32
//       base_type      u8   // HLSLStructLayoutBaseType
//       num_rows       u8
//       num_columns    u8
//       flags          u8   // bit0 = isArray; bits 1..7 reserved
//       array_size     u32  // 0 if not array
//       array_stride   u32  // 0 if not array
//       matrix_stride  u32  // 0 if not matrix
//       struct_index   i32  // -1 unless base_type == Struct
//
//   Resource table (resource_count records):
//     name             str  // HLSL variable name
//     kind             u8   // rive::ore::ResourceKind
//     texture_view_dim u8   // rive::ore::TextureViewDim (Undefined unless texture)
//     texture_sample_type u8 // rive::ore::TextureSampleType (Undefined unless texture)
//     texture_multisampled u8
//     stage_mask       u8   // bitwise-OR of BindingMap::kStage* (VS|FS|CS)
//     struct_index     i32  // for UniformBuffer / StorageBuffer*: into struct table; else -1
// clang-format on
//
// v1 → v2: added `stage_mask`. v1 blobs are rejected by `fromBlob` (the
// version is checked exactly, not as a floor) — there are no v1 consumers
// in the wild yet, so this is a clean break rather than a compat shim.
//
// Resources are keyed by HLSL variable name. The consumer (Unreal RHI)
// joins these entries to its `FShaderParametersMetadataBuilder` calls by
// name. Stage visibility is carried directly on each resource via
// `stageMask` (Unreal needs this to feed `EShaderFrequency` /
// `Shader_Type_Visibility` into the parameter metadata); WGSL
// (@group, @binding) info still lives in the separate BindingMap sidecar
// (RSTB target=12) and is not duplicated here.

#include "rive/renderer/ore/ore_binding_map.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace rive::ore
{

// Wire format version + magic. The writer in scripting_workspace and the
// reader here both consume these — keep them in sync via this single
// header.
constexpr uint32_t kHLSLStructLayoutMagic = 0x544C5348u; // 'HSLT' bytes
constexpr uint16_t kHLSLStructLayoutVersion = 2;

// Mirrors `spirv_cross::SPIRType::BaseType` for the subset that survives
// HLSL emission. Frozen on-disk values — never renumber.
enum class HLSLStructLayoutBaseType : uint8_t
{
    Unknown = 0,
    Bool = 1,
    Int = 2,
    UInt = 3,
    Half = 4,
    Float = 5,
    Double = 6,
    Struct = 7,
};

// Bit flags inside HLSLStructLayoutMember::flags.
constexpr uint8_t kHLSLStructLayoutMemberFlagArray = 1u << 0;

struct HLSLStructLayoutMember
{
    std::string name;
    uint32_t offset = 0;
    HLSLStructLayoutBaseType baseType = HLSLStructLayoutBaseType::Unknown;
    uint8_t numRows = 1;
    uint8_t numColumns = 1;
    bool isArray = false;
    uint32_t arraySize = 0;    // 0 if not array
    uint32_t arrayStride = 0;  // 0 if not array
    uint32_t matrixStride = 0; // 0 if not matrix
    int32_t structIndex = -1;  // -1 unless baseType == Struct
};

struct HLSLStructLayoutStruct
{
    std::string name;
    uint32_t declaredSize = 0;
    std::vector<HLSLStructLayoutMember> members;
};

struct HLSLStructLayoutResource
{
    std::string name;
    ResourceKind kind = ResourceKind::UniformBuffer;
    TextureViewDim textureViewDim = TextureViewDim::Undefined;
    TextureSampleType textureSampleType = TextureSampleType::Undefined;
    bool textureMultisampled = false;
    uint8_t stageMask = 0;    // bitwise-OR of BindingMap::kStage* (VS|FS|CS)
    int32_t structIndex = -1; // index into HLSLStructLayout::structs for
                              // UniformBuffer / StorageBuffer* kinds
};

class HLSLStructLayout
{
public:
    std::vector<HLSLStructLayoutStruct> structs;
    std::vector<HLSLStructLayoutResource> resources;

    // Parse a target=17 sidecar blob. Returns false on bad magic,
    // unsupported version, truncated input, or out-of-range indices.
    static bool fromBlob(const uint8_t* data,
                         size_t size,
                         HLSLStructLayout* out);
};

// Strip `: register(<class><N>[, space<M>])` annotations from
// SPIRV-Cross-emitted HLSL. The rive D3D runtime needs the registers
// (they agree with the binding-map allocator). The Unreal RHI assigns
// its own descriptor slots and rejects HLSL with hardcoded registers;
// this helper lets the Unreal plugin load target=3 HLSL and produce
// register-free source at load time.
//
// SPIRV-Cross emits `: register(...)` only on top-level resource
// declarations (cbuffer / Texture* / SamplerState / *Buffer). Cbuffer
// member offsets use `: packoffset(...)` and are left untouched.
//
// Returns the input unchanged if no register clauses are present.
std::string stripHLSLRegisterAnnotations(const std::string& hlsl);

// -- HLSL type-string reconstruction -----------------------------------------
//
// The wire format is purely numeric to keep the sidecar compact. These
// helpers reconstruct the HLSL type-name string that Unreal's
// `FShaderParametersMetadataBuilder::Add{BufferSRV,Texture,Sampler,…}`
// methods expect as their `ShaderType` argument. The Unreal plugin can
// either call these directly to populate `ShaderType`, or re-implement
// equivalent logic inline.

// Returns the HLSL type string for a struct member ("float", "float4",
// "float2x3", "uint", "bool", or the struct's HLSL name for a nested
// struct member). Empty string on Unknown base type.
std::string hlslTypeStringForMember(const HLSLStructLayoutMember& member,
                                    const HLSLStructLayout& layout);

// Returns the HLSL type string for a resource ("Texture2D<float4>",
// "TextureCubeArray<uint4>", "Texture2DMS<float4>", "SamplerState",
// "SamplerComparisonState", "StructuredBuffer<MyStruct>",
// "RWStructuredBuffer<MyStruct>"; the cbuffer block's struct name for
// UniformBuffer kind). The format matches the ShaderType string Unreal
// uses in `AddTexture` / `AddSampler` / `AddBufferSRV` / `AddBufferUAV`
// / `AddNestedStruct` / `AddReferencedStruct`.
std::string hlslTypeStringForResource(const HLSLStructLayoutResource& resource,
                                      const HLSLStructLayout& layout);

} // namespace rive::ore
