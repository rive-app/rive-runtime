#ifndef _RIVE_FILE_ASSET_REFERENCER_HPP_
#define _RIVE_FILE_ASSET_REFERENCER_HPP_

#include <vector>
#include "rive/importers/import_stack.hpp"
#include "rive/assets/file_asset.hpp"

namespace rive
{
class FileAssetReferencer
{
protected:
    rcp<FileAsset> m_fileAsset;

public:
    virtual ~FileAssetReferencer() = 0;
    virtual void setAsset(rcp<FileAsset> asset);
    const rcp<FileAsset> asset() { return m_fileAsset; }
    virtual uint32_t assetId() = 0;
    StatusCode registerReferencer(ImportStack& importStack);
    virtual void assetUpdated() {}
};
} // namespace rive

#endif