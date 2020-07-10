#ifndef _RIVE_CUBIC_MIRRORED_VERTEX_HPP_
#define _RIVE_CUBIC_MIRRORED_VERTEX_HPP_
#include "generated/shapes/cubic_mirrored_vertex_base.hpp"
#include <stdio.h>
namespace rive
{
	class CubicMirroredVertex : public CubicMirroredVertexBase
	{
	public:
		CubicMirroredVertex() { printf("Constructing CubicMirroredVertex\n"); }
	};
} // namespace rive

#endif