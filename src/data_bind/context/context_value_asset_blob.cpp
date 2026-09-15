#include "rive/data_bind/context/context_value_asset_blob.hpp"
#include "rive/data_bind/data_values/data_value_asset_blob.hpp"
#include "rive/data_bind/bindable_property_asset.hpp"
#include "rive/generated/core_registry.hpp"
#include "rive/viewmodel/viewmodel_instance_asset_blob.hpp"
#include "rive/file.hpp"

using namespace rive;

DataBindContextValueAssetBlob::DataBindContextValueAssetBlob(
    DataBind* dataBind) :
    DataBindContextValue(dataBind)
{}

rcp<BlobAsset> DataBindContextValueAssetBlob::fileAsset(DataBind* dataBind)
{
    auto file = dataBind->file();
    auto source = dataBind->source();
    if (file != nullptr && source != nullptr &&
        source->is<ViewModelInstanceAssetBlob>())
    {

        auto asset = file->asset(
            source->as<ViewModelInstanceAssetBlob>()->propertyValue());
        if (asset != nullptr && asset->is<BlobAsset>())
        {
            return static_rcp_cast<BlobAsset>(asset);
        }
    }
    return nullptr;
}

void DataBindContextValueAssetBlob::apply(Core* target,
                                          uint32_t propertyKey,
                                          bool isMainDirection,
                                          DataBind* dataBind)
{
    // A blob has no decoded render resource that a target drawable consumes;
    // binding simply carries the asset id (and the live asset for bindable
    // property indirection).
    if (target->is<BindablePropertyAsset>())
    {
        auto source = dataBind->source();
        BlobAsset* liveBlob =
            source->as<ViewModelInstanceAssetBlob>()->asset().get();
        target->as<BindablePropertyAsset>()->blobValue(liveBlob);
        CoreRegistry::setUint(
            target,
            propertyKey,
            source->as<ViewModelInstanceAssetBlob>()->propertyValue());
    }
    else if (target->is<ViewModelInstanceAssetBlob>())
    {
        // VM -> VM bind into an exposed blob property. When the source carries
        // a runtime-set blob its propertyValue is the -1 sentinel; forwarding
        // that id via setUint would dedupe against the target's existing
        // sentinel and skip propertyValueChanged/onValueChanged, so the live
        // blob never propagates and script listeners never fire. Push the live
        // asset through value() instead, mirroring the image target branch.
        auto source = dataBind->source()->as<ViewModelInstanceAssetBlob>();
        auto sourceValue = source->propertyValue();
        if (sourceValue == static_cast<uint32_t>(-1))
        {
            target->as<ViewModelInstanceAssetBlob>()->value(
                source->asset().get());
        }
        else
        {
            CoreRegistry::setUint(target, propertyKey, sourceValue);
        }
    }
    else
    {
        auto source = dataBind->source();
        CoreRegistry::setUint(
            target,
            propertyKey,
            source->as<ViewModelInstanceAssetBlob>()->propertyValue());
    }
}
