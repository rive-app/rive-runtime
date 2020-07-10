#ifndef _RIVE_LINEAR_ANIMATION_BASE_HPP_
#define _RIVE_LINEAR_ANIMATION_BASE_HPP_
#include "animation/animation.hpp"
namespace rive
{
	class LinearAnimationBase : public Animation
	{
	private:
		int m_Fps = 60;
		int m_Duration = 60;
		double m_Speed = 1;
		int m_LoopValue = 0;
		int m_WorkStart = 0;
		int m_WorkEnd = 0;
		bool m_EnableWorkArea = false;
	public:
		int fps() const { return m_Fps; }
		void fps(int value) { m_Fps = value; }

		int duration() const { return m_Duration; }
		void duration(int value) { m_Duration = value; }

		double speed() const { return m_Speed; }
		void speed(double value) { m_Speed = value; }

		int loopValue() const { return m_LoopValue; }
		void loopValue(int value) { m_LoopValue = value; }

		int workStart() const { return m_WorkStart; }
		void workStart(int value) { m_WorkStart = value; }

		int workEnd() const { return m_WorkEnd; }
		void workEnd(int value) { m_WorkEnd = value; }

		bool enableWorkArea() const { return m_EnableWorkArea; }
		void enableWorkArea(bool value) { m_EnableWorkArea = value; }
	};
} // namespace rive

#endif