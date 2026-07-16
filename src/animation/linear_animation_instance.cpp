#include "rive/animation/linear_animation_instance.hpp"
#include "rive/animation/interpolating_keyframe.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/animation/loop.hpp"
#include "rive/animation/keyed_callback_reporter.hpp"
#include "rive/assets/script_asset.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/data_bind/bindable_property.hpp"
#include "rive/profiler/profiler_macros.h"
#include "rive/scripted/scripted_interpolator.hpp"

#include <cmath>
#include <cassert>

using namespace rive;

LinearAnimationInstance::LinearAnimationInstance(
    const LinearAnimation* animation,
    ArtboardInstance* instance,
    float speedMultiplier) :
    Scene(instance),
    m_animation((assert(animation != nullptr), animation)),
    m_time((speedMultiplier >= 0) ? animation->startTime()
                                  : animation->endTime()),
    m_speedDirection((speedMultiplier >= 0) ? 1 : -1),
    m_totalTime(0.0f),
    m_lastTotalTime(0.0f),
    m_spilledTime(0.0f),
    m_direction(1)
{}

LinearAnimationInstance::LinearAnimationInstance(
    LinearAnimationInstance const& lhs) :
    Scene(lhs),
    m_animation(lhs.m_animation),
    m_time(lhs.m_time),
    m_speedDirection(lhs.m_speedDirection),
    m_totalTime(lhs.m_totalTime),
    m_lastTotalTime(lhs.m_lastTotalTime),
    m_spilledTime(lhs.m_spilledTime),
    m_direction(lhs.m_direction),
    m_didLoop(lhs.m_didLoop),
    m_loopValue(lhs.m_loopValue)
{}

LinearAnimationInstance::~LinearAnimationInstance()
{
    // Critical teardown order, mirroring the SMI pattern at
    // state_machine_instance.cpp:2011-2044: pull cloned data binds out of
    // the artboard and delete them BEFORE m_scriptedInterpolatorInstances
    // destroys the clones whose CustomPropertys are those binds' targets.
    // Without this, the next Artboard::updateDataBinds() (which runs every
    // frame from updatePass) reads through DataBind::target() into freed
    // memory.
    if (m_artboardInstance != nullptr)
    {
        for (auto* bind : m_clonedArtboardDataBinds)
        {
            m_artboardInstance->removeDataBind(bind);
            delete bind;
        }
    }
    m_clonedArtboardDataBinds.clear();
    // m_scriptedInterpolatorInstances destructs here via unique_ptr; safe now
    // that no DataBind still points at the clones' CustomPropertys.

    // Keyframe value holders are owned here. The StateMachineInstance has
    // already removed+deleted the data binds that target them (on
    // state-instance removal, or via deleteDataBinds() before the layers in its
    // destructor), so deleting the holders now is safe. The unique_ptr frees
    // the map itself.
    if (m_keyFrameValueHolders != nullptr)
    {
        for (auto& pair : *m_keyFrameValueHolders)
        {
            delete pair.second;
        }
    }
}

void LinearAnimationInstance::addKeyFrameValueHolder(const KeyFrame* keyframe,
                                                     BindableProperty* holder)
{
    if (m_keyFrameValueHolders == nullptr)
    {
        m_keyFrameValueHolders = std::make_unique<
            std::unordered_map<const KeyFrame*, BindableProperty*>>();
    }
    (*m_keyFrameValueHolders)[keyframe] = holder;
}

BindableProperty* LinearAnimationInstance::keyFrameValueHolder(
    const KeyFrame* keyframe) const
{
    if (m_keyFrameValueHolders == nullptr)
    {
        return nullptr;
    }
    auto it = m_keyFrameValueHolders->find(keyframe);
    return it != m_keyFrameValueHolders->end() ? it->second : nullptr;
}

