#include "rive/generated/data_bind/converters/formula/formula_token_parenthesis_close_base.hpp"
#include "rive/data_bind/converters/formula/formula_token_parenthesis_close.hpp"

using namespace rive;

Core* FormulaTokenParenthesisCloseBase::clone() const
{
    auto cloned = new FormulaTokenParenthesisClose();
    cloned->copy(*this);
    return cloned;
}
