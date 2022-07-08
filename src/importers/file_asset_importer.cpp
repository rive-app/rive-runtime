#include "rive/importers/file_asset_importer.hpp"
#include "rive/assets/file_asset_contents.hpp"
#include "rive/assets/file_asset.hpp"
#include "rive/file_asset_resolver.hpp"
#include "rive/span.hpp"
#include <cstdint>

using namespace rive;

FileAssetImporter::FileAssetImporter(FileAsset* fileAsset,
                                     FileAssetResolver* assetResolver,
                                     Factory* factory) :
    m_FileAsset(fileAsset), m_FileAssetResolver(assetResolver), m_Factory(factory) {}

void FileAssetImporter::loadContents(const FileAssetContents& contents) {
    auto data = contents.bytes();
    if (m_FileAsset->decode(data, m_Factory)) {
        m_LoadedContents = true;
    }
}

StatusCode FileAssetImporter::resolve() {
    if (!m_LoadedContents && m_FileAssetResolver != nullptr) {
        // Contents weren't available in-band, or they couldn't be decoded. Try
        // to find them out of band.
        m_FileAssetResolver->loadContents(*m_FileAsset);
    }

    // Note that it's ok for an asset to not resolve (or to resolve async).
    return StatusCode::Ok;
}
