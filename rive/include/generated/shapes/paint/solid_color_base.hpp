#ifndef _RIVE_SOLID_COLOR_BASE_HPP_
#define _RIVE_SOLID_COLOR_BASE_HPP_
#include "component.hpp"
namespace rive
{
	class SolidColorBase : public Component
	{
	private:
		int m_ColorValue = 0xFF747474;
	public:
		int colorValue() const { return m_ColorValue; }
		void colorValue(int value) { m_ColorValue = value; }
	};
} // namespace rive

#endif