#ifndef _RIVE_VIEW_MODEL_INSTANCE_ASSET_IMAGE_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_ASSET_IMAGE_HPP_
#include "rive/generated/viewmodel/viewmodel_instance_asset_image_base.hpp"
#include "rive/renderer.hpp"
#include "rive/assets/image_asset.hpp"
#include <stdio.h>
namespace rive
{
class ViewModelInstanceAssetImage : public ViewModelInstanceAssetImageBase
{
protected:
    void propertyValueChanged() override;

public:
    void value(RenderImage* image);
    ImageAsset* asset() { return &m_imageAsset; }
    Core* clone() const override;

private:
    ImageAsset m_imageAsset;
};
} // namespace rive

#endif