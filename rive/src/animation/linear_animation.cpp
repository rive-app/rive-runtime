#include "animation/linear_animation.hpp"
#include "animation/keyed_object.hpp"

using namespace rive;

LinearAnimation::~LinearAnimation()
{
	for (auto object : m_KeyedObjects)
	{
		delete object;
	}
}

void LinearAnimation::onAddedDirty(CoreContext* context) 
{
	for (auto object : m_KeyedObjects)
	{
		object->onAddedDirty(context);
	}

}
void LinearAnimation::onAddedClean(CoreContext* context) 
{
	for (auto object : m_KeyedObjects)
	{
		object->onAddedClean(context);
	}
}

void LinearAnimation::addKeyedObject(KeyedObject* object)
{
	m_KeyedObjects.push_back(object);
}

void LinearAnimation::apply(Artboard* artboard, float time, float mix)
{
	for (auto object : m_KeyedObjects)
	{
		object->apply(artboard, time, mix);
	}
}
