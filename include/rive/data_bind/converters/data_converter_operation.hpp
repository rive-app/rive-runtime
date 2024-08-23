#ifndef _RIVE_DATA_CONVERTER_OPERATION_HPP_
#define _RIVE_DATA_CONVERTER_OPERATION_HPP_
#include "rive/generated/data_bind/converters/data_converter_operation_base.hpp"
#include "rive/animation/arithmetic_operation.hpp"
#include <stdio.h>
namespace rive
{
class DataConverterOperation : public DataConverterOperationBase
{
public:
    DataValue* convert(DataValue* value) override;
    DataValue* reverseConvert(DataValue* value) override;
    DataType outputType() override { return DataType::number; };
    ArithmeticOperation op() const { return (ArithmeticOperation)operationType(); }
};
} // namespace rive

#endif