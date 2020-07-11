#ifndef _RIVE_KEY_FRAME_DRAW_ORDER_VALUE_BASE_HPP_
#define _RIVE_KEY_FRAME_DRAW_ORDER_VALUE_BASE_HPP_
#include "core.hpp"
namespace rive
{
	class KeyFrameDrawOrderValueBase : public Core
	{
	private:
		int m_DrawableId = 0;
		int m_Value = 0;
	public:
		int drawableId() const { return m_DrawableId; }
		void drawableId(int value) { m_DrawableId = value; }

		int value() const { return m_Value; }
		void value(int value) { m_Value = value; }
	};
} // namespace rive

#endif