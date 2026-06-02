#ifndef _RIVE_SHADER_ASSET_HPP_
#define _RIVE_SHADER_ASSET_HPP_
#include "rive/generated/assets/shader_asset_base.hpp"
#include "rive/simple_array.hpp"
#include "rive/span.hpp"
#include <cstdint>
#include <unordered_map>

namespace rive
{
/// Asset holding one compiled shader in RSTB (Rive Shader Table Binary) v4.
/// Each ShaderAsset corresponds 1:1 to a WGSL source. The "table" is the
/// per-backend-target variant table (MSL, HLSL, GLSL, SPIR-V, WGSL passthrough)
/// plus the binding-map and GL-fixup sidecars.
///
/// RSTB v4 layout:
///   Header (8 bytes): magic(u32LE=0x52535442) + version(u16LE=4) +
///                     variant_count(u8) + section_count(u8)
///   Per variant: target(u8) + blob_offset(u32LE) + blob_size(u32LE)
///   Per section: tag(u8) + length(u16LE) + data[length]
///   Blob data section (blob_offset values are relative to this section)
///
/// v4 wraps each source variant in an entry-point container carrying the
/// @vertex/@fragment names (see ore_rstb_entry_container.hpp) so the runtime
/// resolves entry points by name instead of assuming vs_main/fs_main.
///
/// ShaderTarget enum (u8):
///   0 = WGSL passthrough, 1 = GLSL ES3, 2 = MSL, 3 = HLSL SM5, 5 = SPIR-V
///   10..13, 16 = binding-map sidecars (per source target)
///   14, 15 = GL program-link fixup tables (VS, FS)
class ShaderAsset : public ShaderAssetBase
{
public:
    bool decode(SimpleArray<uint8_t>& data, Factory* factory) override
    {
        return decode(Span<uint8_t>(data.data(), data.size()), factory);
    }

    bool decode(Span<uint8_t> data, Factory* factory);
    std::string fileExtension() const override { return "rstb"; }

    /// Returns the blob for the given target, or an empty span if not present.
    /// The span is valid for the lifetime of this asset.
    Span<const uint8_t> findShader(uint8_t target) const;

    /// Texture-sampler pair from shader reflection.
    struct TextureSamplerPair
    {
        uint8_t texGroup;
        uint8_t texBinding;
        uint8_t sampGroup;
        uint8_t sampBinding;
    };

    /// Texture-sampler pairs decoded from the RSTB tagged-section payload.
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
    // key = target
    std::unordered_map<uint8_t, ShaderVariant> m_index;

    std::vector<TextureSamplerPair> m_pairs;
};
} // namespace rive

#endif
