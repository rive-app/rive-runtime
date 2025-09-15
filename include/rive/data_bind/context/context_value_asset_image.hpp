#ifndef _RIVE_DATA_BIND_CONTEXT_VALUE_ASSET_IMAGE_HPP_
#define _RIVE_DATA_BIND_CONTEXT_VALUE_ASSET_IMAGE_HPP_
#include "rive/data_bind/context/context_value.hpp"
#include "rive/data_bind/data_values/data_value_asset_image.hpp"
namespace rive
{
class ImageAsset;
class DataBindContextValueAssetImage : public DataBindContextValue
{

public:
    DataBindContextValueAssetImage(DataBind* m_dataBind);
    void apply(Core* component,
               uint32_t propertyKey,
               bool isMainDirection) override;
    rcp<ImageAsset> fileAsset();
};
} // namespace rive

#endif