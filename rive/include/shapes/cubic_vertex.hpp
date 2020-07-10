#ifndef _RIVE_CUBIC_VERTEX_HPP_
#define _RIVE_CUBIC_VERTEX_HPP_
#include "generated/shapes/cubic_vertex_base.hpp"
#include <stdio.h>
namespace rive
{
	class CubicVertex : public CubicVertexBase
	{
	public:
		CubicVertex() { printf("Constructing CubicVertex\n"); }
	};
} // namespace rive

#endif