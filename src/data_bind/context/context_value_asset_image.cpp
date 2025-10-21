#include "rive/data_bind/context/context_value_asset_image.hpp"
#include "rive/data_bind/data_values/data_value_asset_image.hpp"
#include "rive/generated/core_registry.hpp"
#include "rive/file.hpp"

using namespace rive;

DataBindContextValueAssetImage::DataBindContextValueAssetImage(
    DataBind* dataBind) :
    DataBindContextValue(dataBind)
{}

rcp<ImageAsset> DataBindContextValueAssetImage::fileAsset()
{
    auto file = m_dataBind->file();
    auto source = m_dataBind->source();
    if (file != nullptr && source != nullptr &&
        source->is<ViewModelInstanceAssetImage>())
    {

        auto asset = file->asset(
            source->as<ViewModelInstanceAssetImage>()->propertyValue());
        if (asset != nullptr && asset->is<ImageAsset>())
        {
            return static_rcp_cast<ImageAsset>(asset);
        }
    }
    return nullptr;
}

void DataBindContextValueAssetImage::apply(Core* target,
                                           uint32_t propertyKey,
                                           bool isMainDirection)
{
    if (target->is<Image>())
    {
        auto asset = fileAsset();
        if (asset != nullptr)
        {
            target->as<Image>()->setAsset(asset);
        }
        else
        {
            auto source = m_dataBind->source();
            target->as<Image>()->setAsset(
                source->as<ViewModelInstanceAssetImage>()->asset());
        }
    }
    else if (target->is<BindablePropertyAsset>())
    {
        auto source = m_dataBind->source();
        target->as<BindablePropertyAsset>()->imageValue(
            source->as<ViewModelInstanceAssetImage>()->asset()->renderImage());
        CoreRegistry::setUint(
            target,
            propertyKey,
            source->as<ViewModelInstanceAssetImage>()->propertyValue());
    }
    else
    {
        auto source = m_dataBind->source();
        CoreRegistry::setUint(
            target,
            propertyKey,
            source->as<ViewModelInstanceAssetImage>()->propertyValue());
    }
}