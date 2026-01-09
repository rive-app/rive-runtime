#ifndef _RIVE_FILE_ASSET_IMPORTER_HPP_
#define _RIVE_FILE_ASSET_IMPORTER_HPP_

#include "rive/refcnt.hpp"
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
public:
    FileAssetImporter(FileAsset*, rcp<FileAssetLoader>, Factory*);
    virtual void onFileAssetContents(
        std::unique_ptr<FileAssetContents> contents);
    StatusCode resolve() override;

protected:
    FileAsset* m_fileAsset;
    rcp<FileAssetLoader> m_fileAssetLoader;
    Factory* m_factory;
    // we will delete this when we go out of scope
    std::unique_ptr<FileAssetContents> m_content;
};
} // namespace rive
#endif
