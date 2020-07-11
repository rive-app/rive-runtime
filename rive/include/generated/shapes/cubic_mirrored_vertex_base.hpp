#ifndef _RIVE_CUBIC_MIRRORED_VERTEX_BASE_HPP_
#define _RIVE_CUBIC_MIRRORED_VERTEX_BASE_HPP_
#include "shapes/cubic_vertex.hpp"
namespace rive
{
	class CubicMirroredVertexBase : public CubicVertex
	{
	private:
		double m_Rotation = 0;
		double m_Distance = 0;
	public:
		double rotation() const { return m_Rotation; }
		void rotation(double value) { m_Rotation = value; }

		double distance() const { return m_Distance; }
		void distance(double value) { m_Distance = value; }
	};
} // namespace rive

#endif