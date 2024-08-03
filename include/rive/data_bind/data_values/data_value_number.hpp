#ifndef _RIVE_DATA_VALUE_NUMBER_HPP_
#define _RIVE_DATA_VALUE_NUMBER_HPP_
#include "rive/data_bind/data_values/data_value.hpp"

#include <stdio.h>
namespace rive
{
class DataValueNumber : public DataValue
{
private:
    float m_value = 0;

public:
    DataValueNumber(float value) : m_value(value){};
    DataValueNumber(){};
    static const DataType typeKey = DataType::number;
    bool isTypeOf(DataType typeKey) const override { return typeKey == DataType::number; }
    float value() { return m_value; };
    void value(float value) { m_value = value; };
};
} // namespace rive

#endif