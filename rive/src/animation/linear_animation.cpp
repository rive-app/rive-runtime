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

void LinearAnimation::addKeyedObject(KeyedObject* object) { m_KeyedObjects.push_back(object); }