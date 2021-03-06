#include "importers/keyed_object_importer.hpp"
#include "animation/keyed_object.hpp"
#include "animation/keyed_property.hpp"
#include "artboard.hpp"

using namespace rive;

KeyedObjectImporter::KeyedObjectImporter(KeyedObject* keyedObject) :
    m_KeyedObject(keyedObject)
{
}

void KeyedObjectImporter::addKeyedProperty(KeyedProperty* property)
{
	m_KeyedObject->addKeyedProperty(property);
}