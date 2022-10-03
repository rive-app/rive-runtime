#ifndef _RIVE_FILE_ASSET_IMPORTER_HPP_
#define _RIVE_FILE_ASSET_IMPORTER_HPP_

#include "rive/importers/import_stack.hpp"
#include <unordered_map>
#include <vector>

namespace rive
{
class FileAsset;
class FileAssetContents;
class FileAssetResolver;
class Factory;

class FileAssetImporter : public ImportStackObject
{
private:
    bool m_LoadedContents = false;
    FileAsset* m_FileAsset;
    FileAssetResolver* m_FileAssetResolver;
    Factory* m_Factory;
    // we will delete this when we go out of scope
    std::unique_ptr<FileAssetContents> m_Content;

public:
    FileAssetImporter(FileAsset*, FileAssetResolver*, Factory*);
    void loadContents(std::unique_ptr<FileAssetContents> contents);
    StatusCode resolve() override;
};
} // namespace rive
#endif
