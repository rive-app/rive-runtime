#ifndef _RIVE_DATA_VALUE_ENUM_HPP_
#define _RIVE_DATA_VALUE_ENUM_HPP_
#include "rive/data_bind/data_values/data_value.hpp"
#include "rive/viewmodel/data_enum.hpp"

#include <iostream>
namespace rive
{
class DataValueEnum : public DataValue
{
private:
    uint32_t m_value = 0;
    DataEnum* m_dataEnum;

public:
    DataValueEnum(uint32_t value, DataEnum* dataEnum) : m_value(value), m_dataEnum(dataEnum){};
    DataValueEnum(){};
    static const DataType typeKey = DataType::enumType;
    bool isTypeOf(DataType typeKey) const override { return typeKey == DataType::enumType; };
    uint32_t value() { return m_value; };
    void value(uint32_t value) { m_value = value; };
    DataEnum* dataEnum() { return m_dataEnum; };
    void dataEnum(DataEnum* value) { m_dataEnum = value; };
};
} // namespace rive
#endif