#ifndef _RIVE_DATA_CONVERTER_STRING_REMOVE_ZEROS_HPP_
#define _RIVE_DATA_CONVERTER_STRING_REMOVE_ZEROS_HPP_
#include "rive/generated/data_bind/converters/data_converter_string_remove_zeros_base.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/data_bind/data_values/data_value_string.hpp"
#include <stdio.h>
namespace rive
{
class DataConverterStringRemoveZeros : public DataConverterStringRemoveZerosBase
{
public:
    static std::string removeZeros(std::string string);
    DataValue* convert(DataValue* value, DataBind* dataBind) override;
    DataType outputType() override { return DataType::string; };

private:
    DataValueString m_output;
};
} // namespace rive

#endif