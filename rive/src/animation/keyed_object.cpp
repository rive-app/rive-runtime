#include "animation/keyed_object.hpp"
#include "animation/keyed_property.hpp"

using namespace rive;

KeyedObject::~KeyedObject()
{
	for (auto property : m_KeyedProperties)
	{
		delete property;
	}
}
void KeyedObject::addKeyedProperty(KeyedProperty* property) { m_KeyedProperties.push_back(property); }