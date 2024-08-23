#include "rive/importers/viewmodel_instance_list_importer.hpp"
#include "rive/viewmodel/viewmodel_instance_list.hpp"
#include "rive/viewmodel/viewmodel_instance_list_item.hpp"

using namespace rive;

ViewModelInstanceListImporter::ViewModelInstanceListImporter(
    ViewModelInstanceList* viewModelInstanceList) :
    m_ViewModelInstanceList(viewModelInstanceList)
{}
void ViewModelInstanceListImporter::addItem(ViewModelInstanceListItem* listItem)
{
    m_ViewModelInstanceList->addItem(listItem);
}

StatusCode ViewModelInstanceListImporter::resolve() { return StatusCode::Ok; }