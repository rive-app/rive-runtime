#include "rive/data_bind/converters/formula/formula_token.hpp"
#include "rive/data_bind/converters/data_converter_formula.hpp"
#include "rive/importers/data_converter_formula_importer.hpp"
#include "rive/data_bind/data_bind_context.hpp"
#include <cmath>

using namespace rive;

StatusCode FormulaToken::import(ImportStack& importStack)
{
    auto dataConverterFormulaImporter =
        importStack.latest<DataConverterFormulaImporter>(
            DataConverterFormulaBase::typeKey);
    if (dataConverterFormulaImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }
    dataConverterFormulaImporter->formula()->addToken(this);
    m_formula = dataConverterFormulaImporter->formula();
    return Super::import(importStack);
}

void FormulaToken::addDataBind(DataBind* dataBind)
{
    if (m_formula)
    {
        m_formula->addDataBind(dataBind);
    }
}