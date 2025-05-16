#ifndef _RIVE_DATA_VALUE_ASSET_IMAGE_HPP_
#define _RIVE_DATA_VALUE_ASSET_IMAGE_HPP_
#include "rive/data_bind/data_values/data_value.hpp"

#include <iostream>
namespace rive
{
class DataValueAssetImage : public DataValue
{
private:
    uint32_t m_value = -1;

public:
    DataValueAssetImage(uint32_t value) : m_value(value){};
    DataValueAssetImage(){};
    static const DataType typeKey = DataType::assetImage;
    bool isTypeOf(DataType typeKey) const override
    {
        return typeKey == DataType::assetImage;
    };
    uint32_t value() { return m_value; };
    void value(uint32_t value) { m_value = value; };
    constexpr static uint32_t defaultValue = -1;
};
} // namespace rive
#endif