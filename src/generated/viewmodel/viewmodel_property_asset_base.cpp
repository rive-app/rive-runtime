#include "rive/generated/viewmodel/viewmodel_property_asset_base.hpp"
#include "rive/viewmodel/viewmodel_property_asset.hpp"

using namespace rive;

Core* ViewModelPropertyAssetBase::clone() const
{
    auto cloned = new ViewModelPropertyAsset();
    cloned->copy(*this);
    return cloned;
}
