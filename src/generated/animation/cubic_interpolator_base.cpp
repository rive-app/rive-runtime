#include "rive/generated/animation/cubic_interpolator_base.hpp"
#include "rive/animation/cubic_interpolator.hpp"

using namespace rive;

Core* CubicInterpolatorBase::clone() const
{
	auto cloned = new CubicInterpolator();
	cloned->copy(*this);
	return cloned;
}
