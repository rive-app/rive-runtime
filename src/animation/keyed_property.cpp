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

void KeyedProperty::addKeyFrame(KeyFrame* keyframe)
{
	m_KeyFrames.push_back(keyframe);
}

void KeyedProperty::apply(Core* object, float seconds, float mix)
{
	assert(!m_KeyFrames.empty());

	int idx = 0;
	int mid = 0;
	float closestSeconds = 0.0f;
	int start = 0;
	auto numKeyFrames = m_KeyFrames.size();
	int end = (int)numKeyFrames - 1;
	while (start <= end)
	{
		mid = (start + end) >> 1;
		closestSeconds = m_KeyFrames[mid]->seconds();
		if (closestSeconds < seconds)
		{
			start = mid + 1;
		}
		else if (closestSeconds > seconds)
		{
			end = mid - 1;
		}
		else
		{
			idx = start = mid;
			break;
		}
		idx = start;
	}
	int pk = propertyKey();
	if (idx == 0)
	{
		m_KeyFrames[0]->apply(object, pk, mix);
	}
	else
	{
		if (idx < numKeyFrames)
		{
			KeyFrame* fromFrame = m_KeyFrames[idx - 1];
			KeyFrame* toFrame = m_KeyFrames[idx];
			if (seconds == toFrame->seconds())
			{
				toFrame->apply(object, pk, mix);
			}
			else
			{
				if (fromFrame->interpolationType() == 0)
				{
					fromFrame->apply(object, pk, mix);
				}
				else
				{
					fromFrame->applyInterpolation(
					    object, pk, seconds, toFrame, mix);
				}
			}
		}
		else
		{
			m_KeyFrames[idx - 1]->apply(object, pk, mix);
		}
	}
}

StatusCode KeyedProperty::onAddedDirty(CoreContext* context)
{
	StatusCode code;
	for (auto keyframe : m_KeyFrames)
	{
		if ((code = keyframe->onAddedDirty(context)) != StatusCode::Ok)
		{
			return code;
		}
	}
	return StatusCode::Ok;
}

StatusCode KeyedProperty::onAddedClean(CoreContext* context)
{
	StatusCode code;
	for (auto keyframe : m_KeyFrames)
	{
		if ((code = keyframe->onAddedClean(context)) != StatusCode::Ok)
		{
			return code;
		}
	}
	return StatusCode::Ok;
}