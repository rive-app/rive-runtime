#include "rive/importers/file_asset_importer.hpp"
#include "rive/assets/file_asset_contents.hpp"
#include "rive/assets/file_asset.hpp"
#include "rive/file_asset_loader.hpp"
#include "rive/span.hpp"
#include <cstdint>

using namespace rive;

FileAssetImporter::FileAssetImporter(FileAsset* fileAsset,
                                     FileAssetLoader* assetLoader,
                                     Factory* factory) :
    m_FileAsset(fileAsset), m_FileAssetLoader(assetLoader), m_Factory(factory)
{}

// if file asset contents are found when importing a rive file, store those for when we resolve 
// the importer later
void FileAssetImporter::onFileAssetContents(std::unique_ptr<FileAssetContents> contents)
{
    // we should only ever be called once
    assert(!m_Content);
    m_Content = std::move(contents);
}

StatusCode FileAssetImporter::resolve()
{
    // If we have a file asset loader that commits to loading the file asset, let it handle it
    if (m_FileAssetLoader != nullptr && m_FileAssetLoader->willLoadContents(*m_FileAsset))
    {
        m_FileAssetLoader->loadContents(*m_FileAsset);
    }
    // If we do not, but we have found in band contents, load those
    else if (m_Content != nullptr)
    {
        auto data = m_Content->bytes();
        m_FileAsset->decode(data, m_Factory);
    }

    // Note that it's ok for an asset to not resolve (or to resolve async).
    return StatusCode::Ok;
}
