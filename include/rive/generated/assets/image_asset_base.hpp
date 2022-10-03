#ifndef _RIVE_IMAGE_ASSET_BASE_HPP_
#define _RIVE_IMAGE_ASSET_BASE_HPP_
#include "rive/assets/drawable_asset.hpp"
namespace rive
{
class ImageAssetBase : public DrawableAsset
{
protected:
    typedef DrawableAsset Super;

public:
    static const uint16_t typeKey = 105;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ImageAssetBase::typeKey:
            case DrawableAssetBase::typeKey:
            case FileAssetBase::typeKey:
            case AssetBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;

protected:
};
} // namespace rive

#endif