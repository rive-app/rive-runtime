#ifndef _RIVE_DATA_CONVERTER_BOOLEAN_NEGATE_HPP_
#define _RIVE_DATA_CONVERTER_BOOLEAN_NEGATE_HPP_
#include "rive/generated/data_bind/converters/data_converter_boolean_negate_base.hpp"
#include "rive/data_bind/data_values/data_value.hpp"
#include "rive/data_bind/data_values/data_value_boolean.hpp"
#include "rive/data_bind/data_bind.hpp"
#include <stdio.h>
namespace rive
{
class DataConverterBooleanNegate : public DataConverterBooleanNegateBase
{
public:
    DataType outputType() override { return DataType::boolean; };
    DataValue* convert(DataValue* value, DataBind* dataBind) override;
    DataValue* reverseConvert(DataValue* value, DataBind* dataBind) override;

private:
    DataValueBoolean m_output;
};
} // namespace rive

#endif