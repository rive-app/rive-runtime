#ifndef _RIVE_NESTED_REMAP_ANIMATION_HPP_
#define _RIVE_NESTED_REMAP_ANIMATION_HPP_
#include "rive/generated/animation/nested_remap_animation_base.hpp"
#include <stdio.h>
namespace rive
{
	class NestedRemapAnimation : public NestedRemapAnimationBase
	{
	public:
		void timeChanged() override;
		void advance(float elapsedSeconds, Artboard* artboard) override;
		void initializeAnimation(Artboard* artboard) override;
	};
} // namespace rive

#endif