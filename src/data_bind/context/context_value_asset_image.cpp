#include "rive/data_bind/context/context_value_asset_image.hpp"
#include "rive/data_bind/data_values/data_value_asset_image.hpp"
#include "rive/generated/core_registry.hpp"
#include "rive/file.hpp"

using namespace rive;

DataBindContextValueAssetImage::DataBindContextValueAssetImage(
    DataBind* dataBind) :
    DataBindContextValue(dataBind)
{}

rcp<ImageAsset> DataBindContextValueAssetImage::fileAsset(DataBind* dataBind)
{
    auto file = dataBind->file();
    auto source = dataBind->source();
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
                                           bool isMainDirection,
                                           DataBind* dataBind)
{
    if (target->is<Image>())
    {
        auto asset = fileAsset(dataBind);
        if (asset != nullptr)
        {
            target->as<Image>()->setAsset(asset);
        }
        else
        {
            auto source = dataBind->source();
            target->as<Image>()->setAsset(
                source->as<ViewModelInstanceAssetImage>()->asset());
        }
    }
    else if (target->is<BindablePropertyAsset>())
    {
        auto source = dataBind->source();
        target->as<BindablePropertyAsset>()->imageValue(
            source->as<ViewModelInstanceAssetImage>()->asset()->renderImage());
        CoreRegistry::setUint(
            target,
            propertyKey,
            source->as<ViewModelInstanceAssetImage>()->propertyValue());
    }
    else
    {
        auto source = dataBind->source();
        CoreRegistry::setUint(
            target,
            propertyKey,
            source->as<ViewModelInstanceAssetImage>()->propertyValue());
    }
}