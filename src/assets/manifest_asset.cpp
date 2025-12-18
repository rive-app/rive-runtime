#include "rive/assets/manifest_asset.hpp"
#include "rive/factory.hpp"
#include "rive/core/binary_reader.hpp"
#include "rive/span.hpp"
#include "rive/manifest_sections.hpp"

using namespace rive;

const std::string ManifestAsset::empty;

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

        if (static_cast<ManifestSections>(sectionValue) ==
            ManifestSections::names)
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