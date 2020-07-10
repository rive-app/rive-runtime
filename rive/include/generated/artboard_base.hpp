#ifndef _RIVE_ARTBOARD_BASE_HPP_
#define _RIVE_ARTBOARD_BASE_HPP_
#include "container_component.hpp"
namespace rive
{
	class ArtboardBase : public ContainerComponent
	{
	private:
		double m_Width = 0.0;
		double m_Height = 0.0;
		double m_X = 0.0;
		double m_Y = 0.0;
		double m_OriginX = 0.0;
		double m_OriginY = 0.0;
	public:
		double width() const { return m_Width; }
		void width(double value) { m_Width = value; }

		double height() const { return m_Height; }
		void height(double value) { m_Height = value; }

		double x() const { return m_X; }
		void x(double value) { m_X = value; }

		double y() const { return m_Y; }
		void y(double value) { m_Y = value; }

		double originX() const { return m_OriginX; }
		void originX(double value) { m_OriginX = value; }

		double originY() const { return m_OriginY; }
		void originY(double value) { m_OriginY = value; }
	};
} // namespace rive

#endif