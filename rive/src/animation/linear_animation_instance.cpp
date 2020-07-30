#include "animation/linear_animation_instance.hpp"
#include "animation/loop.hpp"
#include <cmath>

using namespace rive;

LinearAnimationInstance::LinearAnimationInstance(LinearAnimation* animation) :
    m_Animation(animation),
    m_Time(animation->enableWorkArea() ? animation->workStart() : 0),
    m_Direction(1)
{
}

bool LinearAnimationInstance::advance(float elapsedSeconds)
{
	LinearAnimation& animation = *m_Animation;
	m_Time += elapsedSeconds * animation.speed() * m_Direction;

	int fps = animation.fps();

	float frames = m_Time * fps;

	int start = animation.enableWorkArea() ? animation.workStart() : 0;
	int end =
	    animation.enableWorkArea() ? animation.workEnd() : animation.duration();
	int range = end - start;

	bool keepGoing = true;

	switch (animation.loop())
	{
		case Loop::oneShot:
			if (frames > end)
			{
				keepGoing = false;
				frames = end;
				m_Time = frames / fps;
			}
			break;
		case Loop::loop:
			if (frames >= end)
			{
				frames = m_Time * fps;
				frames = start + std::fmod(frames - start, range);
				m_Time = frames / fps;
			}
			break;
		case Loop::pingPong:
			while (true)
			{
				if (m_Direction == 1 && frames >= end)
				{
					m_Direction = -1;
					frames = end + (end - frames);
					m_Time = frames / fps;
				}
				else if (m_Direction == -1 && frames < start)
				{
					m_Direction = 1;
					frames = start + (start - frames);
					m_Time = frames / fps;
				}
				else
				{
					// we're within the range, we can stop fixing. We do this in
					// a loop to fix conditions when time has advanced so far
					// that we've ping-ponged back and forth a few times in a
					// single frame. We want to accomodate for this in cases
					// where animations are not advanced on regular intervals.
					break;
				}
			}
			break;
	}
	return keepGoing;
}

void LinearAnimationInstance::time(float value)
{
	if (m_Time == value)
	{
		return;
	}
	m_Time = value;
	m_Direction = 1;
}
