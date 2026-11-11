#include "rive/importers/manifest_asset_importer.hpp"
#include "rive/assets/file_asset_contents.hpp"
#include "rive/assets/file_asset.hpp"
#include "rive/file_asset_loader.hpp"

using namespace rive;

ManifestAssetImporter::ManifestAssetImporter(
    FileAsset* fileAsset,
    Factory* factory)
    : FileAssetImporter(fileAsset, nullptr, factory)
{}

void ManifestAssetImporter::onFileAssetContents(
    std::unique_ptr<FileAssetContents> contents)
{
  FileAssetImporter::onFileAssetContents(std::move(contents));
  if (m_content->bytes().size() > 0)
  {
    m_fileAsset->decode(m_content->bytes(), m_factory);
  }
}

// Decoded with its contents already.
StatusCode ManifestAssetImporter::resolve() { return StatusCode::Ok; }
