#include "rive/data_bind/converters/data_converter.hpp"
#include "rive/importers/import_stack.hpp"
#include "rive/importers/backboard_importer.hpp"
#include "rive/backboard.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/data_bind/data_bind_context.hpp"

using namespace rive;

DataConverter::~DataConverter() { deleteDataBinds(); }

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

void DataConverter::bindFromContext(DataContext* dataContext,
                                    DataBind* dataBind)
{
    m_parentDataBind = dataBind;
    bindDataBindsFromContext(dataContext);
}

void DataConverter::unbind() { unbindDataBinds(); }

void DataConverter::markConverterDirty()
{
    m_parentDataBind->addDirt(ComponentDirt::Dependents |
                                  ComponentDirt::Bindings,
                              false);
}

void DataConverter::addDirtyDataBind(DataBind* dataBind)
{
    markConverterDirty();
    DataBindContainer::addDirtyDataBind(dataBind);
}

void DataConverter::update() { updateDataBinds(false); }

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