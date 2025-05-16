#ifndef _RIVE_VIEW_MODEL_PROPERTY_ASSET_IMAGE_BASE_HPP_
#define _RIVE_VIEW_MODEL_PROPERTY_ASSET_IMAGE_BASE_HPP_
#include "rive/viewmodel/viewmodel_property_asset.hpp"
namespace rive
{
class ViewModelPropertyAssetImageBase : public ViewModelPropertyAsset
{
protected:
    typedef ViewModelPropertyAsset Super;

public:
    static const uint16_t typeKey = 585;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ViewModelPropertyAssetImageBase::typeKey:
            case ViewModelPropertyAssetBase::typeKey:
            case ViewModelPropertyBase::typeKey:
            case ViewModelComponentBase::typeKey:
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