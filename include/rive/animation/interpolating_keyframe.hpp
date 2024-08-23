#ifndef _RIVE_INTERPOLATING_KEY_FRAME_HPP_
#define _RIVE_INTERPOLATING_KEY_FRAME_HPP_
#include "rive/generated/animation/interpolating_keyframe_base.hpp"

namespace rive
{
class KeyFrameInterpolator;
class InterpolatingKeyFrame : public InterpolatingKeyFrameBase
{
public:
    inline KeyFrameInterpolator* interpolator() const { return m_interpolator; }
    virtual void apply(Core* object, int propertyKey, float mix) = 0;
    virtual void applyInterpolation(Core* object,
                                    int propertyKey,
                                    float seconds,
                                    const KeyFrame* nextFrame,
                                    float mix) = 0;
    StatusCode onAddedDirty(CoreContext* context) override;

private:
    KeyFrameInterpolator* m_interpolator = nullptr;
};
} // namespace rive

#endif