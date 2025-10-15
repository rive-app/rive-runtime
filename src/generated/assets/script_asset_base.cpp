#include "rive/generated/assets/script_asset_base.hpp"
#include "rive/assets/script_asset.hpp"

using namespace rive;

Core* ScriptAssetBase::clone() const
{
    auto cloned = new ScriptAsset();
    cloned->copy(*this);
    return cloned;
}
