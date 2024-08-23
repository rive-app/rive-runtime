#ifndef _RIVE_FONT_ASSET_BASE_HPP_
#define _RIVE_FONT_ASSET_BASE_HPP_
#include "rive/assets/file_asset.hpp"
namespace rive
{
class FontAssetBase : public FileAsset
{
protected:
    typedef FileAsset Super;

public:
    static const uint16_t typeKey = 141;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case FontAssetBase::typeKey:
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