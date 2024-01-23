#include "rive/animation/linear_animation_instance.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/animation/loop.hpp"
#include "rive/animation/keyed_callback_reporter.hpp"
#include <cmath>
#include <cassert>

using namespace rive;

LinearAnimationInstance::LinearAnimationInstance(const LinearAnimation* animation,
                                                 ArtboardInstance* instance,
                                                 float speedMultiplier) :
    Scene(instance),
    m_animation((assert(animation != nullptr), animation)),
    m_time((speedMultiplier >= 0) ? animation->startTime() : animation->endTime()),
    m_totalTime(0.0f),
    m_lastTotalTime(0.0f),
    m_spilledTime(0.0f),
    m_direction(1)
{}

LinearAnimationInstance::LinearAnimationInstance(LinearAnimationInstance const& lhs) :
    Scene(lhs),
    m_animation(lhs.m_animation),
    m_time(lhs.m_time),
    m_totalTime(lhs.m_totalTime),
    m_lastTotalTime(lhs.m_lastTotalTime),
    m_spilledTime(lhs.m_spilledTime),
    m_direction(lhs.m_direction),
    m_didLoop(lhs.m_didLoop),
    m_loopValue(lhs.m_loopValue)
{}

LinearAnimationInstance::~LinearAnimationInstance() {}

bool LinearAnimationInstance::advanceAndApply(float seconds)
{
    bool more = this->advance(seconds, this);
    this->apply();
    m_artboardInstance->advance(seconds);
    return more;
}

bool LinearAnimationInstance::advance(float elapsedSeconds, KeyedCallbackReporter* reporter)
{
    const LinearAnimation& animation = *m_animation;
    float deltaSeconds = elapsedSeconds * animation.speed() * m_direction;
    m_spilledTime = 0.0f;
    if (deltaSeconds == 0)
    {
        m_didLoop = false;
        return true;
    }

    m_lastTotalTime = m_totalTime;
    m_totalTime += std::abs(deltaSeconds);

    // NOTE:
    // do not track spilled time, if our one shot loop is already completed.
    // stop gap before we move spilled tracking into state machine logic.
    bool killSpilledTime = !this->keepGoing();

    float lastTime = m_time;
    m_time += deltaSeconds;
    if (reporter != nullptr)
    {
        animation.reportKeyedCallbacks(reporter, lastTime, m_time);
    }

    int fps = animation.fps();
    float frames = m_time * fps;
    int start = animation.enableWorkArea() ? animation.workStart() : 0;
    int end = animation.enableWorkArea() ? animation.workEnd() : animation.duration();
    int range = end - start;

    bool didLoop = false;

    // this has some issues when deltaSeconds is 0,
    // right now we basically assume we default to going forwards in that case
    //
    int direction = deltaSeconds < 0 ? -1 : 1;
    switch (loop())
    {
        case Loop::oneShot:
            if (direction == 1 && frames > end)
            {
                m_spilledTime = (frames - end) / fps;
                frames = (float)end;
                m_time = frames / fps;
                didLoop = true;
            }
            else if (direction == -1 && frames < start)
            {
                m_spilledTime = (start - frames) / fps;
                frames = (float)start;
                m_time = frames / fps;
                didLoop = true;
            }
            break;
        case Loop::loop:
            if (direction == 1 && frames >= end)
            {
                m_spilledTime = (frames - end) / fps;
                frames = m_time * fps;
                frames = start + std::fmod(frames - start, (float)range);
                m_time = frames / fps;
                didLoop = true;
                if (reporter != nullptr)
                {
                    animation.reportKeyedCallbacks(reporter, 0.0f, m_time);
                }
            }
            else if (direction == -1 && frames <= start)
            {
                m_spilledTime = (start - frames) / fps;
                frames = m_time * fps;
                frames = end - std::abs(std::fmod(start - frames, (float)range));
                m_time = frames / fps;
                didLoop = true;
                if (reporter != nullptr)
                {
                    animation.reportKeyedCallbacks(reporter, end / (float)fps, m_time);
                }
            }
            break;
        case Loop::pingPong:
            while (true)
            {
                if (direction == 1 && frames >= end)
                {
                    m_spilledTime = (frames - end) / fps;
                    frames = end + (end - frames);
                    lastTime = end / (float)fps;
                }
                else if (direction == -1 && frames < start)
                {
                    m_spilledTime = (start - frames) / fps;
                    frames = start + (start - frames);
                    lastTime = start / (float)fps;
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
                m_time = frames / fps;
                m_direction *= -1;
                direction *= -1;
                didLoop = true;
                if (reporter != nullptr)
                {
                    animation.reportKeyedCallbacks(reporter, lastTime, m_time);
                }
            }
            break;
    }

    if (killSpilledTime)
    {
        m_spilledTime = 0;
    }

    m_didLoop = didLoop;
    return this->keepGoing();
}

void LinearAnimationInstance::time(float value)
{
    if (m_time == value)
    {
        return;
    }
    m_time = value;
    // Make sure to keep last and total in relative lockstep so state machines
    // can track change even when setting time.
    auto diff = m_totalTime - m_lastTotalTime;

    int start = (m_animation->enableWorkArea() ? m_animation->workStart() : 0) * m_animation->fps();
    m_totalTime = value - start;
    m_lastTotalTime = m_totalTime - diff;

    // leaving this RIGHT now. but is this required? it kinda messes up
    // playing things backwards and seeking. what purpose does it solve?
    m_direction = 1;
}

void LinearAnimationInstance::reset(float speedMultiplier = 1.0)
{
    m_time = (speedMultiplier >= 0) ? m_animation->startTime() : m_animation->endTime();
}

uint32_t LinearAnimationInstance::fps() const { return m_animation->fps(); }

uint32_t LinearAnimationInstance::duration() const { return m_animation->duration(); }

float LinearAnimationInstance::speed() const { return m_animation->speed(); }

float LinearAnimationInstance::startTime() const { return m_animation->startTime(); }

std::string LinearAnimationInstance::name() const { return m_animation->name(); }

bool LinearAnimationInstance::isTranslucent() const
{
    return m_artboardInstance->isTranslucent(this);
}

// Returns either the animation's default or overridden loop values
int LinearAnimationInstance::loopValue() const
{
    if (m_loopValue != -1)
    {
        return m_loopValue;
    }
    return m_animation->loopValue();
}

// Override the animation's loop value
void LinearAnimationInstance::loopValue(int value)
{
    if (m_loopValue == value)
    {
        return;
    }
    if (m_loopValue == -1 && m_animation->loopValue() == value)
    {
        return;
    }
    m_loopValue = value;
}

float LinearAnimationInstance::durationSeconds() const { return m_animation->durationSeconds(); }
