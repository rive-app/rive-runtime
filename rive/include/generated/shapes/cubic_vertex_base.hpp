#ifndef _RIVE_CUBIC_VERTEX_BASE_HPP_
#define _RIVE_CUBIC_VERTEX_BASE_HPP_
#include "shapes/path_vertex.hpp"
namespace rive
{
	class CubicVertexBase : public PathVertex
	{
	public:
		static const int typeKey = 36;
		int coreType() const override { return typeKey; }
	};
} // namespace rive

#endif