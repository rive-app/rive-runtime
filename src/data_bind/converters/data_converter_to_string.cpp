#include "rive/math/math_types.hpp"
#include "rive/data_bind/converters/data_converter_to_string.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/data_bind/data_values/data_value_enum.hpp"
#include "rive/data_bind/data_values/data_value_string.hpp"

using namespace rive;

DataValue* DataConverterToString::convert(DataValue* input, DataBind* dataBind)
{
    if (input->is<DataValueNumber>())
    {
        float value = input->as<DataValueNumber>()->value();
        std::string str = std::to_string(value);
        m_output.value(str);
    }
    else if (input->is<DataValueEnum>())
    {
        auto dataEnum = input->as<DataValueEnum>()->dataEnum();
        auto index = input->as<DataValueEnum>()->value();
        auto enumValue = dataEnum->value(index);
        m_output.value(enumValue);
    }
    else if (input->is<DataValueString>())
    {
        m_output.value(input->as<DataValueString>()->value());
    }
    else
    {
        m_output.value(DataValueString::defaultValue);
    }
    return &m_output;
}