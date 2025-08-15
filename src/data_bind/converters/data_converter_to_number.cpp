#include "rive/math/math_types.hpp"
#include "rive/data_bind/converters/data_converter_to_number.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/data_bind/data_values/data_value_enum.hpp"
#include "rive/data_bind/data_values/data_value_color.hpp"
#include "rive/data_bind/data_values/data_value_string.hpp"
#include "rive/data_bind/data_values/data_value_boolean.hpp"
#include "rive/data_bind/data_values/data_value_trigger.hpp"
#include "rive/data_bind/data_values/data_value_symbol_list_index.hpp"
#include "rive/animation/data_converter_to_string_flags.hpp"
#include "rive/data_bind/converters/data_converter_string_remove_zeros.hpp"

using namespace rive;

// Not supported for now. We need to find a way to match the output of dart
// with the output of cpp
DataValue* DataConverterToNumber::convertString(DataValueString* input)
{
    auto value = input->value();
    errno = 0; // Clear errno before the call
    float v = std::atof(value.c_str());
    if (errno == 0)
    {
        m_output.value(v);
    }
    return &m_output;
}

DataValue* DataConverterToNumber::convertColor(DataValueColor* input)
{
    auto colorValue = input->value();
    m_output.value(static_cast<float>(colorValue));
    return &m_output;
}

DataValue* DataConverterToNumber::convertEnum(DataValueEnum* input)
{
    auto index = input->as<DataValueEnum>()->value();
    m_output.value(float(index));
    return &m_output;
}

DataValue* DataConverterToNumber::convert(DataValue* input, DataBind* dataBind)
{
    if (input->is<DataValueString>())
    {
        return convertString(input->as<DataValueString>());
    }
    else if (input->is<DataValueEnum>())
    {
        return convertEnum(input->as<DataValueEnum>());
    }
    else if (input->is<DataValueNumber>())
    {
        m_output.value(input->as<DataValueNumber>()->value());
    }
    else if (input->is<DataValueColor>())
    {
        return convertColor(input->as<DataValueColor>());
    }
    else if (input->is<DataValueBoolean>())
    {

        auto value = input->as<DataValueBoolean>()->value();
        m_output.value(value ? 1.0f : 0.0f);
    }
    else if (input->is<DataValueSymbolListIndex>())
    {
        auto value = input->as<DataValueSymbolListIndex>()->value();
        m_output.value(float(value));
    }
    else
    {
        m_output.value(DataValueNumber::defaultValue);
    }
    return &m_output;
}