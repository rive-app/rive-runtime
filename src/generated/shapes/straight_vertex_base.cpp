#include "generated/shapes/straight_vertex_base.hpp"
#include "shapes/straight_vertex.hpp"

using namespace rive;

Core* StraightVertexBase::clone() const
{
	auto cloned = new StraightVertex();
	cloned->copy(*this);
	return cloned;
}
