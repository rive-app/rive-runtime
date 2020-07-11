#ifndef _RIVE_NODE_BASE_HPP_
#define _RIVE_NODE_BASE_HPP_
#include "container_component.hpp"
namespace rive
{
	class NodeBase : public ContainerComponent
	{
	private:
		double m_X = 0;
		double m_Y = 0;
		double m_Rotation = 0;
		double m_ScaleX = 1;
		double m_ScaleY = 1;
		double m_Opacity = 1;
	public:
		double x() const { return m_X; }
		void x(double value) { m_X = value; }

		double y() const { return m_Y; }
		void y(double value) { m_Y = value; }

		double rotation() const { return m_Rotation; }
		void rotation(double value) { m_Rotation = value; }

		double scaleX() const { return m_ScaleX; }
		void scaleX(double value) { m_ScaleX = value; }

		double scaleY() const { return m_ScaleY; }
		void scaleY(double value) { m_ScaleY = value; }

		double opacity() const { return m_Opacity; }
		void opacity(double value) { m_Opacity = value; }
	};
} // namespace rive

#endif