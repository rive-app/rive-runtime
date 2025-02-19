#include "rive/data_bind/converters/data_converter.hpp"
#include "rive/importers/import_stack.hpp"
#include "rive/importers/backboard_importer.hpp"
#include "rive/backboard.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/data_bind/data_bind_context.hpp"

using namespace rive;

DataConverter::~DataConverter()
{
    for (auto dataBind : m_dataBinds)
    {
        delete dataBind;
    }
}

StatusCode DataConverter::import(ImportStack& importStack)
{
    auto backboardImporter =
        importStack.latest<BackboardImporter>(Backboard::typeKey);
    if (backboardImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }
    backboardImporter->addDataConverter(this);

    return Super::import(importStack);
}

void DataConverter::addDataBind(DataBind* dataBind)
{
    m_dataBinds.push_back(dataBind);
}

void DataConverter::bindFromContext(DataContext* dataContext,
                                    DataBind* dataBind)
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

void DataConverter::markConverterDirty()
{
    m_parentDataBind->addDirt(ComponentDirt::Dependents |
                                  ComponentDirt::Bindings,
                              false);
}

void DataConverter::update()
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

void DataConverter::copy(const DataConverter& object)
{
    auto dataBinds = object.dataBinds();
    for (auto dataBind : dataBinds)
    {
        auto dataBindClone = dataBind->clone()->as<DataBind>();
        dataBindClone->target(this);
        addDataBind(dataBindClone);
    }
    DataConverterBase::copy(object);
}

bool DataConverter::advance(float elapsedTime) { return false; }