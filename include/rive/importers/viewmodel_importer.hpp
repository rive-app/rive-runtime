#ifndef _RIVE_VIEWMODEL_IMPORTER_HPP_
#define _RIVE_VIEWMODEL_IMPORTER_HPP_

#include "rive/importers/import_stack.hpp"

namespace rive
{
class ViewModel;
class ViewModelProperty;
class ViewModelInstance;
class File;

class ViewModelImporter : public ImportStackObject
{
private:
    ViewModel* m_ViewModel;
    File* m_File = nullptr;

public:
    ViewModelImporter(ViewModel* viewModel);
    void file(File* file) { m_File = file; }
    File* file() const { return m_File; }
    void addProperty(ViewModelProperty* property);
    void addInstance(ViewModelInstance* value);
    StatusCode resolve() override;
};
} // namespace rive
#endif
