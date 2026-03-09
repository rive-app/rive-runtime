#ifndef _RIVE_TEXT_INPUT_LISTENER_GROUP_HPP_
#define _RIVE_TEXT_INPUT_LISTENER_GROUP_HPP_

#include "rive/listener_group.hpp"

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
    TextInput* m_textInput;
    bool m_isDragging = false;
};

} // namespace rive

#endif
