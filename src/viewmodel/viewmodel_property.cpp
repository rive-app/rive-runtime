#include "rive/viewmodel/viewmodel_property.hpp"
#include "rive/importers/viewmodel_importer.hpp"
#include "rive/viewmodel/viewmodel.hpp"

using namespace rive;

StatusCode ViewModelProperty::import(ImportStack& importStack)
{
    auto viewModelImporter = importStack.latest<ViewModelImporter>(ViewModel::typeKey);
    if (viewModelImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }

    viewModelImporter->addProperty(this);
    return Super::import(importStack);
}
