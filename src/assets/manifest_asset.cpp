#include "rive/assets/manifest_asset.hpp"
#include "rive/factory.hpp"
#include "rive/core/binary_reader.hpp"
#include "rive/span.hpp"
#include "rive/manifest_sections.hpp"

using namespace rive;

const std::string ManifestAsset::empty;
const std::vector<uint32_t> ManifestAsset::emptyIntVector;

bool ManifestAsset::decodeNames(BinaryReader& reader)
{
    // Read count of names
    uint64_t count = reader.readVarUint64();
    if (reader.hasError())
    {
        return false;
    }

    // Read all name entries
    for (uint64_t i = 0; i < count; i++)
    {
        int id = static_cast<int>(reader.readVarUint64());
        if (reader.hasError())
        {
            return false;
        }

        std::string value = reader.readString();
        if (reader.hasError())
        {
            return false;
        }

        m_names[id] = value;
    }
    return true;
}

bool ManifestAsset::decodePaths(BinaryReader& reader)
{
    // Read count of paths
    uint64_t count = reader.readVarUint64();
    if (reader.hasError())
    {
        return false;
    }

    // Read all path entries
    for (uint64_t i = 0; i < count; i++)
    {
        int id = static_cast<int>(reader.readVarUint64());
        if (reader.hasError())
        {
            return false;
        }
        int pathLength = static_cast<int>(reader.readVarUint64());
        if (reader.hasError())
        {
            return false;
        }
        std::vector<uint32_t> path;
        for (uint64_t j = 0; j < pathLength; j++)
        {
            int pathId = static_cast<uint32_t>(reader.readVarUint64());
            path.push_back(pathId);
        }
        m_paths[id] = path;
        if (reader.hasError())
        {
            return false;
        }
    }
    return true;
}

bool ManifestAsset::decodeWatermark(BinaryReader& reader, uint64_t sectionSize)
{
    const uint8_t* sectionStart = reader.position();

    uint64_t version = reader.readVarUint64();
    if (reader.hasError())
    {
        return false;
    }

    if (version == watermarkSectionVersion)
    {
        uint64_t flags = reader.readVarUint64();
        if (reader.hasError())
        {
            return false;
        }
        // Read as uint32_t rather than truncating a uint64_t: an index that
        // does not fit is a malformed manifest, and silently wrapping it (2^32
        // becomes 0) would attach some unrelated artboard as the watermark.
        // readVarUintAs sets the reader's range error, which the check below
        // turns into a rejected section.
        uint32_t artboardIndex = reader.readVarUintAs<uint32_t>();
        if (reader.hasError())
        {
            return false;
        }
        m_hasWatermark = (flags & watermarkFlagEnabled) != 0;
        m_watermarkArtboardIndex = artboardIndex;
    }
    // An unrecognized version leaves the watermark unset, but the section is
    // still consumed below so the rest of the manifest keeps parsing.

    // Unlike names and paths, this section tolerates trailing bytes: a newer
    // exporter may append fields to it, and decode() requires each known
    // section be consumed exactly. Swallow whatever is left.
    size_t bytesRead = static_cast<size_t>(reader.position() - sectionStart);
    if (bytesRead > sectionSize)
    {
        return false;
    }
    reader.readBytes(sectionSize - bytesRead);
    return !reader.hasError();
}

bool ManifestAsset::decode(SimpleArray<uint8_t>& bytes, Factory* factory)
{
    if (bytes.empty())
    {
        return true;
    }

    BinaryReader reader(Span<const uint8_t>(bytes.data(), bytes.size()));

    while (!reader.reachedEnd())
    {
        // Read section enum value
        uint64_t sectionValue = reader.readVarUint64();
        if (reader.hasError())
        {
            return false;
        }

        // Read section size
        uint64_t sectionSize = reader.readVarUint64();
        if (reader.hasError())
        {
            return false;
        }

        // Store the position before reading the section content
        const uint8_t* sectionStart = reader.position();

        // Compare the raw id, never a narrowed one: ManifestSections has an
        // unsigned char base, so casting down aliases every id mod 256 onto a
        // known section (258 lands on watermark, 256 on names, 257 on paths).
        // That both misreads malformed input and defeats the skip-unknown
        // branch below, which is what lets a newer exporter add sections.
        if (sectionValue == static_cast<uint64_t>(ManifestSections::names))
        {
            if (!decodeNames(reader))
            {
                return false;
            }
        }
        else if (sectionValue == static_cast<uint64_t>(ManifestSections::paths))
        {
            if (!decodePaths(reader))
            {
                return false;
            }
        }
        else if (sectionValue ==
                 static_cast<uint64_t>(ManifestSections::watermark))
        {
            if (!decodeWatermark(reader, sectionSize))
            {
                return false;
            }
        }
        else
        {
            // Unknown section - skip the specified number of bytes
            reader.readBytes(static_cast<size_t>(sectionSize));
            if (reader.hasError())
            {
                return false;
            }
            continue;
        }

        // Verify we read exactly the section size
        size_t bytesRead = reader.position() - sectionStart;
        if (bytesRead != sectionSize)
        {
            return false;
        }
    }

    return true;
}

std::string ManifestAsset::fileExtension() const { return "man"; }

const std::string& ManifestAsset::resolveName(int id)
{
    auto it = m_names.find(id);
    if (it != m_names.end())
    {
        return it->second;
    }
    return empty;
}

const std::vector<uint32_t>& ManifestAsset::resolvePath(int id)
{
    auto it = m_paths.find(id);
    if (it != m_paths.end())
    {
        return it->second;
    }
    return emptyIntVector;
}