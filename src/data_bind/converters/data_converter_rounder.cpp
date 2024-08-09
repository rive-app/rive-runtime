#include "rive/math/math_types.hpp"
#include "rive/data_bind/converters/data_converter_rounder.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"

using namespace rive;

DataValue* DataConverterRounder::convert(DataValue* input)
{
    auto output = new DataValueNumber();
    if (input->is<DataValueNumber>())
    {
        float value = input->as<DataValueNumber>()->value();
        auto numberOfPlaces = decimals();
        // TODO: @hernan review this way of rounding
        float rounder = pow(10.0f, (float)numberOfPlaces);
        output->value(std::round(value * rounder) / rounder);
    }
    return output;
}