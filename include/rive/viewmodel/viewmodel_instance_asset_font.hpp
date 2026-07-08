#ifndef _RIVE_VIEW_MODEL_INSTANCE_ASSET_FONT_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_ASSET_FONT_HPP_
#include "rive/generated/viewmodel/viewmodel_instance_asset_font_base.hpp"
#include "rive/text_engine.hpp"
#include "rive/refcnt.hpp"
#include "rive/data_bind/data_values/data_value_integer.hpp"
#include "rive/assets/font_asset.hpp"
#include <stdio.h>
namespace rive
{
class ViewModelInstanceAssetFont : public ViewModelInstanceAssetFontBase
{
protected:
    void propertyValueChanged() override;

public:
    ViewModelInstanceAssetFont();
    void value(Font* font);
    rcp<FontAsset> asset() { return m_fontAsset; }
    Core* clone() const override;
    void applyValue(DataValueInteger*);

private:
    rcp<FontAsset> m_fontAsset = nullptr;
};
} // namespace rive

#endif
