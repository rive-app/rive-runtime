#include "rive/generated/animation/cubic_interpolator_component_base.hpp"
#include "rive/animation/cubic_interpolator_component.hpp"

using namespace rive;

Core* CubicInterpolatorComponentBase::clone() const
{
    auto cloned = new CubicInterpolatorComponent();
    cloned->copy(*this);
    return cloned;
}
