#include "rive/generated/data_bind/converters/data_converter_boolean_negate_base.hpp"
#include "rive/data_bind/converters/data_converter_boolean_negate.hpp"

using namespace rive;

Core* DataConverterBooleanNegateBase::clone() const
{
    auto cloned = new DataConverterBooleanNegate();
    cloned->copy(*this);
    return cloned;
}
