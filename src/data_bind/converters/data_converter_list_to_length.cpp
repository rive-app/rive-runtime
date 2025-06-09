#include "rive/math/math_types.hpp"
#include "rive/data_bind/converters/data_converter_list_to_length.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/data_bind/data_values/data_value_list.hpp"

using namespace rive;

DataValue* DataConverterListToLength::convert(DataValue* input,
                                              DataBind* dataBind)
{
    if (input->is<DataValueList>())
    {
        size_t length = input->as<DataValueList>()->value()->size();
        m_output.value((float)length);
    }
    else
    {
        m_output.value(DataValueNumber::defaultValue);
    }
    return &m_output;
}