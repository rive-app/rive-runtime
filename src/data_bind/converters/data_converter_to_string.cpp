#include "rive/math/math_types.hpp"
#include "rive/data_bind/converters/data_converter_to_string.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/data_bind/data_values/data_value_enum.hpp"
#include "rive/data_bind/data_values/data_value_color.hpp"
#include "rive/data_bind/data_values/data_value_string.hpp"
#include "rive/data_bind/data_values/data_value_boolean.hpp"
#include "rive/data_bind/data_values/data_value_trigger.hpp"
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

DataValue* DataConverterToString::convertColor(DataValueColor* input)
{
    auto colorValue = input->value();
    std::string outputValue = "";
    std::stringstream stream;
    if (colorFormat() != "")
    {
        m_converter.color(colorValue);

        bool isEscaped = false;
        bool isMarker = false;

        for (const char& c : colorFormat())
        {
            if (isEscaped)
            {
                stream << c;
                isEscaped = false;
            }
            // Escape
            else if (c == '\\')
            {
                if (isMarker)
                {
                    stream << '%';
                    isMarker = false;
                }
                isEscaped = true;
            }
            // Marker
            else if (c == '%')
            {
                if (isMarker)
                {
                    stream << '%';
                }
                isMarker = true;
            }
            else if (isMarker)
            {
                if (c == 'r')
                {
                    stream << m_converter.red();
                }
                else if (c == 'g')
                {
                    stream << m_converter.green();
                }
                else if (c == 'b')
                {
                    stream << m_converter.blue();
                }
                else if (c == 'a')
                {
                    stream << m_converter.alpha();
                }
                else if (c == 'R')
                {
                    m_converter.redHex(stream);
                }
                else if (c == 'G')
                {
                    m_converter.greenHex(stream);
                }
                else if (c == 'B')
                {
                    m_converter.blueHex(stream);
                }
                else if (c == 'A')
                {
                    m_converter.alphaHex(stream);
                }
                else if (c == 'h')
                {
                    stream << m_converter.hue();
                }
                else if (c == 'l')
                {
                    stream << m_converter.luminance();
                }
                else if (c == 's')
                {
                    stream << m_converter.saturation();
                }
                else
                {
                    stream << '%';
                    stream << c;
                }
                isMarker = false;
            }
            else
            {
                stream << c;
            }
            outputValue = stream.str();
        }
    }
    else
    {
        outputValue = std::to_string(colorValue);
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
    else if (input->is<DataValueColor>())
    {
        return convertColor(input->as<DataValueColor>());
    }
    else if (input->is<DataValueBoolean>())
    {

        auto value = input->as<DataValueBoolean>()->value();
        m_output.value(value ? "1" : "0");
    }
    else if (input->is<DataValueTrigger>())
    {

        auto value = input->as<DataValueTrigger>()->value();
        m_output.value(std::to_string(value));
    }
    else
    {
        m_output.value(DataValueString::defaultValue);
    }
    return &m_output;
}