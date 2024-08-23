#include "rive/importers/keyed_object_importer.hpp"
#include "rive/animation/keyed_object.hpp"
#include "rive/animation/keyed_property.hpp"
#include "rive/artboard.hpp"

using namespace rive;

KeyedObjectImporter::KeyedObjectImporter(KeyedObject* keyedObject) : m_KeyedObject(keyedObject) {}

void KeyedObjectImporter::addKeyedProperty(std::unique_ptr<KeyedProperty> property)
{
    m_KeyedObject->addKeyedProperty(std::move(property));
}