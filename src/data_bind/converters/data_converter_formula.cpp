#include "rive/data_bind/converters/data_converter_formula.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/data_bind/data_values/data_value_symbol_list_index.hpp"
#include "rive/data_bind/converters/formula/formula_token_value.hpp"
#include "rive/data_bind/converters/formula/formula_token_argument_separator.hpp"
#include "rive/data_bind/converters/formula/formula_token_input.hpp"
#include "rive/data_bind/converters/formula/formula_token_operation.hpp"
#include "rive/data_bind/converters/formula/formula_token_parenthesis.hpp"
#include "rive/data_bind/converters/formula/formula_token_function.hpp"
#include "rive/data_bind/converters/formula/formula_token_parenthesis_open.hpp"
#include "rive/data_bind/converters/formula/formula_token_parenthesis_close.hpp"
#include "rive/animation/arithmetic_operation.hpp"
#include "rive/random_mode.hpp"
#include "rive/function_type.hpp"
#include "rive/math/math_types.hpp"
#include "rive/math/random.hpp"
#include <cmath>

using namespace rive;

DataConverterFormula::~DataConverterFormula()
{
    unbind();
    if (m_isInstance)
    {

        for (auto& token : m_outputQueue)
        {
            delete token;
        }
    }
    else
    {

        for (auto& token : m_tokens)
        {
            delete token;
        }
    }
    m_outputQueue.clear();
    m_tokens.clear();
}

int DataConverterFormula::getPrecedence(FormulaToken* token)
{
    if (token->is<FormulaTokenParenthesis>())
    {
        return 1;
    }
    if (token->is<FormulaTokenFunction>())
    {
        return 1;
    }
    if (token->is<FormulaTokenOperation>())
    {
        auto operationToken = token->as<FormulaTokenOperation>();
        auto operationType =
            (ArithmeticOperation)operationToken->operationType();
        if (operationType == ArithmeticOperation::add)
        {
            return 2;
        }
        else if (operationType == ArithmeticOperation::subtract)
        {
            return 2;
        }
        else if (operationType == ArithmeticOperation::multiply)
        {
            return 3;
        }
        else if (operationType == ArithmeticOperation::divide)
        {
            return 3;
        }
    }
    return 0;
}

void DataConverterFormula::calculateFormula()
{
    // convert the formula to Reverse Polish Notation using a version of
    // the Shunting yard algorithm
    // Some optimizations could be done here by precomputing constant parts
    // of the equation
    std::vector<FormulaToken*> operationsStack;
    int tokenIndex = 0;
    for (auto& token : m_tokens)
    {
        if (token->is<FormulaTokenValue>() || token->is<FormulaTokenInput>())
        {
            m_outputQueue.push_back(token);
        }
        else if (token->is<FormulaTokenOperation>())
        {
            while (operationsStack.size() > 0 &&
                   !operationsStack.back()->is<FormulaTokenParenthesisOpen>() &&
                   getPrecedence(operationsStack.back()) >=
                       getPrecedence(token))
            {
                FormulaToken* higherToken = operationsStack.back();
                operationsStack.pop_back();
                m_outputQueue.push_back(higherToken);
            }
            operationsStack.push_back(token);
        }
        else if (token->is<FormulaTokenParenthesisOpen>() ||
                 token->is<FormulaTokenFunction>())
        {

            // Peeking into the next token to identify whether the current
            // function has no arguments. Is there a better way to reliably
            // count the number of arguments?
            FormulaToken* nextToken = tokenIndex == m_tokens.size() - 1
                                          ? nullptr
                                          : m_tokens[tokenIndex + 1];
            m_argumentsCount[token] =
                (nextToken != nullptr &&
                 nextToken->is<FormulaTokenParenthesisClose>())
                    ? 0
                    : 1;
            operationsStack.push_back(token);
        }
        else if (token->is<FormulaTokenParenthesisClose>())
        {

            while (
                operationsStack.size() > 0 &&
                (!operationsStack.back()->is<FormulaTokenParenthesisOpen>() &&
                 !operationsStack.back()->is<FormulaTokenFunction>()))
            {
                auto higherToken = operationsStack.back();
                operationsStack.pop_back();
                m_outputQueue.push_back(higherToken);
            }
            if (operationsStack.size() == 0)
            {
                // mismatched parenthesis
            }
            else
            {
                auto openingToken = operationsStack.back();
                operationsStack.pop_back();
                if (openingToken->is<FormulaTokenFunction>())
                {
                    m_outputQueue.push_back(openingToken);
                }
                else
                {
                    // Discarding open parenthesis
                }
            }
        }
        else if (token->is<FormulaTokenArgumentSeparator>() &&
                 operationsStack.size() > 0)
        {
            size_t index = operationsStack.size() - 1;
            while (index >= 0)
            {
                auto operationTokenCandidate = operationsStack[index];
                auto argumentsCount =
                    m_argumentsCount.find(operationTokenCandidate);
                if (argumentsCount != m_argumentsCount.end())
                {

                    int count = argumentsCount->second;
                    m_argumentsCount[operationTokenCandidate] = count + 1;
                    break;
                }
                if (index == 0)
                {
                    break;
                }
                index -= 1;
            }
            while (
                operationsStack.size() > 0 &&
                (!operationsStack.back()->is<FormulaTokenParenthesisOpen>() &&
                 !operationsStack.back()->is<FormulaTokenFunction>()))
            {
                auto higherToken = operationsStack.back();
                operationsStack.pop_back();
                m_outputQueue.push_back(higherToken);
            }
        }
        tokenIndex++;
    }
    while (operationsStack.size() > 0)
    {
        auto operation = operationsStack.back();
        operationsStack.pop_back();
        if (operation->is<FormulaTokenParenthesisOpen>())
        {
            // mismatched parenthesis
        }
        else
        {
            m_outputQueue.push_back(operation);
        }
    }
}

