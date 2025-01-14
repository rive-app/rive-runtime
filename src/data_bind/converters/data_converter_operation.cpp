#include "rive/math/math_types.hpp"
#include "rive/data_bind/converters/data_converter_operation.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include <cmath>

using namespace rive;

DataValue* DataConverterOperation::convertValue(DataValue* input, float value)
{
    if (input->is<DataValueNumber>())
    {
        float inputValue = input->as<DataValueNumber>()->value();
        float resultValue = value;
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
            case ArithmeticOperation::squareRoot:
                resultValue = sqrtf(inputValue);
                break;
            case ArithmeticOperation::power:
                resultValue = powf(inputValue, resultValue);
                break;
            case ArithmeticOperation::exp:
                resultValue = exp(inputValue);
                break;
            case ArithmeticOperation::log:
                resultValue = log(inputValue);
                break;
            case ArithmeticOperation::cosine:
                resultValue = cos(inputValue);
                break;
            case ArithmeticOperation::sine:
                resultValue = sin(inputValue);
                break;
            case ArithmeticOperation::tangent:
                resultValue = tan(inputValue);
                break;
            case ArithmeticOperation::acosine:
                resultValue = acos(inputValue);
                break;
            case ArithmeticOperation::asine:
                resultValue = asin(inputValue);
                break;
            case ArithmeticOperation::atangent:
                resultValue = atan(inputValue);
                break;
        }
        m_output.value(resultValue);
    }
    else
    {
        m_output.value(DataValueNumber::defaultValue);
    }
    return &m_output;
}

DataValue* DataConverterOperation::reverseConvertValue(DataValue* input,
                                                       float value)
{
    auto output = new DataValueNumber();
    if (input->is<DataValueNumber>())
    {
        float inputValue = input->as<DataValueNumber>()->value();
        float resultValue = value;
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
            case ArithmeticOperation::squareRoot:
                resultValue = powf(inputValue, 2);
                break;
            case ArithmeticOperation::power:
                resultValue = powf(inputValue, 1 / resultValue);
                break;
            case ArithmeticOperation::exp:
                resultValue = log(inputValue);
                break;
            case ArithmeticOperation::log:
                resultValue = exp(inputValue);
                break;
            case ArithmeticOperation::cosine:
                resultValue = acos(inputValue);
                break;
            case ArithmeticOperation::sine:
                resultValue = asin(inputValue);
                break;
            case ArithmeticOperation::tangent:
                resultValue = atan(inputValue);
                break;
            case ArithmeticOperation::acosine:
                resultValue = cos(inputValue);
                break;
            case ArithmeticOperation::asine:
                resultValue = sin(inputValue);
                break;
            case ArithmeticOperation::atangent:
                resultValue = tan(inputValue);
                break;
        }
        output->value(resultValue);
    }
    return output;
}