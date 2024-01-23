#include "rive/generated/assets/audio_asset_base.hpp"
#include "rive/assets/audio_asset.hpp"

using namespace rive;

Core* AudioAssetBase::clone() const
{
    auto cloned = new AudioAsset();
    cloned->copy(*this);
    return cloned;
}
