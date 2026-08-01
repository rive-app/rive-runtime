#include <sstream>
#include <iomanip>
#include <array>

#include "rive/viewmodel/viewmodel_instance_asset_blob.hpp"
#include "rive/component_dirt.hpp"
#include "rive/refcnt.hpp"
#include "rive/data_bind/data_values/data_value.hpp"
#include "rive/data_bind/data_values/data_value_asset_blob.hpp"

using namespace rive;

ViewModelInstanceAssetBlob::ViewModelInstanceAssetBlob() {}

void ViewModelInstanceAssetBlob::propertyValueChanged()
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

void ViewModelInstanceAssetBlob::value(BlobAsset* blob)
{
    if (m_blobAsset.get() == blob)
    {
        propertyValue(-1);
        return;
    }
#ifdef WITH_RIVE_TOOLS
    const bool alreadySentinel = (propertyValue() == static_cast<uint32_t>(-1));
#endif
    m_blobAsset = blob == nullptr ? nullptr : ref_rcp(blob);
#ifdef WITH_RIVE_TOOLS
    if (!alreadySentinel)
    {
        propertyValue(-1);
    }
    else if (m_changedCallback != nullptr)
    {
        m_changedCallback(this, propertyValue());
    }
#else
    propertyValue(-1);
#endif
    addDirt(ComponentDirt::Bindings);
    onValueChanged();
}

void ViewModelInstanceAssetBlob::applyValue(DataValueInteger* dataValue)
{
    if (dataValue && dataValue->is<DataValueAssetBlob>())
    {
        auto blob = dataValue->as<DataValueAssetBlob>()->blobValue();
        value(blob);
        if (blob)
        {
            return;
        }
    }
    propertyValue(dataValue->value());
}

Core* ViewModelInstanceAssetBlob::clone() const
{
    auto cloned = new ViewModelInstanceAssetBlob();
    cloned->copy(*this);
    for (const auto& asset : assets())
    {
        cloned->addAsset(asset);
    }
    return cloned;
}
