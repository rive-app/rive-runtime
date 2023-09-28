#ifndef _RIVE_FILE_ASSET_RESOLVER_HPP_
#define _RIVE_FILE_ASSET_RESOLVER_HPP_

#include <cstdint>
#include <vector>

namespace rive
{
class FileAsset;
class FileAssetLoader
{
public:
    virtual ~FileAssetLoader() {}

    /// The return value sets the intention for handling loading the contents
    /// of the given asset. When no asset loader commits to handling the contents 
    /// we will load assets in band if provided. 
    /// 
    /// @param asset describes the asset that Rive is looking for the
    /// contents of.
    virtual bool willLoadContents(FileAsset& asset) {
        return true;
    }

    /// Load the contents of the given asset
    ///
    /// @param asset describes the asset that Rive is looking for the
    /// contents of.
    virtual void loadContents(FileAsset& asset) = 0;
};
} // namespace rive
#endif
