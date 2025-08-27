#include "rive/generated/data_bind/converters/formula/formula_token_function_base.hpp"
#include "rive/data_bind/converters/formula/formula_token_function.hpp"

using namespace rive;

Core* FormulaTokenFunctionBase::clone() const
{
    auto cloned = new FormulaTokenFunction();
    cloned->copy(*this);
    return cloned;
}
