#include "rive/generated/assets/font_asset_base.hpp"
#include "rive/assets/font_asset.hpp"

using namespace rive;

Core* FontAssetBase::clone() const
{
    auto cloned = new FontAsset();
    cloned->copy(*this);
    return cloned;
}
