#ifndef _RIVE_BINDABLE_PROPERTY_IMPORTER_HPP_
#define _RIVE_BINDABLE_PROPERTY_IMPORTER_HPP_

#include "rive/importers/import_stack.hpp"

namespace rive
{
class Core;
class BindableProperty;
class BindablePropertyImporter : public ImportStackObject
{
private:
    BindableProperty* m_bindableProperty;

public:
    BindablePropertyImporter(BindableProperty* bindableProperty);
    BindableProperty* bindableProperty() { return m_bindableProperty; }
};
} // namespace rive
#endif