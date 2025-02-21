#include "rive/generated/data_bind/converters/formula/formula_token_argument_separator_base.hpp"
#include "rive/data_bind/converters/formula/formula_token_argument_separator.hpp"

using namespace rive;

Core* FormulaTokenArgumentSeparatorBase::clone() const
{
    auto cloned = new FormulaTokenArgumentSeparator();
    cloned->copy(*this);
    return cloned;
}
