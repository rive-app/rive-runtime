#ifndef _RIVE_KEY_FRAME_HPP_
#define _RIVE_KEY_FRAME_HPP_
#include "generated/animation/keyframe_base.hpp"
namespace rive
{
	class KeyFrame : public KeyFrameBase
	{
	public:
		void onAddedDirty(CoreContext* context) override {}
		void onAddedClean(CoreContext* context) override {}
	};
} // namespace rive

#endif