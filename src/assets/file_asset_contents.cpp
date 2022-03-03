#include "rive/assets/file_asset_contents.hpp"
#include "rive/assets/file_asset.hpp"
#include "rive/importers/file_asset_importer.hpp"

using namespace rive;

StatusCode FileAssetContents::import(ImportStack& importStack) {
    auto fileAssetImporter =
        importStack.latest<FileAssetImporter>(FileAsset::typeKey);
    if (fileAssetImporter == nullptr) {
        return StatusCode::MissingObject;
    }
    fileAssetImporter->loadContents(*this);

    return Super::import(importStack);
}

void FileAssetContents::decodeBytes(const Span<uint8_t>& value) {
    m_Bytes = std::vector(value.begin(), value.end());
}

void FileAssetContents::copyBytes(const FileAssetContentsBase& object) {
    // Should never be called.
    assert(false);
}

const std::vector<uint8_t>& FileAssetContents::bytes() const { return m_Bytes; }