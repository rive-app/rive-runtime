#include "rive/generated/data_bind/converters/data_converter_range_mapper_values_base.hpp"
#include "rive/data_bind/converters/data_converter_range_mapper_values.hpp"

using namespace rive;

Core* DataConverterRangeMapperValuesBase::clone() const
{
    auto cloned = new DataConverterRangeMapperValues();
    cloned->copy(*this);
    return cloned;
}
