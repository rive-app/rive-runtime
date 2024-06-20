#ifndef _RIVE_VIEWMODEL_INSTANCE_LIST_IMPORTER_HPP_
#define _RIVE_VIEWMODEL_INSTANCE_LIST_IMPORTER_HPP_

#include "rive/importers/import_stack.hpp"

namespace rive
{
class Core;
class ViewModelInstanceList;
class ViewModelInstanceListItem;
class ViewModelInstanceListImporter : public ImportStackObject
{
private:
    ViewModelInstanceList* m_ViewModelInstanceList;

public:
    ViewModelInstanceListImporter(ViewModelInstanceList* viewModelInstanceList);
    void addItem(ViewModelInstanceListItem* listItem);
    StatusCode resolve() override;
};
} // namespace rive
#endif
