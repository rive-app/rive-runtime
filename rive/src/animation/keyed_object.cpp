#include "animation/keyed_object.hpp"
#include "animation/keyed_property.hpp"
#include "artboard.hpp"

using namespace rive;

KeyedObject::~KeyedObject()
{
	for (auto property : m_KeyedProperties)
	{
		delete property;
	}
}
void KeyedObject::addKeyedProperty(KeyedProperty* property)
{
	m_KeyedProperties.push_back(property);
}


void KeyedObject::onAddedDirty(CoreContext* context) 
{
	for (auto property : m_KeyedProperties)
	{
		property->onAddedDirty(context);
	}

}
void KeyedObject::onAddedClean(CoreContext* context) 
{
	for (auto property : m_KeyedProperties)
	{
		property->onAddedClean(context);
	}
}


void KeyedObject::apply(Artboard* artboard, float time, float mix)
{
	Core* object = artboard->resolve(objectId());
	if (object == nullptr)
	{
		return;
	}
	for (auto property : m_KeyedProperties)
	{
		property->apply(object, time, mix);
	}
}