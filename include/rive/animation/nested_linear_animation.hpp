#ifndef _RIVE_NESTED_LINEAR_ANIMATION_HPP_
#define _RIVE_NESTED_LINEAR_ANIMATION_HPP_
#include "rive/generated/animation/nested_linear_animation_base.hpp"
#include <stdio.h>
namespace rive
{
	class LinearAnimationInstance;
	class NestedLinearAnimation : public NestedLinearAnimationBase
	{
	private:
		LinearAnimationInstance* m_AnimationInstance = nullptr;

	protected:
		LinearAnimationInstance* animationInstance()
		{
			return m_AnimationInstance;
		}

	public:
		~NestedLinearAnimation();
		void advance(float elapsedSeconds, Artboard* artboard) override;
		void initializeAnimation(Artboard* artboard) override;
	};
} // namespace rive

#endif