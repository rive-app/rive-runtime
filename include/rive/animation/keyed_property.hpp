#ifndef _RIVE_KEYED_PROPERTY_HPP_
#define _RIVE_KEYED_PROPERTY_HPP_
#include "rive/generated/animation/keyed_property_base.hpp"
#include <vector>
namespace rive
{
class KeyFrame;
class KeyedCallbackReporter;
class KeyedProperty : public KeyedPropertyBase
{
public:
    KeyedProperty();
    ~KeyedProperty() override;
    void addKeyFrame(std::unique_ptr<KeyFrame>);
    StatusCode onAddedClean(CoreContext* context) override;
    StatusCode onAddedDirty(CoreContext* context) override;

    /// Report any keyframes that occured between secondsFrom and secondsTo.
    void reportKeyedCallbacks(KeyedCallbackReporter* reporter,
                              uint32_t objectId,
                              float secondsFrom,
                              float secondsTo,
                              bool isAtStartFrame) const;

    /// Apply interpolating key frames.
    void apply(Core* object, float time, float mix);

    StatusCode import(ImportStack& importStack) override;
    KeyFrame* first() const
    {
        if (m_keyFrames.size() > 0)
        {
            return m_keyFrames.front().get();
        }
        return nullptr;
    }

private:
    int closestFrameIndex(float seconds, int exactOffset = 0) const;
    std::vector<std::unique_ptr<KeyFrame>> m_keyFrames;
};
} // namespace rive

#endif