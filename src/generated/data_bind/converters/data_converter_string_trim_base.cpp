#include "rive/generated/data_bind/converters/data_converter_string_trim_base.hpp"
#include "rive/data_bind/converters/data_converter_string_trim.hpp"

using namespace rive;

Core* DataConverterStringTrimBase::clone() const
{
    auto cloned = new DataConverterStringTrim();
    cloned->copy(*this);
    return cloned;
}
