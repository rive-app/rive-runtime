#ifndef _RIVE_KEY_FRAME_DRAW_ORDER_HPP_
#define _RIVE_KEY_FRAME_DRAW_ORDER_HPP_
#include "generated/animation/keyframe_draw_order_base.hpp"
#include <vector>
namespace rive
{
	class KeyFrameDrawOrderValue;
	class KeyFrameDrawOrder : public KeyFrameDrawOrderBase
	{
	private:
		std::vector<KeyFrameDrawOrderValue*> m_Values;
	public:
		void addValue(KeyFrameDrawOrderValue* value);
	};
} // namespace rive

#endif