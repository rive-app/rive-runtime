#include "rive/data_bind/converters/formula/formula_token.hpp"
#include "rive/data_bind/converters/data_converter_formula.hpp"
#include "rive/importers/data_converter_formula_importer.hpp"
#include "rive/data_bind/data_bind_context.hpp"
#include <cmath>

using namespace rive;

FormulaToken::~FormulaToken()
{
    for (auto dataBind : m_dataBinds)
    {
        delete dataBind;
    }
}

StatusCode FormulaToken::import(ImportStack& importStack)
{
    auto dataConveterFormulaImporter =
        importStack.latest<DataConverterFormulaImporter>(
            DataConverterFormulaBase::typeKey);
    if (dataConveterFormulaImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }
    dataConveterFormulaImporter->formula()->addToken(this);
    return Super::import(importStack);
}

void FormulaToken::addDataBind(DataBind* dataBind)
{
    m_dataBinds.push_back(dataBind);
}

void FormulaToken::markDirty()
{
    m_parentDataBind->addDirt(ComponentDirt::Dependents |
                                  ComponentDirt::Bindings,
                              false);
}

void FormulaToken::bindFromContext(DataContext* dataContext, DataBind* dataBind)
{
    m_parentDataBind = dataBind;
    for (auto dataBind : m_dataBinds)
    {
        if (dataBind->is<DataBindContext>())
        {

            dataBind->as<DataBindContext>()->bindFromContext(dataContext);
        }
    }
}

void FormulaToken::unbind()
{
    for (auto dataBind : m_dataBinds)
    {
        dataBind->unbind();
    }
}

void FormulaToken::update()
{
    for (auto dataBind : m_dataBinds)
    {
        auto d = dataBind->dirt();
        if (d == ComponentDirt::None)
        {
            continue;
        }
        dataBind->dirt(ComponentDirt::None);
        dataBind->update(d);
    }
}

void FormulaToken::copy(const FormulaTokenBase& object)
{
    auto dataBinds = object.as<FormulaToken>()->dataBinds();
    for (auto dataBind : dataBinds)
    {
        auto dataBindClone = dataBind->clone()->as<DataBind>();
        dataBindClone->target(this);
        addDataBind(dataBindClone);
    }
    FormulaTokenBase::copy(object);
}