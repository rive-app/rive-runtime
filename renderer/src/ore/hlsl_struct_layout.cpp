/*
 * Copyright 2026 Rive
 */

#include "rive/renderer/ore/hlsl_struct_layout.hpp"

// Parser + register stripper are always compiled. Dead-code elimination
// strips them when no caller references them, so non-Unreal binaries pay
// nothing for the support. Compiling unconditionally lets unit tests
// exercise the parser without depending on the `for_unreal` premake gate.
#include <cstring>
#include <regex>

namespace rive::ore
{

namespace
{

inline uint16_t readU16LE(const uint8_t* p)
{
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

inline uint32_t readU32LE(const uint8_t* p)
{
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

inline int32_t readI32LE(const uint8_t* p)
{
    return static_cast<int32_t>(readU32LE(p));
}

// Read a u16-length-prefixed string at *p. Advances *p past the string on
// success. Returns false if the string would run past `end`.
bool readString(const uint8_t** p, const uint8_t* end, std::string* out)
{
    if (*p + 2 > end)
        return false;
    const uint16_t len = readU16LE(*p);
    *p += 2;
    if (*p + len > end)
        return false;
    out->assign(reinterpret_cast<const char*>(*p), len);
    *p += len;
    return true;
}

} // namespace

bool HLSLStructLayout::fromBlob(const uint8_t* data,
                                size_t size,
                                HLSLStructLayout* out)
{
    if (out == nullptr || data == nullptr)
        return false;
    out->structs.clear();
    out->resources.clear();

    constexpr size_t kHeaderSize = 12;
    if (size < kHeaderSize)
        return false;

    if (readU32LE(data) != kHLSLStructLayoutMagic)
        return false;
    if (readU16LE(data + 4) != kHLSLStructLayoutVersion)
        return false;
    // bytes [6..8) reserved
    const uint16_t structCount = readU16LE(data + 8);
    const uint16_t resourceCount = readU16LE(data + 10);

    const uint8_t* p = data + kHeaderSize;
    const uint8_t* end = data + size;

    out->structs.reserve(structCount);
    for (uint16_t s = 0; s < structCount; ++s)
    {
        HLSLStructLayoutStruct st;
        if (!readString(&p, end, &st.name))
            return false;
        if (p + 4 + 2 > end)
            return false;
        st.declaredSize = readU32LE(p);
        p += 4;
        const uint16_t memberCount = readU16LE(p);
        p += 2;

        st.members.reserve(memberCount);
        for (uint16_t m = 0; m < memberCount; ++m)
        {
            HLSLStructLayoutMember mb;
            if (!readString(&p, end, &mb.name))
                return false;
            // 4 (offset) + 4 (base_type/rows/cols/flags) + 4 + 4 + 4 + 4
            constexpr size_t kMemberFixed = 24;
            if (p + kMemberFixed > end)
                return false;
            mb.offset = readU32LE(p);
            p += 4;
            mb.baseType = static_cast<HLSLStructLayoutBaseType>(p[0]);
            mb.numRows = p[1];
            mb.numColumns = p[2];
            const uint8_t flags = p[3];
            mb.isArray = (flags & kHLSLStructLayoutMemberFlagArray) != 0;
            p += 4;
            mb.arraySize = readU32LE(p);
            p += 4;
            mb.arrayStride = readU32LE(p);
            p += 4;
            mb.matrixStride = readU32LE(p);
            p += 4;
            mb.structIndex = readI32LE(p);
            p += 4;
            // Schema invariant: struct_index for Struct members must
            // reference a strictly-earlier table entry (topological order
            // guarantees children are visited before parents).
            if (mb.baseType == HLSLStructLayoutBaseType::Struct)
            {
                if (mb.structIndex < 0 ||
                    static_cast<size_t>(mb.structIndex) >= s)
                    return false;
            }
            st.members.push_back(std::move(mb));
        }
        out->structs.push_back(std::move(st));
    }

    out->resources.reserve(resourceCount);
    for (uint16_t r = 0; r < resourceCount; ++r)
    {
        HLSLStructLayoutResource res;
        if (!readString(&p, end, &res.name))
            return false;
        // 1 (kind) + 1 (texture_view_dim) + 1 (texture_sample_type) +
        // 1 (texture_multisampled) + 4 (struct_index) = 8 bytes
        constexpr size_t kResourceFixed = 8;
        if (p + kResourceFixed > end)
            return false;
        res.kind = static_cast<ResourceKind>(p[0]);
        res.textureViewDim = static_cast<TextureViewDim>(p[1]);
        res.textureSampleType = static_cast<TextureSampleType>(p[2]);
        res.textureMultisampled = (p[3] != 0);
        p += 4;
        res.structIndex = readI32LE(p);
        p += 4;
        if (res.structIndex >= 0 &&
            static_cast<size_t>(res.structIndex) >= out->structs.size())
            return false;
        out->resources.push_back(std::move(res));
    }

    return true;
}

namespace
{

const char* baseScalarHLSLName(HLSLStructLayoutBaseType t)
{
    switch (t)
    {
        case HLSLStructLayoutBaseType::Bool:
            return "bool";
        case HLSLStructLayoutBaseType::Int:
            return "int";
        case HLSLStructLayoutBaseType::UInt:
            return "uint";
        case HLSLStructLayoutBaseType::Half:
            return "half";
        case HLSLStructLayoutBaseType::Float:
            return "float";
        case HLSLStructLayoutBaseType::Double:
            return "double";
        case HLSLStructLayoutBaseType::Struct:
        case HLSLStructLayoutBaseType::Unknown:
            return "";
    }
    return "";
}

const char* textureViewDimHLSLName(TextureViewDim d, bool multisampled)
{
    using D = TextureViewDim;
    if (multisampled)
    {
        // MS variants only exist for 2D / 2DArray. Cube / 3D MS aren't a
        // thing in HLSL.
        if (d == D::D2Array)
            return "Texture2DMSArray";
        return "Texture2DMS";
    }
    switch (d)
    {
        case D::D1:
            return "Texture1D";
        case D::D2:
            return "Texture2D";
        case D::D2Array:
            return "Texture2DArray";
        case D::Cube:
            return "TextureCube";
        case D::CubeArray:
            return "TextureCubeArray";
        case D::D3:
            return "Texture3D";
        case D::Undefined:
            return "Texture2D"; // safe default; HLSL most common
    }
    return "Texture2D";
}

const char* textureSampleTypeHLSLName(TextureSampleType t)
{
    using ST = TextureSampleType;
    switch (t)
    {
        case ST::Float:
        case ST::UnfilterableFloat:
        case ST::Depth:
            return "float4";
        case ST::Sint:
            return "int4";
        case ST::Uint:
            return "uint4";
        case ST::Undefined:
            return "float4";
    }
    return "float4";
}

} // namespace

std::string hlslTypeStringForMember(const HLSLStructLayoutMember& member,
                                    const HLSLStructLayout& layout)
{
    if (member.baseType == HLSLStructLayoutBaseType::Struct)
    {
        if (member.structIndex < 0 ||
            static_cast<size_t>(member.structIndex) >= layout.structs.size())
            return "";
        return layout.structs[static_cast<size_t>(member.structIndex)].name;
    }
    const char* base = baseScalarHLSLName(member.baseType);
    if (*base == 0)
        return "";
    // Scalar: just "float" / "uint" / etc.
    if (member.numRows <= 1 && member.numColumns <= 1)
        return base;
    // Vector: "floatN" where N is the column count.
    if (member.numRows <= 1)
        return std::string(base) + std::to_string(member.numColumns);
    // Matrix: "floatRxC".
    return std::string(base) + std::to_string(member.numRows) + "x" +
           std::to_string(member.numColumns);
}

std::string hlslTypeStringForResource(const HLSLStructLayoutResource& resource,
                                      const HLSLStructLayout& layout)
{
    switch (resource.kind)
    {
        case ResourceKind::UniformBuffer:
        {
            // Unreal references the cbuffer's block struct directly by
            // name (AddIncludedStruct / AddNestedStruct take struct
            // metadata, not a string). Return the struct's HLSL name so
            // the caller can match it against `layout.structs`.
            if (resource.structIndex >= 0 &&
                static_cast<size_t>(resource.structIndex) <
                    layout.structs.size())
            {
                return layout.structs[static_cast<size_t>(resource.structIndex)]
                    .name;
            }
            return "";
        }
        case ResourceKind::StorageBufferRO:
        {
            std::string inner = "float4"; // safe default
            if (resource.structIndex >= 0 &&
                static_cast<size_t>(resource.structIndex) <
                    layout.structs.size())
            {
                inner =
                    layout.structs[static_cast<size_t>(resource.structIndex)]
                        .name;
            }
            return "StructuredBuffer<" + inner + ">";
        }
        case ResourceKind::StorageBufferRW:
        {
            std::string inner = "float4";
            if (resource.structIndex >= 0 &&
                static_cast<size_t>(resource.structIndex) <
                    layout.structs.size())
            {
                inner =
                    layout.structs[static_cast<size_t>(resource.structIndex)]
                        .name;
            }
            return "RWStructuredBuffer<" + inner + ">";
        }
        case ResourceKind::SampledTexture:
            return std::string(
                       textureViewDimHLSLName(resource.textureViewDim,
                                              resource.textureMultisampled)) +
                   "<" + textureSampleTypeHLSLName(resource.textureSampleType) +
                   ">";
        case ResourceKind::StorageTexture:
        {
            // RW variants of texture types; SPIR-V doesn't carry the
            // exact texel format here, so default to float4 — the Unreal
            // RHI can override based on its own reflection if it cares.
            const char* dim =
                textureViewDimHLSLName(resource.textureViewDim, false);
            // Texture2D -> RWTexture2D, Texture3D -> RWTexture3D, etc.
            return std::string("RW") + dim + "<" +
                   textureSampleTypeHLSLName(resource.textureSampleType) + ">";
        }
        case ResourceKind::Sampler:
            return "SamplerState";
        case ResourceKind::ComparisonSampler:
            return "SamplerComparisonState";
    }
    return "";
}

std::string stripHLSLRegisterAnnotations(const std::string& hlsl)
{
    // Pattern: optional whitespace, ':' , optional whitespace, the literal
    // "register(", anything that isn't ')', closing ')'. Replaced with
    // empty string globally. See the design note in the .hpp header for
    // the false-positive analysis — SPIRV-Cross-emitted HLSL has no other
    // ':' annotation that could match this shape.
    //
    // ECMAScript regex (std::regex default); `\s` matches newlines so
    // multi-line declarations are handled correctly.
    static const std::regex kRegisterAnnotation(R"(\s*:\s*register\([^)]*\))");
    return std::regex_replace(hlsl, kRegisterAnnotation, std::string());
}

} // namespace rive::ore
