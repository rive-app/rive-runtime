#include "rive/animation/interpolating_keyframe.hpp"
#include "rive/animation/keyframe_interpolator.hpp"
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/core_context.hpp"
#include "rive/scripted/scripted_interpolator.hpp"

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

KeyFrameInterpolator* InterpolatingKeyFrame::effectiveInterpolator(
    const LinearAnimationInstance* context) const
{
    KeyFrameInterpolator* shared = m_interpolator;
    if (shared == nullptr || context == nullptr ||
        !shared->is<ScriptedInterpolator>())
    {
        return shared;
    }
    auto* sharedSI = static_cast<const ScriptedInterpolator*>(shared);
    if (auto* stateful = context->statefulInterpolator(this, sharedSI))
    {
        return stateful;
    }
    return shared;
}