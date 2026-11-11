#ifndef _RIVE_MANIFEST_ASSET_IMPORTER_HPP_
#define _RIVE_MANIFEST_ASSET_IMPORTER_HPP_

#include "rive/importers/file_asset_importer.hpp"

namespace rive
{
/// Decodes the manifest as soon as its contents are read, rather than when
/// the next asset or the end of the file resolves the importer, so objects
/// read after it can resolve its names while they deserialize. The exporter
/// writes the manifest before the Backboard. The manifest is the file's own
/// index, not content, so it isn't offered to the host's FileAssetLoader.
class ManifestAssetImporter : public FileAssetImporter
{
public:
  ManifestAssetImporter(FileAsset* fileAsset, Factory* factory);
  void onFileAssetContents(
      std::unique_ptr<FileAssetContents> contents) override;
  StatusCode resolve() override;
};
} // namespace rive
#endif
