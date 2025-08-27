#include "rive/generated/data_bind/converters/data_converter_range_mapper_base.hpp"
#include "rive/data_bind/converters/data_converter_range_mapper.hpp"

using namespace rive;

Core* DataConverterRangeMapperBase::clone() const
{
    auto cloned = new DataConverterRangeMapper();
    cloned->copy(*this);
    return cloned;
}
