#ifndef _RIVE_CUBIC_ASYMMETRIC_VERTEX_HPP_
#define _RIVE_CUBIC_ASYMMETRIC_VERTEX_HPP_
#include "generated/shapes/cubic_asymmetric_vertex_base.hpp"
#include <stdio.h>
namespace rive
{
	class CubicAsymmetricVertex : public CubicAsymmetricVertexBase
	{
	public:
		CubicAsymmetricVertex() { printf("Constructing CubicAsymmetricVertex\n"); }
	};
} // namespace rive

#endif