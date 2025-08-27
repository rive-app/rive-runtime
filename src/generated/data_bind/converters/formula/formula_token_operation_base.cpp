#include "rive/generated/data_bind/converters/formula/formula_token_operation_base.hpp"
#include "rive/data_bind/converters/formula/formula_token_operation.hpp"

using namespace rive;

Core* FormulaTokenOperationBase::clone() const
{
    auto cloned = new FormulaTokenOperation();
    cloned->copy(*this);
    return cloned;
}
