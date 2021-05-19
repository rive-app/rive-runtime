#include "generated/shapes/cubic_asymmetric_vertex_base.hpp"
#include "shapes/cubic_asymmetric_vertex.hpp"

using namespace rive;

Core* CubicAsymmetricVertexBase::clone() const
{
	auto cloned = new CubicAsymmetricVertex();
	cloned->copy(*this);
	return cloned;
}
