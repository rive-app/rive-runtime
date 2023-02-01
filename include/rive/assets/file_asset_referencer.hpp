#ifndef _RIVE_FILE_ASSET_REFERENCER_HPP_
#define _RIVE_FILE_ASSET_REFERENCER_HPP_

#include <vector>
#include "rive/importers/import_stack.hpp"

namespace rive
{
class FileAsset;
class FileAssetReferencer
{
public:
    virtual ~FileAssetReferencer() {}
    virtual void assets(const std::vector<FileAsset*>& assets) = 0;
    StatusCode registerReferencer(ImportStack& importStack);
};
} // namespace rive

#endif