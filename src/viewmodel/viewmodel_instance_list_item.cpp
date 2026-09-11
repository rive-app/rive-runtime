#include <sstream>
#include <iomanip>
#include <array>

#include "rive/viewmodel/viewmodel_instance_list.hpp"
#include "rive/viewmodel/viewmodel_instance_list_item.hpp"
#include "rive/viewmodel/viewmodel_instance_symbol_list_index.hpp"
#include "rive/viewmodel/symbol_type.hpp"
#include "rive/importers/viewmodel_instance_list_importer.hpp"

using namespace rive;

void ViewModelInstanceListItem::assignListIndex(uint32_t index)
{
    auto vmi = viewModelInstance();
    if (vmi == nullptr)
    {
        return;
    }
    auto symbol = vmi->propertyValue(SymbolType::itemIndex);
    if (symbol != nullptr)
    {
        symbol->as<ViewModelInstanceSymbolListIndex>()->propertyValue(index);
    }
}

StatusCode ViewModelInstanceListItem::import(ImportStack& importStack)
{
    auto viewModelInstanceList =
        importStack.latest<ViewModelInstanceListImporter>(
            ViewModelInstanceList::typeKey);
    if (viewModelInstanceList == nullptr)
    {
        return StatusCode::MissingObject;
    }
    viewModelInstanceList->addItem(rcp<ViewModelInstanceListItem>(this));

    return Super::import(importStack);
}