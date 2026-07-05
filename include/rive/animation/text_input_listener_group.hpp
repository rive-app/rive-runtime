#ifndef _RIVE_TEXT_INPUT_LISTENER_GROUP_HPP_
#define _RIVE_TEXT_INPUT_LISTENER_GROUP_HPP_

#include "rive/listener_group.hpp"
#include "rive/math/vec2d.hpp"

namespace rive
{
class TextInput;
class StateMachineInstance;

/// A TextInputListenerGroup handles pointer events on TextInput components,
/// managing drag-to-select text behavior.
class TextInputListenerGroup : public ListenerGroup
{
public:
    TextInputListenerGroup(TextInput* textInput,
                           StateMachineInstance* stateMachineInstance);
    ~TextInputListenerGroup() override = default;

    bool canEarlyOut(Component* drawable) override { return false; }
    bool needsDownListener(Component* drawable) override { return true; }
    bool needsUpListener(Component* drawable) override { return true; }

    ProcessEventResult processEvent(
        Component* component,
        Vec2D position,
        int pointerId,
        ListenerType hitEvent,
        bool canHit,
        float timeStamp,
        StateMachineInstance* stateMachineInstance) override;

private:
    // Returns a timestamp in microseconds for multi-click detection. Uses the
    // passed-in (deterministic) timeStamp when File::deterministicMode is set,
    // otherwise wall-clock time, mirroring ScrollPhysics.
    long long nowMicros(float timeStamp) const;

    TextInput* m_textInput;
    bool m_isDragging = false;

    // Multi-click (double/triple-click) detection state.
    int m_clickCount = 0;
    long long m_lastClickTime = 0; // microseconds
    Vec2D m_lastClickPosition = Vec2D(0, 0);
    static constexpr long long kMultiClickIntervalUs = 500000; // 0.5s
    static constexpr float kMultiClickDistance = 16.0f;        // world units
};

} // namespace rive

#endif
