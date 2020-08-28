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

StatusCode LinearAnimation::onAddedDirty(CoreContext* context)
{
	StatusCode code;
	for (auto object : m_KeyedObjects)
	{
		if ((code = object->onAddedDirty(context)) != StatusCode::Ok)
		{
			return code;
		}
	}
	return StatusCode::Ok;
}
StatusCode LinearAnimation::onAddedClean(CoreContext* context)
{
	StatusCode code;
	for (auto object : m_KeyedObjects)
	{
		if ((code = object->onAddedClean(context)) != StatusCode::Ok)
		{
			return code;
		}
	}
	return StatusCode::Ok;
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