float DataConverterFormula::applyOperation(float left,
                                           float right,
                                           int operationTypeInt)
{
    auto operationType = (ArithmeticOperation)operationTypeInt;
    switch (operationType)
    {
        case ArithmeticOperation::add:
            return left + right;
        case ArithmeticOperation::subtract:
            return left - right;
        case ArithmeticOperation::multiply:
            return left * right;
        case ArithmeticOperation::divide:
            return left / right;
        case ArithmeticOperation::modulo:
            return math::positive_mod(left, right);
        default:
            return 0.0f;
    }
}

float DataConverterFormula::getRandom(int randomIndex)
{
    if (randomModeValue() == static_cast<uint32_t>(RandomMode::always))
    {
        return RandomProvider::generateRandomFloat();
    }

    while (m_randoms.size() <= randomIndex)
    {
        m_randoms.push_back(RandomProvider::generateRandomFloat());
    }
    return m_randoms[randomIndex];
}

float DataConverterFormula::applyFunction(std::vector<float>& stack,
                                          int functionTypeIndex,
                                          int totalArguments)
{

    std::vector<float> functionArguments;
    int index = 0;
    int currentRandom = 0;
    while (index < totalArguments)
    {
        if (stack.size() > 0)
        {
            auto functionArgument = stack.back();
            stack.pop_back();
            functionArguments.push_back(functionArgument);
        }
        index++;
    }
    auto functionType = (FunctionType)functionTypeIndex;
    switch (functionType)
    {
        case FunctionType::min:
        {
            if (functionArguments.size() > 0)
            {
                float minValue = functionArguments[0];
                for (auto i = 1; i < functionArguments.size(); i++)
                {
                    if (functionArguments[i] < minValue)
                    {
                        minValue = functionArguments[i];
                    }
                }
                return minValue;
            }
        }
        break;
        case FunctionType::max:
        {
            if (functionArguments.size() > 0)
            {
                float maxValue = functionArguments[0];
                for (auto i = 1; i < functionArguments.size(); i++)
                {
                    if (functionArguments[i] > maxValue)
                    {
                        maxValue = functionArguments[i];
                    }
                }
                return maxValue;
            }
        }
        break;
        case FunctionType::round:
            if (functionArguments.size() > 0)
            {
                return roundf(functionArguments.back());
            }
            break;
        case FunctionType::ceil:
            if (functionArguments.size() > 0)
            {
                return ceilf(functionArguments.back());
            }
            break;
        case FunctionType::floor:
            if (functionArguments.size() > 0)
            {
                return floorf(functionArguments.back());
            }
            break;
        case FunctionType::sqrt:
            if (functionArguments.size() > 0)
            {
                return sqrtf(functionArguments.back());
            }
            break;
        case FunctionType::pow:
        {
            if (functionArguments.size() > 1)
            {
                auto exponent = functionArguments[functionArguments.size() - 2];
                auto x = functionArguments.back();
                return powf(x, exponent);
            }
        }
        break;
        case FunctionType::exp:
            if (functionArguments.size() > 0)
            {
                return exp(functionArguments.back());
            }
            break;
        case FunctionType::log:
            if (functionArguments.size() > 0)
            {
                return log(functionArguments.back());
            }
            break;
        case FunctionType::cosine:
            if (functionArguments.size() > 0)
            {
                return cos(functionArguments.back());
            }
            break;
        case FunctionType::sine:
            if (functionArguments.size() > 0)
            {
                return sin(functionArguments.back());
            }
            break;
        case FunctionType::tangent:
            if (functionArguments.size() > 0)
            {
                return tan(functionArguments.back());
            }
            break;
        case FunctionType::acosine:

            if (functionArguments.size() > 0)
            {
                return acos(functionArguments.back());
            }
            break;
        case FunctionType::asine:

            if (functionArguments.size() > 0)
            {
                return asin(functionArguments.back());
            }
            break;
        case FunctionType::atangent:

            if (functionArguments.size() > 0)
            {
                return atan(functionArguments.back());
            }
            break;
        case FunctionType::atangent2:
        {
            if (functionArguments.size() > 1)
            {
                auto argument1 = functionArguments.back();
                auto argument2 =
                    functionArguments[functionArguments.size() - 2];
                return atan2(argument1, argument2);
            }
        }
        break;
        case FunctionType::random:
        {
            float randomValue = getRandom(currentRandom++);
            float lowerBound = 0;
            float upperBound = 1;
            if (functionArguments.size() == 1)
            {
                upperBound = functionArguments.back();
            }
            else if (functionArguments.size() > 1)
            {
                lowerBound = functionArguments.back();
                upperBound = functionArguments[functionArguments.size() - 2];
            }
            return lowerBound + (upperBound - lowerBound) * randomValue;
        }
        break;
        default:
            return 0.0f;
    }
    return 0;
}

