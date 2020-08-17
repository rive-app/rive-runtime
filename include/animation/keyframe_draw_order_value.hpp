#ifndef _RIVE_KEY_FRAME_DRAW_ORDER_VALUE_HPP_
#define _RIVE_KEY_FRAME_DRAW_ORDER_VALUE_HPP_
#include "generated/animation/keyframe_draw_order_value_base.hpp"
namespace rive
{
	class KeyFrameDrawOrderValue : public KeyFrameDrawOrderValueBase
	{
	public:
		void onAddedClean(CoreContext* context) {}
		void onAddedDirty(CoreContext* context) {}
	};
} // namespace rive

#endif