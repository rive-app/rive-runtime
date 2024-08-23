#ifndef _RIVE_DATA_CONVERTER_TO_STRING_HPP_
#define _RIVE_DATA_CONVERTER_TO_STRING_HPP_
#include "rive/generated/data_bind/converters/data_converter_to_string_base.hpp"
#include <stdio.h>
namespace rive
{
class DataConverterToString : public DataConverterToStringBase
{
public:
    DataValue* convert(DataValue* value) override;
    DataType outputType() override { return DataType::string; };
};
} // namespace rive

#endif