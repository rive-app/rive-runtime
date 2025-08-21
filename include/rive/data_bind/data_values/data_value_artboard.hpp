#ifndef _RIVE_DATA_VALUE_ARTBOARD_HPP_
#define _RIVE_DATA_VALUE_ARTBOARD_HPP_
#include "rive/data_bind/data_values/data_value.hpp"

#include <iostream>
namespace rive
{
class DataValueArtboard : public DataValue
{
private:
    uint32_t m_value = -1;

public:
    DataValueArtboard(uint32_t value) : m_value(value) {};
    DataValueArtboard() {};
    static const DataType typeKey = DataType::artboard;
    bool isTypeOf(DataType typeKey) const override
    {
        return typeKey == DataType::artboard;
    };
    uint32_t value() { return m_value; };
    void value(uint32_t value) { m_value = value; };
    constexpr static uint32_t defaultValue = -1;
};
} // namespace rive
#endif