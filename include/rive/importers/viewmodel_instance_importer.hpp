#ifndef _RIVE_VIEWMODEL_INSTANCE_IMPORTER_HPP_
#define _RIVE_VIEWMODEL_INSTANCE_IMPORTER_HPP_

#include "rive/importers/import_stack.hpp"

namespace rive
{
class ViewModelInstance;
class ViewModelInstanceValue;

class ViewModelInstanceImporter : public ImportStackObject
{
private:
    ViewModelInstance* m_ViewModelInstance;

public:
    ViewModelInstanceImporter(ViewModelInstance* viewModelInstance);
    void addValue(ViewModelInstanceValue* value);
    StatusCode resolve() override;
};
} // namespace rive
#endif
