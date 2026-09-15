#include "rive/generated/viewmodel/viewmodel_instance_asset_font_base.hpp"
#include "rive/viewmodel/viewmodel_instance_asset_font.hpp"

using namespace rive;

Core* ViewModelInstanceAssetFontBase::clone() const
{
    auto cloned = new ViewModelInstanceAssetFont();
    cloned->copy(*this);
    return cloned;
}
