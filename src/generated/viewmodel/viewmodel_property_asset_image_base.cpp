#include "rive/generated/viewmodel/viewmodel_property_asset_image_base.hpp"
#include "rive/viewmodel/viewmodel_property_asset_image.hpp"

using namespace rive;

Core* ViewModelPropertyAssetImageBase::clone() const
{
    auto cloned = new ViewModelPropertyAssetImage();
    cloned->copy(*this);
    return cloned;
}
