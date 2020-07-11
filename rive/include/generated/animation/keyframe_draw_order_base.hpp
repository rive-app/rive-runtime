#ifndef _RIVE_KEY_FRAME_DRAW_ORDER_BASE_HPP_
#define _RIVE_KEY_FRAME_DRAW_ORDER_BASE_HPP_
#include "animation/keyframe.hpp"
namespace rive
{
	class KeyFrameDrawOrderBase : public KeyFrame
	{
	public:
		static const int typeKey = 32;
		int coreType() const override { return typeKey; }
	};
} // namespace rive

#endif