#include "rive/generated/data_bind/converters/data_converter_list_to_length_base.hpp"
#include "rive/data_bind/converters/data_converter_list_to_length.hpp"

using namespace rive;

Core* DataConverterListToLengthBase::clone() const
{
    auto cloned = new DataConverterListToLength();
    cloned->copy(*this);
    return cloned;
}
