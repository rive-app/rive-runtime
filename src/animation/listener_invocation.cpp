#include "rive/animation/listener_invocation.hpp"
#include "rive/animation/semantic_listener_group.hpp"

#include <string>
#include <utility>

using namespace rive;

ListenerInvocation ListenerInvocation::pointer(Vec2D position,
                                               Vec2D previousPosition,
                                               int pointerId,
                                               ListenerType hitEvent,
                                               float timeStamp)
{
    PointerInvocation p;
    p.position = position;
    p.previousPosition = previousPosition;
    p.pointerId = pointerId;
    p.hitEvent = hitEvent;
    p.timeStamp = timeStamp;
    return ListenerInvocation(ListenerInvocationStorage(std::move(p)));
}

ListenerInvocation ListenerInvocation::keyboard(Key key,
                                                KeyModifiers modifiers,
                                                bool isPressed,
                                                bool isRepeat)
{
    KeyboardInvocation k;
    k.key = key;
    k.modifiers = modifiers;
    k.isPressed = isPressed;
    k.isRepeat = isRepeat;
    return ListenerInvocation(ListenerInvocationStorage(std::move(k)));
}

ListenerInvocation ListenerInvocation::textInput(std::string text)
{
    return ListenerInvocation(
        ListenerInvocationStorage(TextInputInvocation{std::move(text)}));
}

ListenerInvocation ListenerInvocation::focus(FocusListenerGroup* group,
                                             bool isFocus)
{
    FocusInvocation f;
    f.group = group;
    f.isFocus = isFocus;
    return ListenerInvocation(ListenerInvocationStorage(std::move(f)));
}

ListenerInvocation ListenerInvocation::reportedEvent(Event* reportedEvent,
                                                     float delaySeconds)
{
    ReportedEventInvocation e;
    e.reportedEvent = reportedEvent;
    e.delaySeconds = delaySeconds;
    return ListenerInvocation(ListenerInvocationStorage(std::move(e)));
}

ListenerInvocation ListenerInvocation::viewModelChange(
    ListenerViewModel* source)
{
    ViewModelChangeInvocation v;
    v.source = source;
    return ListenerInvocation(ListenerInvocationStorage(std::move(v)));
}

ListenerInvocation ListenerInvocation::none()
{
    return ListenerInvocation(ListenerInvocationStorage(NoneInvocation{}));
}

ListenerInvocation ListenerInvocation::gamepadConnected(
    const GamepadSnapshot& snapshot)
{
    GamepadConnectedInvocation c;
    c.snapshot = snapshot;
    return ListenerInvocation(ListenerInvocationStorage(std::move(c)));
}

ListenerInvocation ListenerInvocation::gamepadEvent(
    GamepadEventInvocation value)
{
    return ListenerInvocation(ListenerInvocationStorage(std::move(value)));
}

ListenerInvocation ListenerInvocation::gamepadDisconnected(int deviceId)
{
    GamepadDisconnectedInvocation d;
    d.deviceId = deviceId;
    return ListenerInvocation(ListenerInvocationStorage(std::move(d)));
}

ListenerInvocation ListenerInvocation::semantic(SemanticListenerGroup* group,
                                                SemanticActionType actionType)
{
    SemanticInvocation s;
    s.group = group;
    s.actionType = actionType;
    return ListenerInvocation(ListenerInvocationStorage(std::move(s)));
}
