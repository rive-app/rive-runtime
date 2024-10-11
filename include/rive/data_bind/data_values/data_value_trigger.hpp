#ifndef _RIVE_DATA_VALUE_TRIGGER_HPP_
#define _RIVE_DATA_VALUE_TRIGGER_HPP_
#include "rive/data_bind/data_values/data_value.hpp"

#include <stdio.h>
namespace rive
{
class DataValueTrigger : public DataValue
{
private:
    uint32_t m_value = 0;

public:
    DataValueTrigger(uint32_t value) : m_value(value){};
    DataValueTrigger(){};
    static const DataType typeKey = DataType::trigger;
    bool isTypeOf(DataType typeKey) const override
    {
        return typeKey == DataType::trigger;
    }
    uint32_t value() { return m_value; };
    void value(uint32_t value) { m_value = value; };
    constexpr static const uint32_t defaultValue = 0;
};
} // namespace rive

#endif