#ifndef _RIVE_MANIFEST_ASSET_BASE_HPP_
#define _RIVE_MANIFEST_ASSET_BASE_HPP_
#include "rive/assets/file_asset.hpp"
namespace rive
{
class ManifestAssetBase : public FileAsset
{
protected:
    typedef FileAsset Super;

public:
    static const uint16_t typeKey = 642;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif