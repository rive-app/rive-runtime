#ifndef _RIVE_FORMULA_TOKEN_HPP_
#define _RIVE_FORMULA_TOKEN_HPP_
#include "rive/generated/data_bind/converters/formula/formula_token_base.hpp"
#include "rive/data_bind/data_values/data_value.hpp"
#include "rive/data_bind/data_context.hpp"
#include "rive/data_bind/data_bind.hpp"
#include <stdio.h>
namespace rive
{
class DataConverterFormula;
class FormulaToken : public FormulaTokenBase
{
public:
    StatusCode import(ImportStack& importStack) override;
    void addDataBind(DataBind* dataBind);

private:
    DataConverterFormula* m_formula = nullptr;
};
} // namespace rive

#endif