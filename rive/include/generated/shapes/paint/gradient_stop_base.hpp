#ifndef _RIVE_GRADIENT_STOP_BASE_HPP_
#define _RIVE_GRADIENT_STOP_BASE_HPP_
#include "component.hpp"
namespace rive
{
	class GradientStopBase : public Component
	{
	private:
		int m_ColorValue = 0xFFFFFFFF;
		double m_Position = 0;
	public:
		int colorValue() const { return m_ColorValue; }
		void colorValue(int value) { m_ColorValue = value; }

		double position() const { return m_Position; }
		void position(double value) { m_Position = value; }
	};
} // namespace rive

#endif