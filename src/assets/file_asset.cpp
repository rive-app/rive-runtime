#include <sstream>
#include <iomanip>
#include <array>

#include "rive/assets/file_asset.hpp"
#include "rive/backboard.hpp"
#include "rive/importers/backboard_importer.hpp"

using namespace rive;

StatusCode FileAsset::import(ImportStack& importStack)
{
    auto backboardImporter = importStack.latest<BackboardImporter>(Backboard::typeKey);
    if (backboardImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }
    backboardImporter->addFileAsset(this);

    return Super::import(importStack);
}

std::string FileAsset::uniqueName() const
{
    // remove final extension
    std::string uniqueName = name();
    std::size_t finalDot = uniqueName.rfind('.');

    if (finalDot != std::string::npos)
    {
        uniqueName = uniqueName.substr(0, finalDot);
    }
    return uniqueName + "-" + std::to_string(assetId());
}

std::string FileAsset::uniqueFilename() const { return uniqueName() + "." + fileExtension(); }

void FileAsset::copyCdnUuid(const FileAssetBase& object)
{
    // Should never be called.
    assert(false);
}

void FileAsset::decodeCdnUuid(Span<const uint8_t> value)
{
    m_cdnUuid = std::vector<uint8_t>(value.begin(), value.end());
}

Span<const uint8_t> FileAsset::cdnUuid() const { return m_cdnUuid; }

std::string FileAsset::cdnUuidStr() const
{
    constexpr uint8_t uuidSize = 16;
    if (m_cdnUuid.size() != uuidSize)
    {
        return "";
    }
    const std::array<const int, uuidSize>
        indices{3, 2, 1, 0, 5, 4, 7, 6, 9, 8, 15, 14, 13, 12, 11, 10};

    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (int idx : indices)
    {
        ss << std::setw(2) // always 2 chars
           << static_cast<unsigned int>(m_cdnUuid[idx]);
        if (idx == 0 || idx == 4 || idx == 6 || idx == 8)
            ss << '-';
    }

    return ss.str();
}