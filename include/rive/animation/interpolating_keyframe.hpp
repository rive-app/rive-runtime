#ifndef _RIVE_INTERPOLATING_KEY_FRAME_HPP_
#define _RIVE_INTERPOLATING_KEY_FRAME_HPP_
#include "rive/generated/animation/interpolating_keyframe_base.hpp"
#include <stdio.h>
namespace rive
{
class InterpolatingKeyFrame : public InterpolatingKeyFrameBase
{
public:
    inline CubicInterpolator* interpolator() const { return m_interpolator; }
    virtual void apply(Core* object, int propertyKey, float mix) = 0;
    virtual void applyInterpolation(Core* object,
                                    int propertyKey,
                                    float seconds,
                                    const KeyFrame* nextFrame,
                                    float mix) = 0;
    StatusCode onAddedDirty(CoreContext* context) override;

private:
    CubicInterpolator* m_interpolator = nullptr;
};
} // namespace rive

#endif