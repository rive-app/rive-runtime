#include "rive/generated/data_bind/bindable_property_asset_base.hpp"
#include "rive/data_bind/bindable_property_asset.hpp"

using namespace rive;

Core* BindablePropertyAssetBase::clone() const
{
    auto cloned = new BindablePropertyAsset();
    cloned->copy(*this);
    return cloned;
}
