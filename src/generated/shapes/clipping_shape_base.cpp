#include "generated/shapes/clipping_shape_base.hpp"
#include "shapes/clipping_shape.hpp"

using namespace rive;

Core* ClippingShapeBase::clone() const
{
	auto cloned = new ClippingShape();
	cloned->copy(*this);
	return cloned;
}
