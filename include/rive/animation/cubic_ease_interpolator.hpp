#ifndef _RIVE_CUBIC_EASE_INTERPOLATOR_HPP_
#define _RIVE_CUBIC_EASE_INTERPOLATOR_HPP_
#include "rive/generated/animation/cubic_ease_interpolator_base.hpp"
#include <stdio.h>
namespace rive
{
class CubicEaseInterpolator : public CubicEaseInterpolatorBase
{
public:
    float transformValue(float valueFrom, float valueTo, float factor) override;
    float transform(float factor) const override;
};
} // namespace rive

#endif