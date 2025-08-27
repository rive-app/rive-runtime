#ifndef _RIVE_VIEWMODEL_IMPORTER_HPP_
#define _RIVE_VIEWMODEL_IMPORTER_HPP_

#include "rive/importers/import_stack.hpp"

namespace rive
{
class ViewModel;
class ViewModelProperty;
class ViewModelInstance;

class ViewModelImporter : public ImportStackObject
{
private:
    ViewModel* m_ViewModel;

public:
    ViewModelImporter(ViewModel* viewModel);
    void addProperty(ViewModelProperty* property);
    void addInstance(ViewModelInstance* value);
    StatusCode resolve() override;
};
} // namespace rive
#endif
