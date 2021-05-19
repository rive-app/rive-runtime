#include "generated/shapes/ellipse_base.hpp"
#include "shapes/ellipse.hpp"

using namespace rive;

Core* EllipseBase::clone() const
{
	auto cloned = new Ellipse();
	cloned->copy(*this);
	return cloned;
}
