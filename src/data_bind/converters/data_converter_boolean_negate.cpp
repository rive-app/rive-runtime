#include "rive/math/math_types.hpp"
#include "rive/data_bind/converters/data_converter_boolean_negate.hpp"
#include "rive/data_bind/data_values/data_value_boolean.hpp"

using namespace rive;

DataValue* DataConverterBooleanNegate::convert(DataValue* input,
                                               DataBind* dataBind)
{
    if (input->is<DataValueBoolean>())
    {
        m_output.value(!input->as<DataValueBoolean>()->value());
    }
    else
    {
        m_output.value(DataValueBoolean::defaultValue);
    }
    return &m_output;
}

DataValue* DataConverterBooleanNegate::reverseConvert(DataValue* input,
                                                      DataBind* dataBind)
{
    return convert(input, dataBind);
}