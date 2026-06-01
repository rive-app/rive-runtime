#ifndef _RIVE_INTERPOLATING_KEY_FRAME_HPP_
#define _RIVE_INTERPOLATING_KEY_FRAME_HPP_
#include "rive/generated/animation/interpolating_keyframe_base.hpp"

namespace rive
{
class KeyFrameInterpolator;
class LinearAnimationInstance;
class InterpolatingKeyFrame : public InterpolatingKeyFrameBase
{
public:
    inline KeyFrameInterpolator* interpolator() const { return m_interpolator; }

    /// Returns the interpolator that should run for this keyframe given the
    /// LAI context. For ScriptedInterpolators this resolves to the per-(LAI,
    /// keyframe) stateful clone; for everything else (or when context is
    /// null) it returns the shared `m_interpolator`. Defined out-of-line so
    /// the header doesn't have to drag in linear_animation_instance.hpp.
    KeyFrameInterpolator* effectiveInterpolator(
        const LinearAnimationInstance* context) const;

    virtual void apply(Core* object, int propertyKey, float mix) = 0;
    virtual void applyInterpolation(
        Core* object,
        int propertyKey,
        float seconds,
        const KeyFrame* nextFrame,
        float mix,
        const LinearAnimationInstance* context = nullptr) = 0;
    StatusCode onAddedDirty(CoreContext* context) override;

private:
    KeyFrameInterpolator* m_interpolator = nullptr;
};
} // namespace rive

#endif