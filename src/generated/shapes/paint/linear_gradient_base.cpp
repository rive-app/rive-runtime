#include "generated/shapes/paint/linear_gradient_base.hpp"
#include "shapes/paint/linear_gradient.hpp"

using namespace rive;

Core* LinearGradientBase::clone() const
{
	auto cloned = new LinearGradient();
	cloned->copy(*this);
	return cloned;
}
