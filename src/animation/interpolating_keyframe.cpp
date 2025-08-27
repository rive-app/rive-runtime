#include "rive/animation/interpolating_keyframe.hpp"
#include "rive/animation/keyframe_interpolator.hpp"
#include "rive/core_context.hpp"

using namespace rive;

StatusCode InterpolatingKeyFrame::onAddedDirty(CoreContext* context)
{
    if (interpolatorId() != -1)
    {
        auto coreObject = context->resolve(interpolatorId());
        if (coreObject == nullptr || !coreObject->is<KeyFrameInterpolator>())
        {
            return StatusCode::MissingObject;
        }
        m_interpolator = coreObject->as<KeyFrameInterpolator>();
    }

    return StatusCode::Ok;
}