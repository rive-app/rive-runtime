#ifndef _RIVE_PATH_VERTEX_BASE_HPP_
#define _RIVE_PATH_VERTEX_BASE_HPP_
#include "component.hpp"
namespace rive
{
	class PathVertexBase : public Component
	{
	private:
		double m_X = 0.0;
		double m_Y = 0.0;
	public:
		double x() const { return m_X; }
		void x(double value) { m_X = value; }

		double y() const { return m_Y; }
		void y(double value) { m_Y = value; }
	};
} // namespace rive

#endif