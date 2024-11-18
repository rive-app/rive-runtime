#include "rive/math/math_types.hpp"
#include "rive/data_bind/converters/data_converter_range_mapper_values.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/animation/data_converter_range_mapper_flags.hpp"

using namespace rive;

DataValue* DataConverterRangeMapperValues::convert(DataValue* input,
                                                   DataBind* dataBind)
{
    return calculateRange(input,
                          minInput(),
                          maxInput(),
                          minOutput(),
                          maxOutput());
}

DataValue* DataConverterRangeMapperValues::reverseConvert(DataValue* input,
                                                          DataBind* dataBind)
{
    return calculateReverseRange(input,
                                 minInput(),
                                 maxInput(),
                                 minOutput(),
                                 maxOutput());
}