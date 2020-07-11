#ifndef _RIVE_SHAPE_PAINT_BASE_HPP_
#define _RIVE_SHAPE_PAINT_BASE_HPP_
#include "container_component.hpp"
namespace rive
{
	class ShapePaintBase : public ContainerComponent
	{
	private:
		bool m_IsVisible = true;
	public:
		bool isVisible() const { return m_IsVisible; }
		void isVisible(bool value) { m_IsVisible = value; }
	};
} // namespace rive

#endif