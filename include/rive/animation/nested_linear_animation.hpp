#ifndef _RIVE_NESTED_LINEAR_ANIMATION_HPP_
#define _RIVE_NESTED_LINEAR_ANIMATION_HPP_
#include "rive/generated/animation/nested_linear_animation_base.hpp"
#include <stdio.h>
namespace rive
{
	class LinearAnimationInstance;
	class NestedLinearAnimation : public NestedLinearAnimationBase
	{
	protected:
		LinearAnimationInstance* m_AnimationInstance = nullptr;

	public:
		~NestedLinearAnimation();

		void initializeAnimation(Artboard* artboard) override;
	};
} // namespace rive

#endif