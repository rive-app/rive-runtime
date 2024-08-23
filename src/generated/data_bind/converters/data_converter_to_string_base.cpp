#include "rive/generated/data_bind/converters/data_converter_to_string_base.hpp"
#include "rive/data_bind/converters/data_converter_to_string.hpp"

using namespace rive;

Core* DataConverterToStringBase::clone() const
{
    auto cloned = new DataConverterToString();
    cloned->copy(*this);
    return cloned;
}
