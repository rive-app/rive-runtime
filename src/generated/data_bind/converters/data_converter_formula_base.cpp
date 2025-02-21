#include "rive/generated/data_bind/converters/data_converter_formula_base.hpp"
#include "rive/data_bind/converters/data_converter_formula.hpp"

using namespace rive;

Core* DataConverterFormulaBase::clone() const
{
    auto cloned = new DataConverterFormula();
    cloned->copy(*this);
    return cloned;
}
