#ifndef _RIVE_LINEAR_ANIMATION_INSTANCE_HPP_
#define _RIVE_LINEAR_ANIMATION_INSTANCE_HPP_
#include "animation/linear_animation.hpp"
#include <vector>
namespace rive
{
	class LinearAnimation;
	class LinearAnimationInstance
	{
	private:
		LinearAnimation* m_Animation = nullptr;
		float m_Time = 0.0f;
		int m_Direction = 1;

	public:
		LinearAnimationInstance(LinearAnimation* animation);
		bool advance(float seconds);
		float time() const { return m_Time; }
		void time(float value);
		void apply(Artboard* artboard, float mix = 1.0f) const
		{
			m_Animation->apply(artboard, m_Time, mix);
		}
	};
} // namespace rive
#endif