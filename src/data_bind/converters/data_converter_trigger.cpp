#include "rive/math/math_types.hpp"
#include "rive/data_bind/converters/data_converter_trigger.hpp"
#include "rive/data_bind/data_values/data_value_trigger.hpp"
#include "rive/data_bind/data_values/data_value_integer.hpp"

using namespace rive;

DataValue* DataConverterTrigger::convert(DataValue* input, DataBind* dataBind)
{
    if (input->is<DataValueInteger>())
    {
        uint32_t inputValue = input->as<DataValueInteger>()->value();
        m_output.value(inputValue + 1);
    }
    else
    {
        m_output.value(DataValueTrigger::defaultValue);
    }
    return &m_output;
}