#include "generated/shapes/shape_base.hpp"
#include "shapes/shape.hpp"

using namespace rive;

Core* ShapeBase::clone() const
{
	auto cloned = new Shape();
	cloned->copy(*this);
	return cloned;
}
