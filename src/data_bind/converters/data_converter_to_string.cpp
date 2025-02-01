#include "rive/math/math_types.hpp"
#include "rive/data_bind/converters/data_converter_to_string.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/data_bind/data_values/data_value_enum.hpp"
#include "rive/data_bind/data_values/data_value_string.hpp"
#include "rive/animation/data_converter_to_string_flags.hpp"
#include "rive/data_bind/converters/data_converter_string_remove_zeros.hpp"
#include <iomanip>
#include <sstream>

using namespace rive;

DataValue* DataConverterToString::convertNumber(DataValueNumber* input)
{
    auto flagsValue = static_cast<DataConverterToStringFlags>(flags());
    float value = input->as<DataValueNumber>()->value();
    std::string outputValue;
    if ((flagsValue & DataConverterToStringFlags::Round) ==
        DataConverterToStringFlags::Round)
    {
        std::stringstream stream;
        stream << std::fixed << std::setprecision(decimals()) << value;
        outputValue = stream.str();
    }
    else
    {
        outputValue = std::to_string(value);
    }
    if ((flagsValue & DataConverterToStringFlags::TrailingZeros) ==
        DataConverterToStringFlags::TrailingZeros)
    {
        outputValue = DataConverterStringRemoveZeros::removeZeros(outputValue);
    }
    m_output.value(outputValue);
    return &m_output;
}

DataValue* DataConverterToString::convert(DataValue* input, DataBind* dataBind)
{
    if (input->is<DataValueNumber>())
    {
        return convertNumber(input->as<DataValueNumber>());
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