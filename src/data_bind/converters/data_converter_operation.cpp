#include "rive/math/math_types.hpp"
#include "rive/data_bind/converters/data_converter_operation.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"

using namespace rive;

DataValue* DataConverterOperation::convert(DataValue* input)
{
    auto output = new DataValueNumber();
    if (input->is<DataValueNumber>())
    {
        float inputValue = input->as<DataValueNumber>()->value();
        float resultValue = value();
        switch (op())
        {
            case ArithmeticOperation::add:
                resultValue = inputValue + resultValue;
                break;
            case ArithmeticOperation::subtract:
                resultValue = inputValue - resultValue;
                break;
            case ArithmeticOperation::multiply:
                resultValue = inputValue * resultValue;
                break;
            case ArithmeticOperation::divide:
                resultValue = inputValue / resultValue;
                break;
            case ArithmeticOperation::modulo:
                resultValue = fmodf(inputValue, resultValue);
                break;
        }
        output->value(resultValue);
    }
    return output;
}

DataValue* DataConverterOperation::reverseConvert(DataValue* input)
{
    auto output = new DataValueNumber();
    if (input->is<DataValueNumber>())
    {
        float inputValue = input->as<DataValueNumber>()->value();
        float resultValue = value();
        switch (op())
        {
            case ArithmeticOperation::add:
                resultValue = inputValue - resultValue;
                break;
            case ArithmeticOperation::subtract:
                resultValue = inputValue + resultValue;
                break;
            case ArithmeticOperation::multiply:
                resultValue = inputValue / resultValue;
                break;
            case ArithmeticOperation::divide:
                resultValue = inputValue * resultValue;
                break;
                // No reverse operation for modulo
            case ArithmeticOperation::modulo:
                break;
        }
        output->value(resultValue);
    }
    return output;
}