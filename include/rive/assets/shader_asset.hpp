#ifndef _RIVE_SHADER_ASSET_HPP_
#define _RIVE_SHADER_ASSET_HPP_
#include "rive/generated/assets/shader_asset_base.hpp"
#include "rive/simple_array.hpp"
#include "rive/span.hpp"
#include <cstdint>
#include <unordered_map>

namespace rive
{
/// Asset holding a compiled shader in RSTB (Rive Shader Table Binary) format.
/// Each ShaderAsset corresponds 1:1 to a WGSL source; the "table" is the
/// per-backend-target variant table inside the RSTB blob (MSL, HLSL, GLSL,
/// SPIR-V, WGSL passthrough).
///
/// RSTB layout (version 2):
///   Header (8 bytes): magic(u32LE=0x52535442) + version(u16LE=2) +
///                     shader_count(u16LE)
///   Per shader entry: shader_id(u32LE) + variant_count(u8) +
///                     section_count(u8)
///     Per variant:    target(u8) + blob_offset(u32LE) + blob_size(u32LE)
///     Per section:    tag(u8) + length(u16LE) + data[length]
///   Blob data section (blob_offset values are relative to this section)
///
/// ShaderTarget enum (u8):
///   0 = WGSL passthrough, 1 = GLSL ES3, 2 = MSL, 3 = HLSL SM5,
///   5 = SPIR-V
///   10..13, 16 = binding-map sidecars (per source target)
///   14, 15 = GL program-link fixup tables (VS, FS)
class ShaderAsset : public ShaderAssetBase
{
public:
    bool decode(SimpleArray<uint8_t>& data, Factory* factory) override;
    std::string fileExtension() const override { return "rstb"; }

    /// Returns the blob for the given (shaderId, target) pair, or an empty
    /// span if not found. The returned span is valid for the lifetime of this
    /// asset.
    Span<const uint8_t> findShader(uint32_t shaderId, uint8_t target) const;

    /// Texture-sampler pair from shader reflection.
    struct TextureSamplerPair
    {
        uint8_t texGroup;
        uint8_t texBinding;
        uint8_t sampGroup;
        uint8_t sampBinding;
    };

    /// Returns texture-sampler pairs decoded from the RSTB.
    /// Empty for v1 RSTBs or shaders without combined samplers.
    Span<const TextureSamplerPair> textureSamplerPairs() const
    {
        return Span<const TextureSamplerPair>(m_pairs.data(), m_pairs.size());
    }

private:
    SimpleArray<uint8_t> m_bytes;

    struct ShaderVariant
    {
        uint32_t offset; // absolute byte offset into m_bytes
        uint32_t size;
    };
    // key = (shaderId << 8) | target
    std::unordered_map<uint64_t, ShaderVariant> m_index;

    std::vector<TextureSamplerPair> m_pairs;
};
} // namespace rive

#endif
