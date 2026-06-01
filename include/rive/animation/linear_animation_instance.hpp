#ifndef _RIVE_LINEAR_ANIMATION_INSTANCE_HPP_
#define _RIVE_LINEAR_ANIMATION_INSTANCE_HPP_

#include "rive/artboard.hpp"
#include "rive/core/field_types/core_callback_type.hpp"
#include "rive/nested_animation.hpp"
#include "rive/scene.hpp"

#include <memory>
#include <unordered_map>

namespace rive
{
class LinearAnimation;
class NestedEventNotifier;
class InterpolatingKeyFrame;
class ScriptedInterpolator;
class DataBind;

class LinearAnimationInstance : public Scene, public NestedEventNotifier
{
public:
    LinearAnimationInstance(const LinearAnimation*,
                            ArtboardInstance*,
                            float speedMultiplier = 1.0);
    LinearAnimationInstance(LinearAnimationInstance const&);
    ~LinearAnimationInstance() override;

    // Advance the animation by the specified time. Returns true if the
    // animation will continue to animate after this advance.
    bool advance(float seconds, KeyedCallbackReporter* reporter);
    bool advance(float seconds) { return advance(seconds, nullptr); }

    void clearSpilledTime() { m_spilledTime = 0; }

    // Returns a pointer to the instance's animation
    const LinearAnimation* animation() const { return m_animation; }

    // Returns the current point in time at which this instance has advance
    // to
    float time() const { return m_time; }

    // Returns the direction that we are currently playing in
    float direction() const { return m_direction; }

    // Returns speed in the current direction of the animation.
    float directedSpeed() const { return m_direction * speed(); }

    // Update the direction of the animation instance, positive value for
    // forwards Negative for backwards
    void direction(int direction)
    {
        if (direction > 0)
        {
            m_direction = 1;
        }
        else
        {
            m_direction = -1;
        }
    }

    // Sets the animation's point in time.
    void time(float value);

    // Applies the animation instance to its artboard instance. The mix (a value
    // between 0 and 1) is the strength at which the animation is mixed with
    // other animations applied to the artboard.
    void apply(float mix = 1.0f) const
    {
        m_animation->apply(m_artboardInstance, m_time, mix, this);
    }

    // Returns a per-(this LAI, keyframe) stateful clone of the given
    // ScriptedInterpolator, lazily creating it on first call. Returns nullptr
    // when scripting is disabled or the clone cannot be vended; callers
    // should fall back to the shared template (which already degrades to
    // identity/linear when its Lua side isn't ready). Time-seeking via
    // `time(float)` and `reset()` intentionally does NOT clear the cache —
    // statefulness is the whole point.
    ScriptedInterpolator* statefulInterpolator(
        const InterpolatingKeyFrame* keyframe,
        const ScriptedInterpolator* shared) const;

    // Set when the animation is advanced, true if the animation has stopped
    // (oneShot), reached the end (loop), or changed direction (pingPong)
    bool didLoop() const { return m_didLoop; }

    bool keepGoing() const
    {
        return this->loopValue() != static_cast<int>(rive::Loop::oneShot) ||
               (directedSpeed() > 0 && m_time < m_animation->endSeconds()) ||
               (directedSpeed() < 0 && m_time > m_animation->startSeconds());
    }

    bool keepGoing(float speedMultiplier) const
    {
        return this->loopValue() != static_cast<int>(rive::Loop::oneShot) ||
               (directedSpeed() * speedMultiplier > 0 &&
                m_time < m_animation->endSeconds()) ||
               (directedSpeed() * speedMultiplier < 0 &&
                m_time > m_animation->startSeconds());
    }

    float totalTime() const { return m_totalTime; }
    float lastTotalTime() const { return m_lastTotalTime; }
    float spilledTime() const { return m_spilledTime; }
    float durationSeconds() const override;

    // Forwarded from animation
    uint32_t fps() const;
    uint32_t duration() const;
    float speed() const;
    float startTime() const;

    // Returns either the animation's default or overridden loop values
    Loop loop() const override { return (Loop)loopValue(); }
    int loopValue() const;
    // Override the animation's default loop
    void loopValue(int value);

    bool isTranslucent() const override;
    bool advanceAndApply(float seconds) override;
    std::string name() const override;
    void reset(float speedMultiplier);
    void reportEvent(Event* event, float secondsDelay = 0.0f) override;

private:
    const LinearAnimation* m_animation = nullptr;
    float m_time;
    float m_speedDirection;
    float m_totalTime;
    float m_lastTotalTime;
    float m_spilledTime;

    // float because it gets multiplied with other floats
    float m_direction;
    bool m_didLoop;
    int m_loopValue = -1;

    // Lazy outer pointer => the common case (no scripted interpolators) pays
    // one nullptr check on the apply hot path. Inner unique_ptr destroys the
    // cloned ScriptedInterpolator (and its Lua ref) when this LAI is
    // destroyed. `mutable` so the cache populates from the const apply().
    // Intentionally not copied by the copy ctor — a copied LAI starts with
    // a fresh empty cache.
    mutable std::unique_ptr<
        std::unordered_map<const InterpolatingKeyFrame*,
                           std::unique_ptr<ScriptedInterpolator>>>
        m_scriptedInterpolatorInstances;

    // Data binds that cloneProperties() appended to m_artboardInstance on
    // our behalf for the cloned ScriptedInterpolators above. We must
    // removeDataBind+delete each of these in ~LinearAnimationInstance BEFORE
    // m_scriptedInterpolatorInstances tears down, because the bind targets
    // point at CustomPropertys owned by the clones. Captured by snapshotting
    // m_artboardInstance->dataBinds().size() before/after each cloneScripted-
    // Object call (addDataBind only ever appends). `mutable` so it can
    // populate from the const apply() path. Not copied by the copy ctor.
    mutable std::vector<DataBind*> m_clonedArtboardDataBinds;
};
} // namespace rive
#endif
