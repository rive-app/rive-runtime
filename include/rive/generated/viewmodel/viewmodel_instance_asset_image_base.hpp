#ifndef _RIVE_VIEW_MODEL_INSTANCE_ASSET_IMAGE_BASE_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_ASSET_IMAGE_BASE_HPP_
#include "rive/viewmodel/viewmodel_instance_asset.hpp"
namespace rive
{
class ViewModelInstanceAssetImageBase : public ViewModelInstanceAsset
{
protected:
    typedef ViewModelInstanceAsset Super;

public:
    static const uint16_t typeKey = 587;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ViewModelInstanceAssetImageBase::typeKey:
            case ViewModelInstanceAssetBase::typeKey:
            case ViewModelInstanceValueBase::typeKey:
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