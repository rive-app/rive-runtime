#ifndef _RIVE_KEY_FRAME_INTERPOLATOR_HPP_
#define _RIVE_KEY_FRAME_INTERPOLATOR_HPP_
#include "rive/generated/animation/keyframe_interpolator_base.hpp"
#include <stdio.h>
namespace rive
{
class KeyFrameInterpolator : public KeyFrameInterpolatorBase
{
public:
    /// Convert a linear interpolation value to an eased one.
    virtual float transformValue(float valueFrom, float valueTo, float factor) = 0;

    /// Convert a linear interpolation factor to an eased one.
    virtual float transform(float factor) const = 0;

    StatusCode import(ImportStack& importStack) override;
};
} // namespace rive

#endif