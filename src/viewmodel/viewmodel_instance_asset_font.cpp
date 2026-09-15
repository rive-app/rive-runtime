#include <sstream>
#include <iomanip>
#include <array>

#include "rive/viewmodel/viewmodel_instance_asset_font.hpp"
#include "rive/component_dirt.hpp"
#include "rive/refcnt.hpp"
#include "rive/data_bind/data_values/data_value.hpp"
#include "rive/data_bind/data_values/data_value_asset_font.hpp"

using namespace rive;

ViewModelInstanceAssetFont::ViewModelInstanceAssetFont() :
    m_fontAsset(rcp<FontAsset>(new FontAsset()))
{}

void ViewModelInstanceAssetFont::propertyValueChanged()
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

void ViewModelInstanceAssetFont::value(Font* font)
{
    if (m_fontAsset->font().get() == font)
    {
        propertyValue(-1);
        return;
    }
#ifdef WITH_RIVE_TOOLS
    const bool alreadySentinel = (propertyValue() == static_cast<uint32_t>(-1));
#endif
    if (font == nullptr)
    {
        m_fontAsset->font(nullptr);
    }
    else
    {
        font->ref();
        m_fontAsset->font(rcp<Font>(font));
    }
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

void ViewModelInstanceAssetFont::applyValue(DataValueInteger* dataValue)
{
    if (dataValue && dataValue->is<DataValueAssetFont>())
    {
        auto font = dataValue->as<DataValueAssetFont>()->fontValue();
        value(font);
        if (font)
        {
            return;
        }
    }
    propertyValue(dataValue->value());
}

Core* ViewModelInstanceAssetFont::clone() const
{
    auto cloned = new ViewModelInstanceAssetFont();
    cloned->copy(*this);
    for (const auto& asset : assets())
    {
        cloned->addAsset(asset);
    }
    return cloned;
}
