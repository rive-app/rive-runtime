#ifndef _RIVE_LINEAR_ANIMATION_INSTANCE_HPP_
#define _RIVE_LINEAR_ANIMATION_INSTANCE_HPP_

#include "rive/artboard.hpp"
#include "rive/core/field_types/core_callback_type.hpp"
#include "rive/nested_animation.hpp"
#include "rive/scene.hpp"

namespace rive
{
class LinearAnimation;
class NestedEventNotifier;

class LinearAnimationInstance : public Scene, public NestedEventNotifier
{
public:
    LinearAnimationInstance(const LinearAnimation*, ArtboardInstance*, float speedMultiplier = 1.0);
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
    void apply(float mix = 1.0f) const { m_animation->apply(m_artboardInstance, m_time, mix); }

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
               (directedSpeed() * speedMultiplier > 0 && m_time < m_animation->endSeconds()) ||
               (directedSpeed() * speedMultiplier < 0 && m_time > m_animation->startSeconds());
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
};
} // namespace rive
#endif
