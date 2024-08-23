#include "rive/generated/data_bind/converters/data_converter_operation_base.hpp"
#include "rive/data_bind/converters/data_converter_operation.hpp"

using namespace rive;

Core* DataConverterOperationBase::clone() const
{
    auto cloned = new DataConverterOperation();
    cloned->copy(*this);
    return cloned;
}
