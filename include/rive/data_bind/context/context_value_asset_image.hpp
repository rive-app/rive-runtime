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
    bool syncTargetValue(Core* target, uint32_t propertyKey) override;
    ImageAsset* fileAsset();

protected:
    DataValue* targetValue() override { return &m_targetDataValue; }

private:
    uint32_t m_previousValue = -1;
    DataValueAssetImage m_targetDataValue;
};
} // namespace rive

#endif