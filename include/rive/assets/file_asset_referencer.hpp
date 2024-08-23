#ifndef _RIVE_FILE_ASSET_REFERENCER_HPP_
#define _RIVE_FILE_ASSET_REFERENCER_HPP_

#include <vector>
#include "rive/importers/import_stack.hpp"

namespace rive
{
class FileAsset;
class FileAssetReferencer
{
protected:
    FileAsset* m_fileAsset = nullptr;

public:
    virtual ~FileAssetReferencer() = 0;
    virtual void setAsset(FileAsset* asset);
    virtual uint32_t assetId() = 0;
    StatusCode registerReferencer(ImportStack& importStack);
};
} // namespace rive

#endif