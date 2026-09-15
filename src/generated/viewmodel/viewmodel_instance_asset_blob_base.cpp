#include "rive/generated/viewmodel/viewmodel_instance_asset_blob_base.hpp"
#include "rive/viewmodel/viewmodel_instance_asset_blob.hpp"

using namespace rive;

Core* ViewModelInstanceAssetBlobBase::clone() const
{
    auto cloned = new ViewModelInstanceAssetBlob();
    cloned->copy(*this);
    return cloned;
}