DataValue* DataConverterFormula::convert(DataValue* value, DataBind* dataBind)
{
    if (value->is<DataValueNumber>() || value->is<DataValueSymbolListIndex>())
    {
        float inputValue =
            value->is<DataValueNumber>()
                ? value->as<DataValueNumber>()->value()
                : (float)(value->as<DataValueSymbolListIndex>()->value());
        float resultValue = inputValue;

        std::vector<float> stack;
        for (auto& token : m_outputQueue)
        {
            if (token->is<FormulaTokenOperation>())
            {
                if (stack.size() > 1)
                {
                    auto right = stack.back();
                    stack.pop_back();
                    auto left = stack.back();
                    stack.pop_back();
                    float operationResult = applyOperation(
                        left,
                        right,
                        token->as<FormulaTokenOperation>()->operationType());
                    stack.push_back(operationResult);
                }
            }
            else if (token->is<FormulaTokenFunction>())
            {
                auto argumentsCount = m_argumentsCount.find(token);
                float operationResult = applyFunction(
                    stack,
                    token->as<FormulaTokenFunction>()->functionType(),
                    argumentsCount == m_argumentsCount.end()
                        ? 0
                        : argumentsCount->second);
                stack.push_back(operationResult);
            }
            else if (token->is<FormulaTokenInput>())
            {
                stack.push_back(inputValue);
            }
            else if (token->is<FormulaTokenValue>())
            {
                stack.push_back(
                    token->as<FormulaTokenValue>()->operationValue());
            }
        }

        // If the formula is well formed, the stack at the end has to be of size
        // 1
        if (stack.size() == 1)
        {
            resultValue = stack.back();
            stack.pop_back();
        }

        m_output.value(resultValue);
    }
    else
    {
        m_output.value(DataValueNumber::defaultValue);
    }
    return &m_output;
}

DataValue* DataConverterFormula::reverseConvert(DataValue* value,
                                                DataBind* dataBind)
{
    return convert(value, dataBind);
}

void DataConverterFormula::addToken(FormulaToken* token)
{
    m_tokens.push_back(token);
}

void DataConverterFormula::addOutputToken(FormulaToken* token,
                                          int argumentsCount)
{
    m_outputQueue.push_back(token);
    m_argumentsCount[token] = argumentsCount;
}

// Warning! this clone override is not making a clean copy of the core object.
// It creates a limited instance to avoid recalculating the output stack every
// time
Core* DataConverterFormula::clone() const
{
    auto cloned = DataConverterFormulaBase::clone()->as<DataConverterFormula>();
    // Instead of cloning all tokens, we can clone the processed tokens that
    // will be used during conversion
    for (auto& token : m_outputQueue)
    {
        auto tokenArgumentsCountSearch = m_argumentsCount.find(token);
        auto argumentsCount =
            tokenArgumentsCountSearch == m_argumentsCount.end()
                ? 0
                : tokenArgumentsCountSearch->second;
        auto clonedToken = token->clone()->as<FormulaToken>();
        cloned->addOutputToken(clonedToken, argumentsCount);
        int index = 0;
        for (auto& dataBind : dataBinds())
        {
            if (dataBind->target() == token)
            {
                cloned->dataBinds()[index]->target(clonedToken);
            }
            index++;
        }
    }
    cloned->isInstance(true);
    return cloned;
}

void DataConverterFormula::bindFromContext(DataContext* dataContext,
                                           DataBind* dataBind)
{
    DataConverter::bindFromContext(dataContext, dataBind);
    if (dataBind && dataBind->source())
    {
        m_source = ref_rcp<ViewModelInstanceValue>(dataBind->source());
        m_source->addDependent(this);
    }
}

void DataConverterFormula::addDirt(ComponentDirt value, bool recurse)
{
    if (randomModeValue() == static_cast<uint32_t>(RandomMode::sourceChange))
    {
        m_randoms.clear();
    }
}

void DataConverterFormula::unbind()
{
    if (m_source)
    {
        m_source->removeDependent(this);
        m_source = nullptr;
    }
}
