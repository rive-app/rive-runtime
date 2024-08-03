#ifndef _RIVE_DATA_VALUE_BOOLEAN_HPP_
#define _RIVE_DATA_VALUE_BOOLEAN_HPP_
#include "rive/data_bind/data_values/data_value.hpp"

#include <stdio.h>
namespace rive
{
class DataValueBoolean : public DataValue
{
private:
    bool m_value = false;

public:
    DataValueBoolean(bool value) : m_value(value){};
    DataValueBoolean(){};
    static const DataType typeKey = DataType::boolean;
    bool isTypeOf(DataType typeKey) const override { return typeKey == DataType::boolean; }
    bool value() { return m_value; };
    void value(bool value) { m_value = value; };
};
} // namespace rive

#endif