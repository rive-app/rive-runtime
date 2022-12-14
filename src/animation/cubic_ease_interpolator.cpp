#include "rive/animation/cubic_ease_interpolator.hpp"

using namespace rive;

float CubicEaseInterpolator::transformValue(float valueFrom, float valueTo, float factor)
{
    return valueFrom + (valueTo - valueFrom) * calcBezier(getT(factor), y1(), y2());
}

float CubicEaseInterpolator::transform(float factor) const
{
    return calcBezier(getT(factor), y1(), y2());
}