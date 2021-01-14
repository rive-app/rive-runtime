#ifndef _RIVE_LINEAR_ANIMATION_INSTANCE_HPP_
#define _RIVE_LINEAR_ANIMATION_INSTANCE_HPP_
#include "animation/linear_animation.hpp"

namespace rive
{
	// Return result when calling advanceDidLoop
	struct AdvanceResult
	{
		bool isActive;
		bool didLoop;
	};

	class LinearAnimation;

	class LinearAnimationInstance
	{
	private:
		LinearAnimation* m_Animation = nullptr;
		float m_Time;
		int m_Direction;

	public:
		LinearAnimationInstance(LinearAnimation* animation);
		bool advance(float seconds);
		bool advance(float seconds, bool& didLoop);
		AdvanceResult advanceDidLoop(float seconds);
		LinearAnimation* animation() const { return m_Animation; }
		float time() const { return m_Time; }
		void time(float value);
		void apply(Artboard* artboard, float mix = 1.0f) const
		{
			m_Animation->apply(artboard, m_Time, mix);
		}
	};
} // namespace rive
#endif