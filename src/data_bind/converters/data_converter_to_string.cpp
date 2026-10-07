#include "rive/math/math_types.hpp"
#include "rive/data_bind/converters/data_converter_to_string.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/data_bind/data_values/data_value_enum.hpp"
#include "rive/data_bind/data_values/data_value_color.hpp"
#include "rive/data_bind/data_values/data_value_string.hpp"
#include "rive/data_bind/data_values/data_value_boolean.hpp"
#include "rive/data_bind/data_values/data_value_trigger.hpp"
#include "rive/data_bind/data_values/data_value_symbol_list_index.hpp"
#include "rive/animation/data_converter_to_string_flags.hpp"
#include "rive/data_bind/converters/data_converter_string_remove_zeros.hpp"
#include <stdio.h>

using namespace rive;

std::string DataConverterToString::formatWithCommas(std::string& num)
{
    // Split integer and fractional parts
    std::string intPart, fracPart;
    size_t dotPos = num.find('.');
    if (dotPos != std::string::npos)
    {
        intPart = num.substr(0, dotPos);
        fracPart = num.substr(dotPos); // keep the '.' too
    }
    else
    {
        intPart = num;
    }

    // Insert commas into the integer part
    int insertPosition = (int)intPart.length() - 3;
    while (insertPosition > 0 && isdigit(intPart[insertPosition - 1]))
    {
        intPart.insert(insertPosition, ",");
        insertPosition -= 3;
    }

    return intPart + fracPart;
}

DataValue* DataConverterToString::convertNumber(DataValueNumber* input)
{
    auto flagsValue = static_cast<DataConverterToStringFlags>(flags());
    float value = input->as<DataValueNumber>()->value();
    std::string outputValue;
    if ((flagsValue & DataConverterToStringFlags::Round) ==
        DataConverterToStringFlags::Round)
    {
        int precision = (int)decimals();
        int length = snprintf(nullptr, 0, "%.*f", precision, value);
        outputValue.resize(length);
        snprintf(&outputValue[0], length + 1, "%.*f", precision, value);
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
    if ((flagsValue & DataConverterToStringFlags::FormatWithCommas) ==
        DataConverterToStringFlags::FormatWithCommas)
    {
        outputValue = formatWithCommas(outputValue);
    }
    m_output.value(outputValue);
    return &m_output;
}

DataValue* DataConverterToString::convertColor(DataValueColor* input)
{
    auto colorValue = input->value();
    std::string outputValue = "";
    if (colorFormat() != "")
    {
        m_converter.color(colorValue);

        bool isEscaped = false;
        bool isMarker = false;

        for (const char& c : colorFormat())
        {
            if (isEscaped)
            {
                outputValue += c;
                isEscaped = false;
            }
            // Escape
            else if (c == '\\')
            {
                if (isMarker)
                {
                    outputValue += '%';
                    isMarker = false;
                }
                isEscaped = true;
            }
            // Marker
            else if (c == '%')
            {
                if (isMarker)
                {
                    outputValue += '%';
                }
                isMarker = true;
            }
            else if (isMarker)
            {
                if (c == 'r')
                {
                    outputValue += m_converter.red();
                }
                else if (c == 'g')
                {
                    outputValue += m_converter.green();
                }
                else if (c == 'b')
                {
                    outputValue += m_converter.blue();
                }
                else if (c == 'a')
                {
                    outputValue += m_converter.alpha();
                }
                else if (c == 'R')
                {
                    m_converter.redHex(outputValue);
                }
                else if (c == 'G')
                {
                    m_converter.greenHex(outputValue);
                }
                else if (c == 'B')
                {
                    m_converter.blueHex(outputValue);
                }
                else if (c == 'A')
                {
                    m_converter.alphaHex(outputValue);
                }
                else if (c == 'h')
                {
                    outputValue += m_converter.hue();
                }
                else if (c == 'l')
                {
                    outputValue += m_converter.luminance();
                }
                else if (c == 's')
                {
                    outputValue += m_converter.saturation();
                }
                else
                {
                    outputValue += '%';
                    outputValue += c;
                }
                isMarker = false;
            }
            else
            {
                outputValue += c;
            }
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
        if (dataEnum)
        {
            auto index = input->as<DataValueEnum>()->value();
            auto enumValue = dataEnum->value(index);
            m_output.value(enumValue);
        }
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
    else if (input->is<DataValueSymbolListIndex>())
    {
        auto value = input->as<DataValueSymbolListIndex>()->value();
        m_output.value(std::to_string(value));
    }
    else
    {
        m_output.value(DataValueString::defaultValue);
    }
    return &m_output;
}

void DataConverterToString::decimalsChanged() { markConverterDirty(); }

void DataConverterToString::colorFormatChanged() { markConverterDirty(); }