// Returns a per-(this LAI, keyframe) stateful clone of the given shared
// ScriptedInterpolator. Mirrors StateMachineInstance::scriptedObjects but
// keyed by the keyframe pointer so distinct keyframes that share a script
// each get their own Lua `self` table. Lazily allocates the map and the
// clone on first hit; subsequent calls return the cached clone. Cleanup
// runs in ~LinearAnimationInstance via unique_ptr.
ScriptedInterpolator* LinearAnimationInstance::statefulInterpolator(
    const InterpolatingKeyFrame* keyframe,
    const ScriptedInterpolator* shared) const
{
    if (shared == nullptr || keyframe == nullptr)
    {
        return nullptr;
    }
    if (m_scriptedInterpolatorInstances == nullptr)
    {
        m_scriptedInterpolatorInstances = std::make_unique<
            std::unordered_map<const InterpolatingKeyFrame*,
                               std::unique_ptr<ScriptedInterpolator>>>();
    }
    auto& map = *m_scriptedInterpolatorInstances;
    auto it = map.find(keyframe);
    if (it != map.end())
    {
        return it->second.get();
    }

    // cloneScriptedObject is non-const on the source; the shared template is
    // logically read-only here so the const_cast is safe and matches the
    // ScriptedListenerAction pattern.
    auto* cloneAsObj =
        const_cast<ScriptedInterpolator*>(shared)->cloneScriptedObject(
            m_artboardInstance);
    if (cloneAsObj == nullptr)
    {
        return nullptr;
    }
    auto* clone = static_cast<ScriptedInterpolator*>(cloneAsObj);
    clone->dataContext(m_artboardInstance->dataContext());

    // Walk the clone's ScriptInputs to discover the binds cloneProperties
    // just parked on the artboard for this clone. The back-pointer is set
    // inside cloneProperties (scripted_object.cpp) immediately after
    // dataBindContainer->addDataBind(...). We track these so ~LAI can
    // removeDataBind + delete each one BEFORE the clone — whose
    // CustomProperty is the bind's target — is destroyed.
    for (auto* prop : clone->customProperties())
    {
        if (auto* input = ScriptInput::from(prop))
        {
            if (auto* bind = input->dataBind())
            {
                m_clonedArtboardDataBinds.push_back(bind);
            }
        }
    }

    // Late-binding safety net: reinit() inside cloneScriptedObject is a no-op
    // when scriptAsset() is null at clone time. If the asset became available
    // later (editor live-edit), init now. ensureScriptInitialized
    // short-circuits when already initialized so this stays cheap.
    if (clone->scriptAsset() != nullptr && !clone->userLuaInitDone())
    {
        clone->scriptAsset()->initScriptedObject(clone);
        clone->hydrateScriptInputs();
    }

    auto* raw = clone;
    map.emplace(keyframe, std::unique_ptr<ScriptedInterpolator>(clone));
    return raw;
}

bool LinearAnimationInstance::advanceAndApply(float seconds)
{
    RIVE_PROF_SCOPE_L(1)
    bool more = this->advance(seconds, this);
    this->apply();
    if (m_artboardInstance->advance(seconds))
    {
        more = true;
    }
    return more || keepGoing();
}

