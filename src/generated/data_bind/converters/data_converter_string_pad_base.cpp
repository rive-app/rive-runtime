#include "rive/generated/data_bind/converters/data_converter_string_pad_base.hpp"
#include "rive/data_bind/converters/data_converter_string_pad.hpp"

using namespace rive;

Core* DataConverterStringPadBase::clone() const
{
    auto cloned = new DataConverterStringPad();
    cloned->copy(*this);
    return cloned;
}
