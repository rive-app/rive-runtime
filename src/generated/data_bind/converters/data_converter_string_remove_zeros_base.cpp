#include "rive/generated/data_bind/converters/data_converter_string_remove_zeros_base.hpp"
#include "rive/data_bind/converters/data_converter_string_remove_zeros.hpp"

using namespace rive;

Core* DataConverterStringRemoveZerosBase::clone() const
{
    auto cloned = new DataConverterStringRemoveZeros();
    cloned->copy(*this);
    return cloned;
}
