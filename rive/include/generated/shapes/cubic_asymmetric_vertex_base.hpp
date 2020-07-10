#ifndef _RIVE_CUBIC_ASYMMETRIC_VERTEX_BASE_HPP_
#define _RIVE_CUBIC_ASYMMETRIC_VERTEX_BASE_HPP_
#include "shapes/cubic_vertex.hpp"
namespace rive
{
	class CubicAsymmetricVertexBase : public CubicVertex
	{
	private:
		double m_Rotation = 0;
		double m_InDistance = 0;
		double m_OutDistance = 0;
	public:
		double rotation() const { return m_Rotation; }
		void rotation(double value) { m_Rotation = value; }

		double inDistance() const { return m_InDistance; }
		void inDistance(double value) { m_InDistance = value; }

		double outDistance() const { return m_OutDistance; }
		void outDistance(double value) { m_OutDistance = value; }
	};
} // namespace rive

#endif