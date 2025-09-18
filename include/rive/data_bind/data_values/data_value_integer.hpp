#ifndef _RIVE_DATA_VALUE_INTEGER_HPP_
#define _RIVE_DATA_VALUE_INTEGER_HPP_
#include "rive/data_bind/data_values/data_value.hpp"

#include <stdio.h>
namespace rive
{
class DataValueInteger : public DataValue
{
private:
    uint32_t m_value = 0;

public:
    DataValueInteger(uint32_t value) : m_value(value) {};
    DataValueInteger() {};
    static const DataType typeKey = DataType::integer;
    bool isTypeOf(DataType typeKey) const override
    {
        return typeKey == DataType::integer;
    }
    uint32_t value() { return m_value; };
    void value(uint32_t value) { m_value = value; };
    constexpr static const uint32_t defaultValue = 0;
};
} // namespace rive

#endif