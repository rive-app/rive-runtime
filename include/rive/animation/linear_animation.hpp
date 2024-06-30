#ifndef _RIVE_LINEAR_ANIMATION_HPP_
#define _RIVE_LINEAR_ANIMATION_HPP_
#include "rive/animation/loop.hpp"
#include "rive/generated/animation/linear_animation_base.hpp"
#include <vector>
namespace rive
{
class Artboard;
class KeyedObject;
class KeyedCallbackReporter;

class LinearAnimation : public LinearAnimationBase
{
private:
    std::vector<std::unique_ptr<KeyedObject>> m_KeyedObjects;

    friend class Artboard;

public:
    LinearAnimation();
    ~LinearAnimation() override;
    StatusCode onAddedDirty(CoreContext* context) override;
    StatusCode onAddedClean(CoreContext* context) override;
    void addKeyedObject(std::unique_ptr<KeyedObject>);
    void apply(Artboard* artboard, float time, float mix = 1.0f) const;

    Loop loop() const { return (Loop)loopValue(); }

    StatusCode import(ImportStack& importStack) override;

    float durationSeconds() const;
    /// Returns the start time/ end time of the animation in seconds
    float startSeconds() const;
    float endSeconds() const;

    /// Returns the start time/ end time of the animation in seconds, considering speed
    float startTime() const;
    float startTime(float multiplier) const;
    float endTime() const;

    /// Convert a global clock to local seconds (takes into consideration
    /// work area start/end, speed, looping).
    float globalToLocalSeconds(float seconds) const;

    const KeyedObject* getObject(size_t index) const
    {
        if (index < m_KeyedObjects.size())
        {
            return m_KeyedObjects[index].get();
        }
        else
        {
            return nullptr;
        }
    }

    size_t numKeyedObjects() const { return m_KeyedObjects.size(); }

#ifdef TESTING
    // Used in testing to check how many animations gets deleted.
    static int deleteCount;
#endif

    void reportKeyedCallbacks(KeyedCallbackReporter* reporter,
                              float secondsFrom,
                              float secondsTo,
                              float speedDirection,
                              bool fromPong) const;
};
} // namespace rive

#endif
