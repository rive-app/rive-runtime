#ifndef _RIVE_BINDABLE_PROPERTY_ASSET_HPP_
#define _RIVE_BINDABLE_PROPERTY_ASSET_HPP_
#include "rive/generated/data_bind/bindable_property_asset_base.hpp"
#include "rive/assets/image_asset.hpp"
#include <stdio.h>
namespace rive
{
class BindablePropertyAsset : public BindablePropertyAssetBase
{
public:
    BindablePropertyAsset() : m_fileAsset(rcp<ImageAsset>(new ImageAsset())) {}
    constexpr static uint32_t defaultValue = -1;
    rcp<ImageAsset> fileAsset() { return m_fileAsset; }
    void imageValue(RenderImage* image)
    {
        m_fileAsset->renderImage(ref_rcp(image));
    }
    RenderImage* imageValue() { return m_fileAsset->renderImage(); }

private:
    rcp<ImageAsset> m_fileAsset = nullptr;
};
} // namespace rive

#endif