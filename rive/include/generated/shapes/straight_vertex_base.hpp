#ifndef _RIVE_STRAIGHT_VERTEX_BASE_HPP_
#define _RIVE_STRAIGHT_VERTEX_BASE_HPP_
#include "shapes/path_vertex.hpp"
namespace rive
{
	class StraightVertexBase : public PathVertex
	{
	private:
		double m_Radius = 0;
	public:
		double radius() const { return m_Radius; }
		void radius(double value) { m_Radius = value; }
	};
} // namespace rive

#endif