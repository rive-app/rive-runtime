#ifndef _RIVE_KEY_FRAME_DOUBLE_HPP_
#define _RIVE_KEY_FRAME_DOUBLE_HPP_
#include "generated/animation/keyframe_double_base.hpp"
#include <stdio.h>
namespace rive
{
	class KeyFrameDouble : public KeyFrameDoubleBase
	{
	public:
		KeyFrameDouble() { printf("Constructing KeyFrameDouble\n"); }
	};
} // namespace rive

#endif