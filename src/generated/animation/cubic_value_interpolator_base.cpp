#include "rive/generated/animation/cubic_value_interpolator_base.hpp"
#include "rive/animation/cubic_value_interpolator.hpp"

using namespace rive;

Core* CubicValueInterpolatorBase::clone() const
{
    auto cloned = new CubicValueInterpolator();
    cloned->copy(*this);
    return cloned;
}
