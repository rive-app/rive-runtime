#ifndef _RIVE_DATA_CONVERTER_LIST_TO_LENGTH_HPP_
#define _RIVE_DATA_CONVERTER_LIST_TO_LENGTH_HPP_
#include "rive/generated/data_bind/converters/data_converter_list_to_length_base.hpp"
#include "rive/data_bind/data_values/data_value.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include <stdio.h>
namespace rive
{
class DataConverterListToLength : public DataConverterListToLengthBase
{
public:
    DataValue* convert(DataValue* value, DataBind* dataBind) override;
    DataType outputType() override { return DataType::number; };

private:
    DataValueNumber m_output;
};
} // namespace rive

#endif