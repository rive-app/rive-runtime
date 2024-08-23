#ifndef _RIVE_FILE_ASSET_IMPORTER_HPP_
#define _RIVE_FILE_ASSET_IMPORTER_HPP_

#include "rive/importers/import_stack.hpp"
#include <unordered_map>
#include <vector>

namespace rive
{
class FileAsset;
class FileAssetContents;
class FileAssetLoader;
class Factory;

class FileAssetImporter : public ImportStackObject
{
private:
    FileAsset* m_FileAsset;
    FileAssetLoader* m_FileAssetLoader;
    Factory* m_Factory;
    // we will delete this when we go out of scope
    std::unique_ptr<FileAssetContents> m_Content;

public:
    FileAssetImporter(FileAsset*, FileAssetLoader*, Factory*);
    void onFileAssetContents(std::unique_ptr<FileAssetContents> contents);
    StatusCode resolve() override;
};
} // namespace rive
#endif
