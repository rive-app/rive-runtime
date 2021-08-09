#ifndef _RIVE_BLEND_STATE_DIRECT_INSTANCE_HPP_
#define _RIVE_BLEND_STATE_DIRECT_INSTANCE_HPP_

#include "animation/blend_state_instance.hpp"
#include "animation/blend_state_direct.hpp"
#include "animation/blend_animation_direct.hpp"

namespace rive
{
	class BlendStateDirectInstance
	    : public BlendStateInstance<BlendStateDirect, BlendAnimationDirect>
	{
	public:
		BlendStateDirectInstance(const BlendStateDirect* blendState);
		void advance(float seconds, SMIInput** inputs) override;
	};
} // namespace rive
#endif