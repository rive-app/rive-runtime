#include "rive/generated/data_bind/converters/formula/formula_token_value_base.hpp"
#include "rive/data_bind/converters/formula/formula_token_value.hpp"

using namespace rive;

Core* FormulaTokenValueBase::clone() const
{
    auto cloned = new FormulaTokenValue();
    cloned->copy(*this);
    return cloned;
}
