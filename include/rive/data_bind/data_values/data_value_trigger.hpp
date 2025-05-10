#ifndef _RIVE_DATA_VALUE_TRIGGER_HPP_
#define _RIVE_DATA_VALUE_TRIGGER_HPP_
#include "rive/data_bind/data_values/data_value_integer.hpp"

#include <stdio.h>
namespace rive
{
class DataValueTrigger : public DataValueInteger
{

public:
    DataValueTrigger(uint32_t value) : DataValueInteger(value){};
    DataValueTrigger(){};
    static const DataType typeKey = DataType::trigger;
    bool isTypeOf(DataType typeKey) const override
    {
        return typeKey == DataType::trigger;
    }
};
} // namespace rive

#endif