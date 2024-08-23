#include "rive/math/math_types.hpp"
#include "rive/data_bind/converters/data_converter_to_string.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/data_bind/data_values/data_value_enum.hpp"
#include "rive/data_bind/data_values/data_value_string.hpp"

using namespace rive;

DataValue* DataConverterToString::convert(DataValue* input)
{
    auto output = new DataValueString();
    if (input->is<DataValueNumber>())
    {
        float value = input->as<DataValueNumber>()->value();
        std::string str = std::to_string(value);
        if (str.find('.') != std::string::npos)
        {
            // Remove trailing zeroes
            str = str.substr(0, str.find_last_not_of('0') + 1);
            // If the decimal point is now the last character, remove that as well
            if (str.find('.') == str.size() - 1)
            {
                str = str.substr(0, str.size() - 1);
            }
        }
        output->value(str);
    }
    else if (input->is<DataValueEnum>())
    {
        auto dataEnum = input->as<DataValueEnum>()->dataEnum();
        auto index = input->as<DataValueEnum>()->value();
        auto enumValue = dataEnum->value(index);
        output->value(enumValue);
    }
    return output;
}