#include <sstream>
#include <iomanip>
#include <array>

#include "rive/viewmodel/viewmodel_instance_list.hpp"
#include "rive/viewmodel/viewmodel_instance_list_item.hpp"
#include "rive/importers/viewmodel_instance_list_importer.hpp"

using namespace rive;

StatusCode ViewModelInstanceListItem::import(ImportStack& importStack)
{
    auto viewModelInstanceList =
        importStack.latest<ViewModelInstanceListImporter>(ViewModelInstanceList::typeKey);
    if (viewModelInstanceList == nullptr)
    {
        return StatusCode::MissingObject;
    }
    viewModelInstanceList->addItem(this);

    return Super::import(importStack);
}