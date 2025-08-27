#include "rive/math/math_types.hpp"
#include "rive/data_bind/converters/data_converter_string_remove_zeros.hpp"
#include "rive/data_bind/data_values/data_value_string.hpp"

using namespace rive;

std::string DataConverterStringRemoveZeros::removeZeros(std::string value)
{
    std::string inputValue = value;
    if (inputValue.find('.') != std::string::npos)
    {
        // Remove trailing zeroes
        inputValue = inputValue.substr(0, inputValue.find_last_not_of('0') + 1);
        // If the decimal point is now the last character, remove that as
        // well
        if (inputValue.find('.') == inputValue.size() - 1)
        {
            inputValue = inputValue.substr(0, inputValue.size() - 1);
        }
    }
    return inputValue;
}

DataValue* DataConverterStringRemoveZeros::convert(DataValue* input,
                                                   DataBind* dataBind)
{
    if (input->is<DataValueString>())
    {
        auto inputValue = removeZeros(input->as<DataValueString>()->value());
        m_output.value(inputValue);
    }
    else
    {
        m_output.value(DataValueString::defaultValue);
    }
    return &m_output;
}