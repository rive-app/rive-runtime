#include <sstream>
#include <iomanip>
#include <array>

#include "rive/viewmodel/viewmodel_instance_asset_image.hpp"
#include "rive/component_dirt.hpp"
#include "rive/refcnt.hpp"

using namespace rive;

void ViewModelInstanceAssetImage::propertyValueChanged()
{
    addDirt(ComponentDirt::Bindings);
#ifdef WITH_RIVE_TOOLS
    if (m_changedCallback != nullptr)
    {
        m_changedCallback(this, propertyValue());
    }
#endif
}

void ViewModelInstanceAssetImage::value(RenderImage* image)
{
    propertyValue(-1);
    if (m_imageAsset.renderImage() == image)
    {
        return;
    }
    if (image == nullptr)
    {
        m_imageAsset.renderImage(nullptr);
    }
    else
    {
        image->ref();
        m_imageAsset.renderImage(rcp<RenderImage>(image));
    }
    addDirt(ComponentDirt::Bindings);
}

Core* ViewModelInstanceAssetImage::clone() const
{
    auto cloned = new ViewModelInstanceAssetImage();
    cloned->copy(*this);
    for (const auto& asset : assets())
    {
        cloned->addAsset(asset);
    }
    return cloned;
}