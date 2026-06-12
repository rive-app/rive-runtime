#ifndef _RIVE_VIEWMODEL_IMPORTER_HPP_
#define _RIVE_VIEWMODEL_IMPORTER_HPP_

#include "rive/importers/import_stack.hpp"

namespace rive
{
class ViewModel;
class ViewModelProperty;

class ViewModelImporter : public ImportStackObject
{
private:
    ViewModel* m_ViewModel;

public:
    ViewModelImporter(ViewModel* viewModel);
    void addProperty(ViewModelProperty* property);
    StatusCode resolve() override;
};
} // namespace rive
#endif
