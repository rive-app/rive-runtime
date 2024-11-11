#include "rive/math/math_types.hpp"
#include "rive/data_bind/converters/data_converter_operation_value.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"

using namespace rive;

DataValue* DataConverterOperationValue::convert(DataValue* input,
                                                DataBind* dataBind)
{
    return convertValue(input, value());
}

DataValue* DataConverterOperationValue::reverseConvert(DataValue* input,
                                                       DataBind* dataBind)
{
    return reverseConvertValue(input, value());
}