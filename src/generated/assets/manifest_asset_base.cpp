#include "rive/generated/assets/manifest_asset_base.hpp"
#include "rive/assets/manifest_asset.hpp"

using namespace rive;

Core* ManifestAssetBase::clone() const
{
    auto cloned = new ManifestAsset();
    cloned->copy(*this);
    return cloned;
}
