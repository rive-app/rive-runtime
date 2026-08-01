#ifndef _RIVE_VIEW_MODEL_INSTANCE_ASSET_BLOB_RUNTIME_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_ASSET_BLOB_RUNTIME_HPP_

#include <string>
#include <stdint.h>
#include "rive/viewmodel/runtime/viewmodel_instance_value_runtime.hpp"
#include "rive/viewmodel/viewmodel_instance_asset_blob.hpp"

namespace rive
{

class ViewModelInstanceAssetBlobRuntime : public ViewModelInstanceValueRuntime
{

public:
    ViewModelInstanceAssetBlobRuntime(
        ViewModelInstanceAssetBlob* viewModelInstance) :
        ViewModelInstanceValueRuntime(viewModelInstance)
    {}
    void value(BlobAsset* blob);
    const DataType dataType() override { return DataType::assetBlob; }

#ifdef TESTING
    BlobAsset* testing_value();
#endif
};
} // namespace rive
#endif
