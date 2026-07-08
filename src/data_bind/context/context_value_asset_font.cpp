#include "rive/data_bind/context/context_value_asset_font.hpp"
#include "rive/data_bind/data_values/data_value_asset_font.hpp"
#include "rive/data_bind/bindable_property_asset.hpp"
#include "rive/generated/core_registry.hpp"
#include "rive/text/text_style.hpp"
#include "rive/file.hpp"

using namespace rive;

DataBindContextValueAssetFont::DataBindContextValueAssetFont(
    DataBind* dataBind) :
    DataBindContextValue(dataBind)
{}

rcp<FontAsset> DataBindContextValueAssetFont::fileAsset(DataBind* dataBind)
{
    auto file = dataBind->file();
    auto source = dataBind->source();
    if (file != nullptr && source != nullptr &&
        source->is<ViewModelInstanceAssetFont>())
    {

        auto asset = file->asset(
            source->as<ViewModelInstanceAssetFont>()->propertyValue());
        if (asset != nullptr && asset->is<FontAsset>())
        {
            return static_rcp_cast<FontAsset>(asset);
        }
    }
    return nullptr;
}

void DataBindContextValueAssetFont::apply(Core* target,
                                          uint32_t propertyKey,
                                          bool isMainDirection,
                                          DataBind* dataBind)
{
    if (target->is<TextStyle>())
    {
        auto asset = fileAsset(dataBind);
        if (asset != nullptr)
        {
            target->as<TextStyle>()->setAsset(asset);
        }
        else
        {
            auto source = dataBind->source();
            target->as<TextStyle>()->setAsset(
                source->as<ViewModelInstanceAssetFont>()->asset());
        }
    }
    else if (target->is<BindablePropertyAsset>())
    {
        auto source = dataBind->source();
        target->as<BindablePropertyAsset>()->fontValue(
            source->as<ViewModelInstanceAssetFont>()->asset()->font().get());
        CoreRegistry::setUint(
            target,
            propertyKey,
            source->as<ViewModelInstanceAssetFont>()->propertyValue());
    }
    else
    {
        auto source = dataBind->source();
        CoreRegistry::setUint(
            target,
            propertyKey,
            source->as<ViewModelInstanceAssetFont>()->propertyValue());
    }
}
