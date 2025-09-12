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
    DataValueNumber(float value) : m_value(value) {};
    DataValueNumber() {};
    static const DataType typeKey = DataType::number;
    bool isTypeOf(DataType typeKey) const override
    {
        return typeKey == DataType::number;
    }
    float value() { return m_value; };
    void value(float value) { m_value = value; };
    constexpr static const float defaultValue = 0;
    bool compare(DataValue* comparand) override
    {
        if (comparand != nullptr && comparand->is<DataValueNumber>())
        {
            return comparand->as<DataValueNumber>()->value() == m_value;
        }
        return false;
    }
    void interpolate(DataValue* to, DataValue* destination, float f) override
    {
        if (to != nullptr && to->is<DataValueNumber>() &&
            destination != nullptr && destination->is<DataValueNumber>())
        {
            auto fromValue = value();
            auto toValue = to->as<DataValueNumber>()->value();
            float fi = 1.0f - f;
            auto result = toValue * f + fromValue * fi;
            destination->as<DataValueNumber>()->value(result);
        }
    }
    void copyValue(DataValue* destination) override
    {
        if (destination != nullptr && destination->is<DataValueNumber>())
        {
            destination->as<DataValueNumber>()->value(value());
        }
    }
};
} // namespace rive

#endif