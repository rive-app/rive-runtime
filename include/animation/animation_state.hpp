#ifndef _RIVE_ANIMATION_STATE_HPP_
#define _RIVE_ANIMATION_STATE_HPP_
#include "generated/animation/animation_state_base.hpp"
#include <stdio.h>
namespace rive
{
	class LinearAnimation;
	class Artboard;
	class StateMachineLayerImporter;

	class AnimationState : public AnimationStateBase
	{
		friend class StateMachineLayerImporter;

	private:
		LinearAnimation* m_Animation = nullptr;

	public:
		const LinearAnimation* animation() const { return m_Animation; }
	};
} // namespace rive

#endif