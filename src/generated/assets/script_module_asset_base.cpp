#include "rive/generated/assets/script_module_asset_base.hpp"
#include "rive/assets/script_module_asset.hpp"

using namespace rive;

Core* ScriptModuleAssetBase::clone() const
{
    auto cloned = new ScriptModuleAsset();
    cloned->copy(*this);
    return cloned;
}
