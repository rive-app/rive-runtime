#ifndef _RIVE_DATA_BIND_CONTEXT_VALUE_ASSET_FONT_HPP_
#define _RIVE_DATA_BIND_CONTEXT_VALUE_ASSET_FONT_HPP_
#include "rive/data_bind/context/context_value.hpp"
#include "rive/data_bind/data_values/data_value_asset_font.hpp"
namespace rive
{
class FontAsset;
class DataBindContextValueAssetFont : public DataBindContextValue
{

public:
    DataBindContextValueAssetFont(DataBind* m_dataBind);
    void apply(Core* component,
               uint32_t propertyKey,
               bool isMainDirection,
               DataBind* dataBind) override;
    rcp<FontAsset> fileAsset(DataBind* dataBind);
};
} // namespace rive

#endif
