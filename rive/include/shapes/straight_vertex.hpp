#ifndef _RIVE_STRAIGHT_VERTEX_HPP_
#define _RIVE_STRAIGHT_VERTEX_HPP_
#include "generated/shapes/straight_vertex_base.hpp"
#include <stdio.h>
namespace rive
{
	class StraightVertex : public StraightVertexBase
	{
	public:
		StraightVertex() { printf("Constructing StraightVertex\n"); }
	};
} // namespace rive

#endif