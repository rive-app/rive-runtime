#include "animation/keyframe.hpp"
#include "animation/cubic_interpolator.hpp"
#include "core_context.hpp"

using namespace rive;

void KeyFrame::onAddedDirty(CoreContext* context)
{
	if (interpolatorId() != 0)
	{
		auto coreObject = context->resolve(interpolatorId());
		if (coreObject == nullptr)
		{
			return;
		}

		if (coreObject->is<CubicInterpolator>())
		{
			m_Interpolator = coreObject->as<CubicInterpolator>();
		}
	}
}

void KeyFrame::computeSeconds(int fps) { m_Seconds = frame() / (float)fps; }