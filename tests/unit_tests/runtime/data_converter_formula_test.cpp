#include <catch.hpp>
#include <chrono>
#include <cstdio>
#include <memory>
#include "rive/animation/arithmetic_operation.hpp"
#include "rive/data_bind/converters/data_converter_formula.hpp"
#include "rive/data_bind/converters/formula/formula_token_argument_separator.hpp"
#include "rive/data_bind/converters/formula/formula_token_function.hpp"
#include "rive/data_bind/converters/formula/formula_token_input.hpp"
#include "rive/data_bind/converters/formula/formula_token_operation.hpp"
#include "rive/data_bind/converters/formula/formula_token_parenthesis_close.hpp"
#include "rive/data_bind/converters/formula/formula_token_parenthesis_open.hpp"
#include "rive/data_bind/converters/formula/formula_token_value.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/function_type.hpp"

using namespace rive;

namespace
{
// Builds a formula from tokens in the order the editor writes them. A
// function token opens its own argument list, closed by a parenthesis.
class FormulaBuilder
{
public:
  FormulaBuilder& value(float v)
  {
    auto token = new FormulaTokenValue();
    token->operationValue(v);
    return add(token);
  }
  FormulaBuilder& input() { return add(new FormulaTokenInput()); }
  FormulaBuilder& op(ArithmeticOperation operation)
  {
    auto token = new FormulaTokenOperation();
    token->operationType((uint32_t)operation);
    return add(token);
  }
  FormulaBuilder& fn(FunctionType type)
  {
    auto token = new FormulaTokenFunction();
    token->functionType((uint32_t)type);
    return add(token);
  }
  FormulaBuilder& open() { return add(new FormulaTokenParenthesisOpen()); }
  FormulaBuilder& close() { return add(new FormulaTokenParenthesisClose()); }
  FormulaBuilder& comma() { return add(new FormulaTokenArgumentSeparator()); }

  // The formula owns its tokens from here on.
  std::unique_ptr<DataConverterFormula> build()
  {
    m_formula->calculateFormula();
    return std::move(m_formula);
  }

private:
  FormulaBuilder& add(FormulaToken* token)
  {
    m_formula->addToken(token);
    return *this;
  }
  std::unique_ptr<DataConverterFormula>
      m_formula = std::make_unique<DataConverterFormula>();
};

float evaluate(DataConverter* formula, float input)
{
  DataValueNumber value(input);
  auto result = formula->convert(&value, nullptr);
  REQUIRE(result->is<DataValueNumber>());
  return result->as<DataValueNumber>()->value();
}

// max(input, 2, 5) * 3 - 1
std::unique_ptr<DataConverterFormula> makeMaxFormula()
{
  return FormulaBuilder()
      .fn(FunctionType::max)
      .input()
      .comma()
      .value(2)
      .comma()
      .value(5)
      .close()
      .op(ArithmeticOperation::multiply)
      .value(3)
      .op(ArithmeticOperation::subtract)
      .value(1)
      .build();
}

// min(max(input, 2), 10) + round(2.6)
std::unique_ptr<DataConverterFormula> makeNestedFormula()
{
  return FormulaBuilder()
      .fn(FunctionType::min)
      .fn(FunctionType::max)
      .input()
      .comma()
      .value(2)
      .close()
      .comma()
      .value(10)
      .close()
      .op(ArithmeticOperation::add)
      .fn(FunctionType::round)
      .value(2.6f)
      .close()
      .build();
}
} // namespace

TEST_CASE("formula applies operator precedence and parentheses", "[formula]")
{
  // 1 + input * 2
  auto plain = FormulaBuilder()
                   .value(1)
                   .op(ArithmeticOperation::add)
                   .input()
                   .op(ArithmeticOperation::multiply)
                   .value(2)
                   .build();
  CHECK(evaluate(plain.get(), 3) == 7);
  CHECK(evaluate(plain.get(), -1) == -1);

  // (1 + input) * 2
  auto grouped = FormulaBuilder()
                     .open()
                     .value(1)
                     .op(ArithmeticOperation::add)
                     .input()
                     .close()
                     .op(ArithmeticOperation::multiply)
                     .value(2)
                     .build();
  CHECK(evaluate(grouped.get(), 3) == 8);
}

TEST_CASE("formula passes every argument to a function in order", "[formula]")
{
  auto max = makeMaxFormula();
  CHECK(evaluate(max.get(), 4) == 14);
  CHECK(evaluate(max.get(), 7) == 20);

  // pow(input, 2): swapped arguments would give 2^input.
  auto pow = FormulaBuilder()
                 .fn(FunctionType::pow)
                 .input()
                 .comma()
                 .value(2)
                 .close()
                 .build();
  CHECK(evaluate(pow.get(), 3) == 9);
}

// Every evaluation starts from an empty stack, and nested calls don't see
// each other's arguments, however often a formula runs and whichever ran
// before it.
TEST_CASE("formula evaluations don't leak into each other", "[formula]")
{
  auto max = makeMaxFormula();
  auto nested = makeNestedFormula();
  for (int i = 0; i < 3; i++)
  {
    CHECK(evaluate(nested.get(), 0) == 5);
    CHECK(evaluate(nested.get(), 50) == 13);
    CHECK(evaluate(nested.get(), 4.5f) == 7.5f);
    CHECK(evaluate(max.get(), 4) == 14);
  }
}

TEST_CASE("an incomplete formula passes its input through", "[formula]")
{
  // input + (the operation has a single operand)
  auto incomplete = FormulaBuilder()
                        .input()
                        .op(ArithmeticOperation::add)
                        .build();
  CHECK(evaluate(incomplete.get(), 6) == 6);
  CHECK(evaluate(incomplete.get(), -2) == -2);
}

// Artboard instances evaluate a clone, which receives the source's resolved
// output queue rather than its tokens.
TEST_CASE("a cloned formula evaluates like its source", "[formula]")
{
  auto nested = makeNestedFormula();
  std::unique_ptr<Core> clone(static_cast<Core*>(nested.get())->clone());
  REQUIRE(clone->is<DataConverterFormula>());
  auto cloned = clone->as<DataConverterFormula>();
  for (float input : {0.0f, 4.5f, 50.0f})
  {
    CHECK(evaluate(cloned, input) == evaluate(nested.get(), input));
  }
}

// What one evaluation costs. Formulas run once per bound value change, and
// files converted from After Effects carry hundreds of them.
TEST_CASE("bench: formula evaluation", "[.][formula_bench]")
{
  auto nested = makeNestedFormula();
  std::unique_ptr<Core> clone(static_cast<Core*>(nested.get())->clone());
  auto formula = clone->as<DataConverterFormula>();
  constexpr int kEvaluations = 2000000;
  DataConverter* converter = formula;
  DataValueNumber value;
  float sum = 0;
  auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < kEvaluations; i++)
  {
    value.value((float)(i % 20));
    sum += converter->convert(&value, nullptr)->as<DataValueNumber>()->value();
  }
  double ns = std::chrono::duration<double, std::nano>(
                  std::chrono::steady_clock::now() - start)
                  .count() /
              kEvaluations;
  printf("formula evaluation: %.1f ns (checksum %.0f)\n", ns, sum);
  CHECK(sum > 0);
}
