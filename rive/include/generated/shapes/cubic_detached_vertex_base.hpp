#ifndef _RIVE_CUBIC_DETACHED_VERTEX_BASE_HPP_
#define _RIVE_CUBIC_DETACHED_VERTEX_BASE_HPP_
#include "shapes/cubic_vertex.hpp"
namespace rive
{
	class CubicDetachedVertexBase : public CubicVertex
	{
	private:
		double m_InRotation = 0;
		double m_InDistance = 0;
		double m_OutRotation = 0;
		double m_OutDistance = 0;
	public:
		double inRotation() const { return m_InRotation; }
		void inRotation(double value) { m_InRotation = value; }

		double inDistance() const { return m_InDistance; }
		void inDistance(double value) { m_InDistance = value; }

		double outRotation() const { return m_OutRotation; }
		void outRotation(double value) { m_OutRotation = value; }

		double outDistance() const { return m_OutDistance; }
		void outDistance(double value) { m_OutDistance = value; }
	};
} // namespace rive

#endif