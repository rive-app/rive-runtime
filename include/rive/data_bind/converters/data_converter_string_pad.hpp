#ifndef _RIVE_DATA_CONVERTER_STRING_PAD_HPP_
#define _RIVE_DATA_CONVERTER_STRING_PAD_HPP_
#include "rive/generated/data_bind/converters/data_converter_string_pad_base.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/data_bind/data_values/data_value_string.hpp"
#include <stdio.h>
namespace rive
{
class DataConverterStringPad : public DataConverterStringPadBase
{
public:
    DataValue* convert(DataValue* value, DataBind* dataBind) override;
    DataType outputType() override { return DataType::string; };
    void lengthChanged() override;
    void padTypeChanged() override;
    void textChanged() override;

private:
    DataValueString m_output;
};
} // namespace rive

#endif