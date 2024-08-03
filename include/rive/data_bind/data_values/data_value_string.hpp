#ifndef _RIVE_DATA_VALUE_STRING_HPP_
#define _RIVE_DATA_VALUE_STRING_HPP_
#include "rive/data_bind/data_values/data_value.hpp"

#include <iostream>
namespace rive
{
class DataValueString : public DataValue
{
private:
    std::string m_value = "";

public:
    DataValueString(std::string value) : m_value(value){};
    DataValueString(){};
    static const DataType typeKey = DataType::string;
    bool isTypeOf(DataType typeKey) const override { return typeKey == DataType::string; };
    std::string value() { return m_value; };
    void value(std::string value) { m_value = value; };
};
} // namespace rive
#endif