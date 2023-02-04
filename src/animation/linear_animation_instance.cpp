#include "rive/animation/linear_animation_instance.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/animation/loop.hpp"
#include "rive/rive_counter.hpp"
#include <cmath>
#include <cassert>

using namespace rive;

LinearAnimationInstance::LinearAnimationInstance(const LinearAnimation* animation,
                                                 ArtboardInstance* instance) :
    Scene(instance),
    m_Animation((assert(animation != nullptr), animation)),
    m_Time(animation->enableWorkArea() ? (float)animation->workStart() / animation->fps() : 0),
    m_TotalTime(0.0f),
    m_LastTotalTime(0.0f),
    m_SpilledTime(0.0f),
    m_Direction(1)
{
    Counter::update(Counter::kLinearAnimationInstance, +1);
}

LinearAnimationInstance::LinearAnimationInstance(LinearAnimationInstance const& lhs) :
    Scene(lhs),
    m_Animation(lhs.m_Animation),
    m_Time(lhs.m_Time),
    m_TotalTime(lhs.m_TotalTime),
    m_LastTotalTime(lhs.m_LastTotalTime),
    m_SpilledTime(lhs.m_SpilledTime),
    m_Direction(lhs.m_Direction),
    m_DidLoop(lhs.m_DidLoop),
    m_LoopValue(lhs.m_LoopValue)
{
    Counter::update(Counter::kLinearAnimationInstance, +1);
}

LinearAnimationInstance::~LinearAnimationInstance()
{
    Counter::update(Counter::kLinearAnimationInstance, -1);
}

bool LinearAnimationInstance::advanceAndApply(float seconds)
{
    bool more = this->advance(seconds);
    this->apply();
    m_ArtboardInstance->advance(seconds);
    return more;
}

bool LinearAnimationInstance::advance(float elapsedSeconds)
{
    const LinearAnimation& animation = *m_Animation;
    float deltaSeconds = elapsedSeconds * animation.speed();

    m_Time += deltaSeconds * m_Direction;
    m_LastTotalTime = m_TotalTime;
    m_TotalTime += deltaSeconds;

    int fps = animation.fps();

    float frames = m_Time * fps;

    int start = animation.enableWorkArea() ? animation.workStart() : 0;
    int end = animation.enableWorkArea() ? animation.workEnd() : animation.duration();
    int range = end - start;

    bool keepGoing = true;
    bool didLoop = false;
    m_SpilledTime = 0.0f;

    int direction = speed() * m_Direction < 0 ? -1 : 1;
    switch (loop())
    {
        case Loop::oneShot:
            if (direction == 1 && frames > end)
            {
                keepGoing = false;
                m_SpilledTime = (frames - end) / fps;
                frames = (float)end;
                m_Time = frames / fps;
                didLoop = true;
            }
            else if (direction == -1 && frames < start)
            {
                keepGoing = false;
                m_SpilledTime = (start - frames) / fps;
                frames = (float)start;
                m_Time = frames / fps;
                didLoop = true;
            }
            break;
        case Loop::loop:
            if (direction == 1 && frames >= end)
            {
                m_SpilledTime = (frames - end) / fps;
                frames = m_Time * fps;
                frames = start + std::fmod(frames - start, (float)range);

                m_Time = frames / fps;
                didLoop = true;
            }
            else if (direction == -1 && frames <= start)
            {
                m_SpilledTime = (start - frames) / fps;
                frames = m_Time * fps;
                frames = end - std::abs(std::fmod(start - frames, (float)range));
                m_Time = frames / fps;
                didLoop = true;
            }
            break;
        case Loop::pingPong:
            while (true)
            {
                if (direction == 1 && frames >= end)
                {
                    m_SpilledTime = (frames - end) / fps;
                    m_Direction *= -1;
                    frames = end + (end - frames);
                    m_Time = frames / fps;
                    didLoop = true;
                }
                else if (direction == -1 && frames < start)
                {
                    m_SpilledTime = (start - frames) / fps;
                    m_Direction *= -1;
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

    int start = (m_Animation->enableWorkArea() ? m_Animation->workStart() : 0) * m_Animation->fps();
    m_TotalTime = value - start;
    m_LastTotalTime = m_TotalTime - diff;

    // leaving this RIGHT now. but is this required? it kinda messes up
    // playing things backwards and seeking. what purpose does it solve?
    m_Direction = 1;
}

uint32_t LinearAnimationInstance::fps() const { return m_Animation->fps(); }

uint32_t LinearAnimationInstance::duration() const { return m_Animation->duration(); }

float LinearAnimationInstance::speed() const { return m_Animation->speed(); }

float LinearAnimationInstance::startSeconds() const { return m_Animation->startSeconds(); }

std::string LinearAnimationInstance::name() const { return m_Animation->name(); }

bool LinearAnimationInstance::isTranslucent() const
{
    return m_ArtboardInstance->isTranslucent(this);
}

// Returns either the animation's default or overridden loop values
int LinearAnimationInstance::loopValue() const
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

float LinearAnimationInstance::durationSeconds() const { return m_Animation->durationSeconds(); }
