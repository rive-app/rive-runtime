#include "generated/shapes/polygon_base.hpp"
#include "shapes/polygon.hpp"

using namespace rive;

Core* PolygonBase::clone() const
{
	auto cloned = new Polygon();
	cloned->copy(*this);
	return cloned;
}
