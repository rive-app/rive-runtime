#include "rive/math/math_types.hpp"
#include "rive/data_bind/converters/data_converter_trigger.hpp"
#include "rive/data_bind/data_values/data_value_trigger.hpp"

using namespace rive;

DataValue* DataConverterTrigger::convert(DataValue* input, DataBind* dataBind)
{
    auto output = new DataValueTrigger();
    if (input->is<DataValueTrigger>())
    {
        uint32_t inputValue = input->as<DataValueTrigger>()->value();
        output->value(inputValue + 1);
    }
    return output;
}