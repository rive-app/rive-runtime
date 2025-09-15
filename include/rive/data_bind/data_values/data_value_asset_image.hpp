#ifndef _RIVE_DATA_VALUE_ASSET_IMAGE_HPP_
#define _RIVE_DATA_VALUE_ASSET_IMAGE_HPP_
#include "rive/data_bind/data_values/data_value_integer.hpp"

#include <iostream>
namespace rive
{
class DataValueAssetImage : public DataValueInteger
{
public:
    DataValueAssetImage(uint32_t value) : DataValueInteger(value) {};
    DataValueAssetImage() : DataValueInteger(-1) {};
    static const DataType typeKey = DataType::assetImage;
    bool isTypeOf(DataType typeKey) const override
    {
        return typeKey == DataType::assetImage || typeKey == DataType::integer;
    };
    constexpr static uint32_t defaultValue = -1;
};
} // namespace rive
#endif