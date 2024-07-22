#include "rive/artboard.hpp"
#include "rive/importers/bindable_property_importer.hpp"
#include "rive/data_bind/bindable_property.hpp"

using namespace rive;

BindablePropertyImporter::BindablePropertyImporter(BindableProperty* bindableProperty) :
    m_bindableProperty(bindableProperty)
{}