#include "rive/generated/animation/cubic_ease_interpolator_base.hpp"
#include "rive/animation/cubic_ease_interpolator.hpp"

using namespace rive;

Core* CubicEaseInterpolatorBase::clone() const
{
    auto cloned = new CubicEaseInterpolator();
    cloned->copy(*this);
    return cloned;
}
