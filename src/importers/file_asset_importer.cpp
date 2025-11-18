#include "rive/importers/file_asset_importer.hpp"
#include "rive/assets/file_asset_contents.hpp"
#include "rive/assets/file_asset.hpp"
#include "rive/file_asset_loader.hpp"
#include "rive/span.hpp"
#include <cstdint>

using namespace rive;

FileAssetImporter::FileAssetImporter(FileAsset* fileAsset,
                                     rcp<FileAssetLoader> assetLoader,
                                     Factory* factory) :
    m_fileAsset(fileAsset),
    m_fileAssetLoader(std::move(assetLoader)),
    m_factory(factory)
{}

// if file asset contents are found when importing a rive file, store those for
// when we resolve the importer later
void FileAssetImporter::onFileAssetContents(
    std::unique_ptr<FileAssetContents> contents)
{
    // we should only ever be called once
    assert(!m_content);
    m_content = std::move(contents);
}

StatusCode FileAssetImporter::resolve()
{
    Span<const uint8_t> bytes;
    if (m_content != nullptr)
    {
        bytes = m_content->bytes();
    }

    // If we have a file asset loader, lets give it the opportunity to claim
    // responsibility for loading the asset
    if (m_fileAssetLoader != nullptr &&
        m_fileAssetLoader->loadContents(*m_fileAsset, bytes, m_factory))
    {
        return StatusCode::Ok;
    }
    // If we do not, but we have found in band contents, load those
    else if (bytes.size() > 0)
    {
        m_fileAsset->decode(m_content->bytes(), m_factory);
    }

    // Note that it's ok for an asset to not resolve (or to resolve async).
    return StatusCode::Ok;
}
