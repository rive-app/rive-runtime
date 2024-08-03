#ifndef _RIVE_DATA_VALUE_COLOR_HPP_
#define _RIVE_DATA_VALUE_COLOR_HPP_
#include "rive/data_bind/data_values/data_value.hpp"

#include <stdio.h>
namespace rive
{
class DataValueColor : public DataValue
{
private:
    int m_value = false;

public:
    DataValueColor(int value) : m_value(value){};
    DataValueColor(){};
    static const DataType typeKey = DataType::color;
    bool isTypeOf(DataType typeKey) const override { return typeKey == DataType::color; }
    int value() { return m_value; };
    void value(int value) { m_value = value; };
};
} // namespace rive

#endif