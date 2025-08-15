#ifndef _RIVE_DATA_CONVERTER_TO_NUMBER_HPP_
#define _RIVE_DATA_CONVERTER_TO_NUMBER_HPP_
#include "rive/generated/data_bind/converters/data_converter_to_number_base.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/data_bind/data_values/data_value_string.hpp"
#include "rive/data_bind/data_values/data_value_color.hpp"
#include "rive/data_bind/data_values/data_value_enum.hpp"
#include <stdio.h>
namespace rive
{
class DataConverterToNumber : public DataConverterToNumberBase
{
public:
    DataValue* convert(DataValue* value, DataBind* dataBind) override;
    DataType outputType() override { return DataType::number; };

private:
    DataValue* convertString(DataValueString* value);
    DataValue* convertColor(DataValueColor* value);
    DataValue* convertEnum(DataValueEnum* value);
    DataValueNumber m_output;
};
} // namespace rive

#endif