bool LinearAnimationInstance::advance(float elapsedSeconds,
                                      KeyedCallbackReporter* reporter)
{
    const LinearAnimation& animation = *m_animation;
    float deltaSeconds = elapsedSeconds * animation.speed() * m_direction;

    m_spilledTime = 0.0f;
    if (deltaSeconds == 0)
    {
        m_didLoop = false;
        return false;
    }

    m_lastTotalTime = m_totalTime;
    m_totalTime += std::abs(deltaSeconds);

    // NOTE:
    // do not track spilled time, if our one shot loop is already completed.
    // stop gap before we move spilled tracking into state machine logic.
    bool killSpilledTime = !this->keepGoing(elapsedSeconds);

    float lastTime = m_time;
    m_time += deltaSeconds;
    if (reporter != nullptr)
    {
        animation.reportKeyedCallbacks(reporter,
                                       lastTime,
                                       m_time,
                                       m_speedDirection,
                                       false);
    }

    float fps = (float)animation.fps();
    float frames = m_time * fps;
    float start =
        animation.enableWorkArea() ? (float)animation.workStart() : 0.0f;
    float end = animation.enableWorkArea() ? (float)animation.workEnd()
                                           : (float)animation.duration();
    float range = end - start;

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
                // Account for the time dilation or contraction applied in the
                // animation local time by its speed to calculate spilled time.
                // Calculate the ratio of the time excess by the total elapsed
                // time in local time (deltaFrames) and multiply the elapsed
                // time by it.
                auto deltaFrames = deltaSeconds * fps;
                auto spilledFramesRatio = (frames - end) / deltaFrames;
                m_spilledTime = spilledFramesRatio * elapsedSeconds;
                frames = (float)end;
                m_time = frames / fps;
                didLoop = true;
            }
            else if (direction == -1 && frames < start)
            {
                auto deltaFrames = std::abs(deltaSeconds * fps);
                auto spilledFramesRatio = (start - frames) / deltaFrames;
                m_spilledTime = spilledFramesRatio * elapsedSeconds;
                frames = (float)start;
                m_time = frames / fps;
                didLoop = true;
            }
            break;
        case Loop::loop:
            if (direction == 1 && frames >= end)
            {
                // How spilled time has to be calculated, given that local time
                // can be scaled to a factor of the regular time:
                // - for convenience, calculate the local elapsed time in frames
                // (deltaFrames)
                // - get the remainder of current frame position (frames) by
                // duration (range)
                // - use that remainder as the ratio of the original time that
                // was not consumed by the loop (spilledFramesRatio)
                // - multiply the original elapsedTime by the ratio to set the
                // spilled time
                auto deltaFrames = deltaSeconds * fps;
                auto remainder = std::fmod(frames - start, (float)range);
                auto spilledFramesRatio = remainder / deltaFrames;
                m_spilledTime = spilledFramesRatio * elapsedSeconds;
                frames = start + remainder;
                m_time = frames / fps;
                didLoop = true;
                if (reporter != nullptr)
                {
                    animation.reportKeyedCallbacks(reporter,
                                                   0.0f,
                                                   m_time,
                                                   m_speedDirection,
                                                   false);
                }
            }
            else if (direction == -1 && frames <= start)
            {
                auto deltaFrames = deltaSeconds * fps;
                auto remainder =
                    std::abs(std::fmod(start - frames, (float)range));
                auto spilledFramesRatio = std::abs(remainder / deltaFrames);
                m_spilledTime = spilledFramesRatio * elapsedSeconds;
                frames = end - remainder;
                m_time = frames / fps;
                didLoop = true;
                if (reporter != nullptr)
                {
                    animation.reportKeyedCallbacks(reporter,
                                                   end / (float)fps,
                                                   m_time,
                                                   m_speedDirection,
                                                   false);
                }
            }
            break;
        case Loop::pingPong:
            bool fromPong = true;
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
                    animation.reportKeyedCallbacks(reporter,
                                                   lastTime,
                                                   m_time,
                                                   m_speedDirection,
                                                   fromPong);
                }
                fromPong = !fromPong;
            }
            break;
    }

    if (killSpilledTime)
    {
        m_spilledTime = 0;
    }

    m_didLoop = didLoop;
    return this->keepGoing(elapsedSeconds);
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

    float start =
        (m_animation->enableWorkArea() ? (float)m_animation->workStart()
                                       : 0.0f) *
        (float)m_animation->fps();
    m_totalTime = value - start;
    m_lastTotalTime = m_totalTime - diff;

    // leaving this RIGHT now. but is this required? it kinda messes up
    // playing things backwards and seeking. what purpose does it solve?
    m_direction = 1;
}

void LinearAnimationInstance::reset(float speedMultiplier = 1.0)
{
    m_time = (speedMultiplier >= 0) ? m_animation->startTime()
                                    : m_animation->endTime();
}

uint32_t LinearAnimationInstance::fps() const { return m_animation->fps(); }

uint32_t LinearAnimationInstance::duration() const
{
    return m_animation->duration();
}

float LinearAnimationInstance::speed() const { return m_animation->speed(); }

float LinearAnimationInstance::startTime() const
{
    return m_animation->startTime();
}

std::string LinearAnimationInstance::name() const
{
    return m_animation->name();
}

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

float LinearAnimationInstance::durationSeconds() const
{
    return m_animation->durationSeconds();
}

void LinearAnimationInstance::reportEvent(Event* event, float secondsDelay)
{
    const std::vector<Event*> events{event};
    notifyListeners(events);
}