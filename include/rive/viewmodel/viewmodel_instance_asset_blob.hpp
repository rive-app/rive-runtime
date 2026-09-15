#ifndef _RIVE_VIEW_MODEL_INSTANCE_ASSET_BLOB_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_ASSET_BLOB_HPP_
#include "rive/generated/viewmodel/viewmodel_instance_asset_blob_base.hpp"
#include "rive/refcnt.hpp"
#include "rive/data_bind/data_values/data_value_integer.hpp"
#include "rive/assets/blob_asset.hpp"
#include <stdio.h>
namespace rive
{
class ViewModelInstanceAssetBlob : public ViewModelInstanceAssetBlobBase
{
protected:
    void propertyValueChanged() override;

public:
    ViewModelInstanceAssetBlob();
    // Directly set the blob asset (e.g. from scripts). Marks propertyValue as a
    // sentinel to indicate the bytes were supplied directly rather than by id.
    void value(BlobAsset* blob);
    rcp<BlobAsset> asset() { return m_blobAsset; }
    Core* clone() const override;
    void applyValue(DataValueInteger*);

private:
    rcp<BlobAsset> m_blobAsset = nullptr;
};
} // namespace rive

#endif
