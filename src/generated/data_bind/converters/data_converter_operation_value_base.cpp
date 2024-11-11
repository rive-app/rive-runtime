#include "rive/generated/data_bind/converters/data_converter_operation_value_base.hpp"
#include "rive/data_bind/converters/data_converter_operation_value.hpp"

using namespace rive;

Core* DataConverterOperationValueBase::clone() const
{
    auto cloned = new DataConverterOperationValue();
    cloned->copy(*this);
    return cloned;
}
