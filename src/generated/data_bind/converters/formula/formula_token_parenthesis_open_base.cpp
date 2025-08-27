#include "rive/generated/data_bind/converters/formula/formula_token_parenthesis_open_base.hpp"
#include "rive/data_bind/converters/formula/formula_token_parenthesis_open.hpp"

using namespace rive;

Core* FormulaTokenParenthesisOpenBase::clone() const
{
    auto cloned = new FormulaTokenParenthesisOpen();
    cloned->copy(*this);
    return cloned;
}
