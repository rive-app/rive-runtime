#include "rive/generated/assets/image_asset_base.hpp"
#include "rive/assets/image_asset.hpp"

using namespace rive;

Core* ImageAssetBase::clone() const
{
    auto cloned = new ImageAsset();
    cloned->copy(*this);
    return cloned;
}
