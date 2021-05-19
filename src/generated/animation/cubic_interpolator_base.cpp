#include "generated/animation/cubic_interpolator_base.hpp"
#include "animation/cubic_interpolator.hpp"

using namespace rive;

Core* CubicInterpolatorBase::clone() const
{
	auto cloned = new CubicInterpolator();
	cloned->copy(*this);
	return cloned;
}
