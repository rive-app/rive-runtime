#ifndef _RIVE_DATA_CONVERTER_HPP_
#define _RIVE_DATA_CONVERTER_HPP_
#include "rive/generated/data_bind/converters/data_converter_base.hpp"
#include "rive/data_bind/data_values/data_value.hpp"
#include <stdio.h>
namespace rive
{
class DataConverter : public DataConverterBase
{
public:
    virtual DataValue* convert(DataValue* value) { return value; };
    virtual DataValue* reverseConvert(DataValue* value) { return value; };
    virtual DataType outputType() { return DataType::none; };
    StatusCode import(ImportStack& importStack) override;
};
} // namespace rive

#endif