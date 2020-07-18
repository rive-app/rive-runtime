#include "animation/keyed_property.hpp"
#include "animation/keyframe.hpp"

using namespace rive;

KeyedProperty::~KeyedProperty()
{
	for (auto keyframe : m_KeyFrames)
	{
		delete keyframe;
	}
}

void KeyedProperty::addKeyFrame(KeyFrame* keyframe) { m_KeyFrames.push_back(keyframe); }