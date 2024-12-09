#include "rive/math/math_types.hpp"
#include "rive/data_bind/converters/data_converter_rounder.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"

using namespace rive;

DataValue* DataConverterRounder::convert(DataValue* input, DataBind* dataBind)
{
    if (input->is<DataValueNumber>())
    {
        float value = input->as<DataValueNumber>()->value();
        auto numberOfPlaces = decimals();
        // TODO: @hernan review this way of rounding
        float rounder = pow(10.0f, (float)numberOfPlaces);
        m_output.value(std::round(value * rounder) / rounder);
    }
    else
    {
        m_output.value(DataValueNumber::defaultValue);
    }
    return &m_output;
}