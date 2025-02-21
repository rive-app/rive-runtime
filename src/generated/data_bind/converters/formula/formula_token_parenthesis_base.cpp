#include "rive/generated/data_bind/converters/formula/formula_token_parenthesis_base.hpp"
#include "rive/data_bind/converters/formula/formula_token_parenthesis.hpp"

using namespace rive;

Core* FormulaTokenParenthesisBase::clone() const
{
    auto cloned = new FormulaTokenParenthesis();
    cloned->copy(*this);
    return cloned;
}
