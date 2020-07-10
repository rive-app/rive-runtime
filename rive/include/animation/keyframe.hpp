#ifndef _RIVE_KEY_FRAME_HPP_
#define _RIVE_KEY_FRAME_HPP_
#include "generated/animation/keyframe_base.hpp"
#include <stdio.h>
namespace rive
{
	class KeyFrame : public KeyFrameBase
	{
	public:
		KeyFrame() { printf("Constructing KeyFrame\n"); }
	};
} // namespace rive

#endif