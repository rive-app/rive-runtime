#ifndef _RIVE_VIEW_MODEL_INSTANCE_ASSET_BLOB_BASE_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_ASSET_BLOB_BASE_HPP_
#include "rive/viewmodel/viewmodel_instance_asset.hpp"
namespace rive
{
class ViewModelInstanceAssetBlobBase : public ViewModelInstanceAsset
{
protected:
    typedef ViewModelInstanceAsset Super;

public:
    static const uint16_t typeKey = 1044;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif