#ifndef _RIVE_MANIFEST_FLAGS_HPP_
#define _RIVE_MANIFEST_FLAGS_HPP_

#include <cstdint>
#include <cstddef>
#include <vector>

namespace rive
{
enum class ManifestSections : unsigned char
{
    names = 0,
    paths = 1,
    watermark = 2,
};

/// Payload layout version of the watermark section. A section written by a
/// newer exporter is ignored rather than misread.
constexpr uint64_t watermarkSectionVersion = 1;
/// Bit 0 of the watermark section's flags: the file carries a watermark.
constexpr uint64_t watermarkFlagEnabled = 0x1;

namespace manifest_detail
{
/// Mirrors BinaryWriter::writeVarUint. Spelled out here so this header stays
/// dependency-free and usable from rml, which does not always link the
/// runtime.
inline void appendVarUint(std::vector<uint8_t>& out, uint64_t value)
{
    do
    {
        uint8_t byte = (uint8_t)(value & 0x7F);
        value >>= 7;
        if (value != 0)
        {
            byte |= 0x80;
        }
        out.push_back(byte);
    } while (value != 0);
}
} // namespace manifest_detail

/// Appends the watermark section to a manifest payload, in the shape
/// ManifestAsset::decodeWatermark reads back:
///
///     varuint sectionId, varuint sectionSize,
///         [ varuint version, varuint flags, varuint artboardIndex ]
///
/// Ported from ManifestAsset._writeWatermark in
/// packages/rive_core/lib/assets/manifest_asset.dart, the editor's exporter.
/// Both writers have to agree byte for byte, so keep this the only C++ copy of
/// the layout.
inline void writeWatermarkManifestSection(uint32_t artboardIndex,
                                          std::vector<uint8_t>& out)
{
    using namespace manifest_detail;
    std::vector<uint8_t> section;
    appendVarUint(section, watermarkSectionVersion);
    appendVarUint(section, watermarkFlagEnabled);
    appendVarUint(section, artboardIndex);

    appendVarUint(out, (uint64_t)ManifestSections::watermark);
    appendVarUint(out, section.size());
    out.insert(out.end(), section.begin(), section.end());
}
} // namespace rive
#endif
