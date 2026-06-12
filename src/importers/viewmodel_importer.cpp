#include "rive/importers/viewmodel_importer.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/viewmodel/viewmodel_property.hpp"

using namespace rive;

ViewModelImporter::ViewModelImporter(ViewModel* viewModel) :
    m_ViewModel(viewModel)
{}
void ViewModelImporter::addProperty(ViewModelProperty* property)
{
    m_ViewModel->addProperty(property);
}

StatusCode ViewModelImporter::resolve() { return StatusCode::Ok; }
