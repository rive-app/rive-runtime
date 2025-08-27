#include "rive/generated/data_bind/converters/formula/formula_token_input_base.hpp"
#include "rive/data_bind/converters/formula/formula_token_input.hpp"

using namespace rive;

Core* FormulaTokenInputBase::clone() const
{
    auto cloned = new FormulaTokenInput();
    cloned->copy(*this);
    return cloned;
}
