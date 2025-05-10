#ifndef _RIVE_DATA_VALUE_SYMBOL_LIST_INDEX_HPP_
#define _RIVE_DATA_VALUE_SYMBOL_LIST_INDEX_HPP_
#include "rive/data_bind/data_values/data_value_integer.hpp"

#include <stdio.h>
namespace rive
{
class DataValueSymbolListIndex : public DataValueInteger
{

public:
    DataValueSymbolListIndex(uint32_t value) : DataValueInteger(value){};
    DataValueSymbolListIndex(){};
    static const DataType typeKey = DataType::symbolListIndex;
    bool isTypeOf(DataType typeKey) const override
    {
        return typeKey == DataType::symbolListIndex;
    }
};
} // namespace rive

#endif