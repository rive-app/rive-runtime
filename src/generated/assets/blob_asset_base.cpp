#include "rive/generated/assets/blob_asset_base.hpp"
#include "rive/assets/blob_asset.hpp"

using namespace rive;

Core* BlobAssetBase::clone() const
{
    auto cloned = new BlobAsset();
    cloned->copy(*this);
    return cloned;
}
