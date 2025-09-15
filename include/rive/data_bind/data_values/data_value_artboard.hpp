#ifndef _RIVE_DATA_VALUE_ARTBOARD_HPP_
#define _RIVE_DATA_VALUE_ARTBOARD_HPP_
#include "rive/data_bind/data_values/data_value.hpp"
#include "rive/data_bind/data_values/data_value_integer.hpp"

#include <iostream>
namespace rive
{
class DataValueArtboard : public DataValueInteger
{
public:
    DataValueArtboard(uint32_t value) : DataValueInteger(value) {};
    DataValueArtboard() : DataValueInteger(-1) {};
    static const DataType typeKey = DataType::artboard;
    bool isTypeOf(DataType typeKey) const override
    {
        return typeKey == DataType::artboard || typeKey == DataType::integer;
    };
    constexpr static uint32_t defaultValue = -1;
};
} // namespace rive
#endif