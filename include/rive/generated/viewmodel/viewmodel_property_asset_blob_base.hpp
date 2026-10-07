#ifndef _RIVE_VIEW_MODEL_PROPERTY_ASSET_BLOB_BASE_HPP_
#define _RIVE_VIEW_MODEL_PROPERTY_ASSET_BLOB_BASE_HPP_
#include "rive/viewmodel/viewmodel_property_asset.hpp"
namespace rive
{
class ViewModelPropertyAssetBlobBase : public ViewModelPropertyAsset
{
protected:
    typedef ViewModelPropertyAsset Super;

public:
    static const uint16_t typeKey = 1043;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif