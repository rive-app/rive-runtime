#ifndef _RIVE_KEY_FRAME_DRAW_ORDER_HPP_
#define _RIVE_KEY_FRAME_DRAW_ORDER_HPP_
#include "generated/animation/keyframe_draw_order_base.hpp"
#include <stdio.h>
namespace rive
{
	class KeyFrameDrawOrder : public KeyFrameDrawOrderBase
	{
	public:
		KeyFrameDrawOrder() { printf("Constructing KeyFrameDrawOrder\n"); }
	};
} // namespace rive

#endif