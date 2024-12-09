#ifndef _RIVE_DATA_CONVERTER_OPERATION_HPP_
#define _RIVE_DATA_CONVERTER_OPERATION_HPP_
#include "rive/generated/data_bind/converters/data_converter_operation_base.hpp"
#include "rive/animation/arithmetic_operation.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include <stdio.h>
namespace rive
{
class DataConverterOperation : public DataConverterOperationBase
{
public:
    DataType outputType() override { return DataType::number; };
    ArithmeticOperation op() const
    {
        return (ArithmeticOperation)operationType();
    }

protected:
    DataValue* convertValue(DataValue* input, float value);
    DataValue* reverseConvertValue(DataValue* input, float value);
    DataValueNumber m_output;
};
} // namespace rive

#endif