#ifndef _RIVE_BINDABLE_PROPERTY_ASSET_HPP_
#define _RIVE_BINDABLE_PROPERTY_ASSET_HPP_
#include "rive/generated/data_bind/bindable_property_asset_base.hpp"
#include "rive/assets/image_asset.hpp"
#include "rive/assets/font_asset.hpp"
#include "rive/text_engine.hpp"
#include <stdio.h>
namespace rive
{
// A single bindable asset property that can carry either a live image or a live
// font. Only one is populated for a given bind (determined by the bound asset
// kind); the other stays empty. The asset id is carried by the
// BindablePropertyId base regardless of kind.
class BindablePropertyAsset : public BindablePropertyAssetBase
{
public:
    BindablePropertyAsset() :
        m_fileAsset(rcp<ImageAsset>(new ImageAsset())),
        m_fontAsset(rcp<FontAsset>(new FontAsset()))
    {}
    constexpr static uint32_t defaultValue = -1;
    rcp<ImageAsset> fileAsset() { return m_fileAsset; }
    void imageValue(RenderImage* image)
    {
        m_fileAsset->renderImage(ref_rcp(image));
    }
    RenderImage* imageValue() { return m_fileAsset->renderImage(); }
    rcp<FontAsset> fontFileAsset() { return m_fontAsset; }
    void fontValue(Font* font) { m_fontAsset->font(ref_rcp(font)); }
    Font* fontValue() { return m_fontAsset->font().get(); }

private:
    rcp<ImageAsset> m_fileAsset = nullptr;
    rcp<FontAsset> m_fontAsset = nullptr;
};
} // namespace rive

#endif