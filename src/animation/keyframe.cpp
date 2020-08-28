#include "animation/keyframe.hpp"
#include "animation/cubic_interpolator.hpp"
#include "core_context.hpp"

using namespace rive;

StatusCode KeyFrame::onAddedDirty(CoreContext* context)
{
	if (interpolatorId() != 0)
	{
		auto coreObject = context->resolve(interpolatorId());
		if (coreObject == nullptr || !coreObject->is<CubicInterpolator>())
		{
			return StatusCode::MissingObject;
		}

		m_Interpolator = coreObject->as<CubicInterpolator>();
	}

	return StatusCode::Ok;
}

void KeyFrame::computeSeconds(int fps) { m_Seconds = frame() / (float)fps; }