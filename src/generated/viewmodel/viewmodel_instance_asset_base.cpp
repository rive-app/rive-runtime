#include "rive/generated/viewmodel/viewmodel_instance_asset_base.hpp"
#include "rive/viewmodel/viewmodel_instance_asset.hpp"

using namespace rive;

Core* ViewModelInstanceAssetBase::clone() const
{
    auto cloned = new ViewModelInstanceAsset();
    cloned->copy(*this);
    return cloned;
}
