#ifndef _RIVE_FILE_ASSET_RESOLVER_HPP_
#define _RIVE_FILE_ASSET_RESOLVER_HPP_

#include <cstdint>
#include <vector>
#include "rive/span.hpp"

namespace rive
{
class Factory;
class FileAsset;
class FileAssetLoader
{
public:
    virtual ~FileAssetLoader() {}

    /// Load the contents of the given asset
    ///
    /// @param asset describes the asset that Rive is looking for the
    /// contents of.
    /// @param inBandBytes is a pointer to the bytes in question
    /// @returns bool indicating if we are loading or have loaded the contents

    virtual bool loadContents(FileAsset& asset,
                              Span<const uint8_t> inBandBytes,
                              Factory* factory) = 0;
};
} // namespace rive
#endif
