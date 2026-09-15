#ifndef _RIVE_DATA_VALUE_ASSET_FONT_HPP_
#define _RIVE_DATA_VALUE_ASSET_FONT_HPP_
#include "rive/data_bind/data_values/data_value_integer.hpp"
#include "rive/assets/font_asset.hpp"
#include "rive/text_engine.hpp"

#include <iostream>
namespace rive
{
class DataValueAssetFont : public DataValueInteger
{
public:
    DataValueAssetFont(uint32_t value) :
        DataValueInteger(value),
        m_fileAsset(rcp<FontAsset>(new FontAsset())) {};
    DataValueAssetFont() :
        DataValueInteger(-1), m_fileAsset(rcp<FontAsset>(new FontAsset())) {};
    static const DataType typeKey = DataType::assetFont;
    bool isTypeOf(DataType typeKey) const override
    {
        return typeKey == DataType::assetFont || typeKey == DataType::integer;
    };
    constexpr static uint32_t defaultValue = -1;
    rcp<FontAsset> fileAsset() { return m_fileAsset; }
    void fontValue(Font* font) { m_fileAsset->font(ref_rcp(font)); }
    Font* fontValue() { return m_fileAsset->font().get(); }

private:
    rcp<FontAsset> m_fileAsset = nullptr;
};
} // namespace rive
#endif
