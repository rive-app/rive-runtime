#ifndef _RIVE_SHADER_ASSET_BASE_HPP_
#define _RIVE_SHADER_ASSET_BASE_HPP_
#include "rive/assets/text_asset.hpp"
namespace rive
{
class ShaderAssetBase : public TextAsset
{
protected:
    typedef TextAsset Super;

public:
    static const uint16_t typeKey = 970;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ShaderAssetBase::typeKey:
            case TextAssetBase::typeKey:
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