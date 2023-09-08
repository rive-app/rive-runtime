#include "rive/animation/interpolating_keyframe.hpp"
#include "rive/animation/cubic_interpolator.hpp"
#include "rive/core_context.hpp"

using namespace rive;

StatusCode InterpolatingKeyFrame::onAddedDirty(CoreContext* context)
{
    if (interpolatorId() != -1)
    {
        auto coreObject = context->resolve(interpolatorId());
        if (coreObject == nullptr || !coreObject->is<CubicInterpolator>())
        {
            return StatusCode::MissingObject;
        }
        m_interpolator = coreObject->as<CubicInterpolator>();
    }

    return StatusCode::Ok;
}