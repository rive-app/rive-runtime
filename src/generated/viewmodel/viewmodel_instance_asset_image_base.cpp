#include "rive/generated/viewmodel/viewmodel_instance_asset_image_base.hpp"
#include "rive/viewmodel/viewmodel_instance_asset_image.hpp"

using namespace rive;

Core* ViewModelInstanceAssetImageBase::clone() const
{
    auto cloned = new ViewModelInstanceAssetImage();
    cloned->copy(*this);
    return cloned;
}
