#ifndef _RIVE_FILE_ASSET_RESOLVER_HPP_
#define _RIVE_FILE_ASSET_RESOLVER_HPP_

#include <cstdint>
#include <vector>

namespace rive
{
class FileAsset;
class FileAssetResolver
{
public:
    virtual ~FileAssetResolver() {}

    /// Expected to be overridden to find asset contents when not provided
    /// in band.
    /// @param asset describes the asset that Rive is looking for the
    /// contents of.
    virtual void loadContents(FileAsset& asset) = 0;
};
} // namespace rive
#endif
