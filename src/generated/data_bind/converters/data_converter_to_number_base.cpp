#include "rive/generated/data_bind/converters/data_converter_to_number_base.hpp"
#include "rive/data_bind/converters/data_converter_to_number.hpp"

using namespace rive;

Core* DataConverterToNumberBase::clone() const
{
    auto cloned = new DataConverterToNumber();
    cloned->copy(*this);
    return cloned;
}
