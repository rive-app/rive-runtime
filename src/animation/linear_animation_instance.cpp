#include "rive/animation/linear_animation_instance.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/animation/loop.hpp"
#include <cmath>

using namespace rive;

LinearAnimationInstance::LinearAnimationInstance(
    const LinearAnimation* animation) :
    m_Animation(animation),
    m_Time(animation->enableWorkArea()
               ? (float)animation->workStart() / animation->fps()
               : 0),
    m_TotalTime(0.0f),
    m_LastTotalTime(0.0f),
    m_SpilledTime(0.0f),
    m_Direction(1)
{
}

bool LinearAnimationInstance::advance(float elapsedSeconds)
{
	const LinearAnimation& animation = *m_Animation;
	m_Time += elapsedSeconds * animation.speed() * m_Direction;
	m_LastTotalTime = m_TotalTime;
	m_TotalTime += elapsedSeconds;

	int fps = animation.fps();

	float frames = m_Time * fps;

	int start = animation.enableWorkArea() ? animation.workStart() : 0;
	int end =
	    animation.enableWorkArea() ? animation.workEnd() : animation.duration();
	int range = end - start;

	bool keepGoing = true;
	bool didLoop = false;
	m_SpilledTime = 0.0f;

	switch (loop())
	{
		case Loop::oneShot:
			if (m_Direction == 1 && frames > end)
			{
				keepGoing = false;
				m_SpilledTime = (frames - end) / fps;
				frames = end;
				m_Time = frames / fps;
				didLoop = true;
			}
			else if (m_Direction == -1 && frames < start)
			{
				keepGoing = false;
				m_SpilledTime = (start - frames) / fps;
				frames = start;
				m_Time = frames / fps;
				didLoop = true;
			}
			break;
		case Loop::loop:
			if (m_Direction == 1 && frames >= end)
			{
				m_SpilledTime = (frames - end) / fps;
				frames = m_Time * fps;
				frames = start + std::fmod(frames - start, range);

				m_Time = frames / fps;
				didLoop = true;
			}
			else if (m_Direction == -1 && frames <= start)
			{

				m_SpilledTime = (start - frames) / fps;
				frames = m_Time * fps;
				frames = end - std::abs(std::fmod(start - frames, range));
				m_Time = frames / fps;
				didLoop = true;
			}
			break;
		case Loop::pingPong:
			while (true)
			{
				if (m_Direction == 1 && frames >= end)
				{
					m_SpilledTime = (frames - end) / fps;
					m_Direction = -1;
					frames = end + (end - frames);
					m_Time = frames / fps;
					didLoop = true;
				}
				else if (m_Direction == -1 && frames < start)
				{
					m_SpilledTime = (start - frames) / fps;
					m_Direction = 1;
					frames = start + (start - frames);
					m_Time = frames / fps;
					didLoop = true;
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

	m_DidLoop = didLoop;
	return keepGoing;
}

void LinearAnimationInstance::time(float value)
{
	if (m_Time == value)
	{
		return;
	}
	m_Time = value;
	// Make sure to keep last and total in relative lockstep so state machines
	// can track change even when setting time.
	auto diff = m_TotalTime - m_LastTotalTime;

	int start = (m_Animation->enableWorkArea() ? m_Animation->workStart() : 0) *
	            m_Animation->fps();
	m_TotalTime = value - start;
	m_LastTotalTime = m_TotalTime - diff;

	// leaving this RIGHT now. but is this required? it kinda messes up
	// playing things backwards and seeking. what purpose does it solve?
	m_Direction = 1;
}

// Returns either the animation's default or overridden loop values
int LinearAnimationInstance::loopValue()
{
	if (m_LoopValue != -1)
	{
		return m_LoopValue;
	}
	return m_Animation->loopValue();
}

// Override the animation's loop value
void LinearAnimationInstance::loopValue(int value)
{
	if (m_LoopValue == value)
	{
		return;
	}
	if (m_LoopValue == -1 && m_Animation->loopValue() == value)
	{
		return;
	}
	m_LoopValue = value;
}