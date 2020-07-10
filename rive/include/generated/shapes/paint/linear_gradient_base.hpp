#ifndef _RIVE_LINEAR_GRADIENT_BASE_HPP_
#define _RIVE_LINEAR_GRADIENT_BASE_HPP_
#include "container_component.hpp"
namespace rive
{
	class LinearGradientBase : public ContainerComponent
	{
	private:
		double m_StartX = 0;
		double m_StartY = 0;
		double m_EndX = 0;
		double m_EndY = 0;
		double m_Opacity = 1;
	public:
		double startX() const { return m_StartX; }
		void startX(double value) { m_StartX = value; }

		double startY() const { return m_StartY; }
		void startY(double value) { m_StartY = value; }

		double endX() const { return m_EndX; }
		void endX(double value) { m_EndX = value; }

		double endY() const { return m_EndY; }
		void endY(double value) { m_EndY = value; }

		double opacity() const { return m_Opacity; }
		void opacity(double value) { m_Opacity = value; }
	};
} // namespace rive

#endif