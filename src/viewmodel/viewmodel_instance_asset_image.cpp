#include <sstream>
#include <iomanip>
#include <array>

#include "rive/viewmodel/viewmodel_instance_asset_image.hpp"
#include "rive/component_dirt.hpp"
#include "rive/refcnt.hpp"
#include "rive/data_bind/data_values/data_value.hpp"
#include "rive/data_bind/data_values/data_value_asset_image.hpp"

using namespace rive;

ViewModelInstanceAssetImage::ViewModelInstanceAssetImage() :
    m_imageAsset(rcp<ImageAsset>(new ImageAsset()))
{}

void ViewModelInstanceAssetImage::propertyValueChanged()
{
    addDirt(ComponentDirt::Bindings);
#ifdef WITH_RIVE_TOOLS
    if (m_changedCallback != nullptr)
    {
        m_changedCallback(this, propertyValue());
    }
#endif
    onValueChanged();
}

void ViewModelInstanceAssetImage::value(RenderImage* image)
{
    propertyValue(-1);
    if (m_imageAsset->renderImage() == image)
    {
        return;
    }
    if (image == nullptr)
    {
        m_imageAsset->renderImage(nullptr);
    }
    else
    {
        image->ref();
        m_imageAsset->renderImage(rcp<RenderImage>(image));
    }
    addDirt(ComponentDirt::Bindings);
    onValueChanged();
}

void ViewModelInstanceAssetImage::applyValue(DataValueInteger* dataValue)
{
    if (dataValue && dataValue->is<DataValueAssetImage>())
    {
        auto renderImage = dataValue->as<DataValueAssetImage>()->imageValue();
        value(renderImage);
        if (renderImage)
        {
            return;
        }
    }
    propertyValue(dataValue->value());
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