#include "rive/math/math_types.hpp"
#include "rive/data_bind/converters/data_converter_string_pad.hpp"
#include "rive/data_bind/data_values/data_value_string.hpp"

using namespace rive;

DataValue* DataConverterStringPad::convert(DataValue* input, DataBind* dataBind)
{
    if (input->is<DataValueString>())
    {
        auto inputValue = input->as<DataValueString>()->value();
        auto inputLength = inputValue.size();
        if (inputLength < length())
        {
            auto padPattern = text();
            auto padLength = padPattern.size();
            inputValue.reserve(length());
            std::string padText{""};
            int padTextSize = length() - inputLength;
            padText.reserve(padTextSize);
            while (inputLength < length())
            {
                auto maxLength =
                    (padTextSize > padLength) ? padLength : padTextSize;
                padText.append(padPattern, 0, maxLength);
                inputLength += maxLength;
            }
            if (padType() == 1)
            {
                inputValue.append(padText, 0, padTextSize);
            }
            else
            {
                inputValue.insert(0, padText, 0, padTextSize);
            }
        }
        m_output.value(inputValue);
    }
    else
    {
        m_output.value(DataValueString::defaultValue);
    }
    return &m_output;
}