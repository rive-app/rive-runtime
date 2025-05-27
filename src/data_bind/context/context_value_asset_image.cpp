#include "rive/data_bind/context/context_value_asset_image.hpp"
#include "rive/data_bind/data_values/data_value_asset_image.hpp"
#include "rive/generated/core_registry.hpp"
#include "rive/file.hpp"

using namespace rive;

DataBindContextValueAssetImage::DataBindContextValueAssetImage(
    DataBind* dataBind) :
    DataBindContextValue(dataBind)
{}

ImageAsset* DataBindContextValueAssetImage::fileAsset()
{
    auto file = m_dataBind->file();
    auto source = m_dataBind->source();
    if (file != nullptr && source != nullptr &&
        source->is<ViewModelInstanceAssetImage>())
    {

        auto asset = file->asset(
            source->as<ViewModelInstanceAssetImage>()->propertyValue());
        if (asset != nullptr)
        {
            return asset->as<ImageAsset>();
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
    else
    {
        auto source = m_dataBind->source();
        CoreRegistry::setUint(
            target,
            propertyKey,
            source->as<ViewModelInstanceAssetImage>()->propertyValue());
    }
}

bool DataBindContextValueAssetImage::syncTargetValue(Core* target,
                                                     uint32_t propertyKey)
{
    auto value = CoreRegistry::getUint(target, propertyKey);

    if (m_previousValue != value)
    {
        m_previousValue = value;
        m_targetDataValue.value(value);
        return true;
    }
    return false;
}