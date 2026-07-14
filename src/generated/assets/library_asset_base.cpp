#include "rive/generated/assets/library_asset_base.hpp"
#include "rive/assets/library_asset.hpp"

using namespace rive;

Core* LibraryAssetBase::clone() const
{
    auto cloned = new LibraryAsset();
    cloned->copy(*this);
    return cloned;
}
