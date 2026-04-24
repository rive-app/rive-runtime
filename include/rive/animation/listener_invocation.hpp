#ifndef _RIVE_LISTENER_INVOCATION_HPP_
#define _RIVE_LISTENER_INVOCATION_HPP_

#include "rive/animation/semantic_listener_group.hpp"
#include "rive/input/focusable.hpp"
#include "rive/listener_type.hpp"
#include "rive/math/vec2d.hpp"

#include <string>
#include <type_traits>
#include <variant>

namespace rive
{

class Event;
class FocusListenerGroup;
class ListenerViewModel;
/// Discriminator for what triggered a listener's actions. Not to be confused
/// with `rive::Event` (file/timeline event objects).
enum class ListenerInvocationKind : uint8_t
{
    pointer = 0,
    keyboard = 1,
    textInput = 2,
    focus = 3,
    reportedEvent = 4,
    viewModelChange = 5,
    none = 6,
    gamepad = 7,
    semantic = 8,
};

struct PointerInvocation
{
    Vec2D position{};
    Vec2D previousPosition{};
    int pointerId = 0;
    ListenerType hitEvent = ListenerType::move;
    float timeStamp = 0.f;
};

struct KeyboardInvocation
{
    Key key = Key::space;
    KeyModifiers modifiers = KeyModifiers::none;
    bool isPressed = false;
    bool isRepeat = false;
};

/// IME / committed text. Owned so `ListenerInvocation` remains valid if
/// retained (e.g. scripted `Invocation` wrappers).
struct TextInputInvocation
{
    std::string text;
};

struct FocusInvocation
{
    FocusListenerGroup* group = nullptr;
    bool isFocus = false;
};

/// A Rive file `Event` reported through the state machine (timeline, etc.).
struct ReportedEventInvocation
{
    Event* reportedEvent = nullptr;
    float delaySeconds = 0.f;
};

struct ViewModelChangeInvocation
{
    ListenerViewModel* source = nullptr;
};

struct NoneInvocation
{};

struct GamepadInvocation
{
    int deviceId = 0;
    uint64_t buttonMask = 0;
    float axis0 = 0.f;
};

struct SemanticInvocation
{
    SemanticListenerGroup* group = nullptr;
    SemanticActionType actionType{};
};

using ListenerInvocationStorage = std::variant<PointerInvocation,
                                               KeyboardInvocation,
                                               TextInputInvocation,
                                               FocusInvocation,
                                               ReportedEventInvocation,
                                               ViewModelChangeInvocation,
                                               NoneInvocation,
                                               GamepadInvocation,
                                               SemanticInvocation>;

/// Payload for a single run of listener actions (pointer, keyboard, reported
/// event, etc.).
class ListenerInvocation
{
public:
    static ListenerInvocation pointer(Vec2D position,
                                      Vec2D previousPosition,
                                      int pointerId,
                                      ListenerType hitEvent,
                                      float timeStamp);
    static ListenerInvocation keyboard(Key key,
                                       KeyModifiers modifiers,
                                       bool isPressed,
                                       bool isRepeat);
    static ListenerInvocation textInput(std::string text);
    static ListenerInvocation focus(FocusListenerGroup* group, bool isFocus);
    static ListenerInvocation reportedEvent(Event* reportedEvent,
                                            float delaySeconds);
    static ListenerInvocation viewModelChange(ListenerViewModel* source);
    static ListenerInvocation none();
    static ListenerInvocation gamepad(int deviceId,
                                      uint64_t buttonMask,
                                      float axis0);
    static ListenerInvocation semantic(SemanticListenerGroup* group,
                                       SemanticActionType actionType);

    ListenerInvocationKind kind() const;

    const PointerInvocation* asPointer() const;
    const KeyboardInvocation* asKeyboard() const;
    const TextInputInvocation* asTextInput() const;
    const FocusInvocation* asFocus() const;
    const ReportedEventInvocation* asReportedEvent() const;
    const ViewModelChangeInvocation* asViewModelChange() const;
    const NoneInvocation* asNone() const;
    const GamepadInvocation* asGamepad() const;
    const SemanticInvocation* asSemantic() const;

    const ListenerInvocationStorage& storage() const { return m_storage; }

private:
    explicit ListenerInvocation(ListenerInvocationStorage&& storage) :
        m_storage(std::move(storage))
    {}

    ListenerInvocationStorage m_storage;
};

inline ListenerInvocationKind ListenerInvocation::kind() const
{
    return std::visit(
        [](const auto& alt) -> ListenerInvocationKind {
            using T = std::decay_t<decltype(alt)>;
            if constexpr (std::is_same_v<T, PointerInvocation>)
            {
                return ListenerInvocationKind::pointer;
            }
            else if constexpr (std::is_same_v<T, KeyboardInvocation>)
            {
                return ListenerInvocationKind::keyboard;
            }
            else if constexpr (std::is_same_v<T, TextInputInvocation>)
            {
                return ListenerInvocationKind::textInput;
            }
            else if constexpr (std::is_same_v<T, FocusInvocation>)
            {
                return ListenerInvocationKind::focus;
            }
            else if constexpr (std::is_same_v<T, ReportedEventInvocation>)
            {
                return ListenerInvocationKind::reportedEvent;
            }
            else if constexpr (std::is_same_v<T, ViewModelChangeInvocation>)
            {
                return ListenerInvocationKind::viewModelChange;
            }
            else if constexpr (std::is_same_v<T, NoneInvocation>)
            {
                return ListenerInvocationKind::none;
            }
            else if constexpr (std::is_same_v<T, GamepadInvocation>)
            {
                return ListenerInvocationKind::gamepad;
            }
            else if constexpr (std::is_same_v<T, SemanticInvocation>)
            {
                return ListenerInvocationKind::semantic;
            }
            else
            {
                return ListenerInvocationKind::none;
            }
        },
        m_storage);
}

inline const PointerInvocation* ListenerInvocation::asPointer() const
{
    return std::get_if<PointerInvocation>(&m_storage);
}

inline const KeyboardInvocation* ListenerInvocation::asKeyboard() const
{
    return std::get_if<KeyboardInvocation>(&m_storage);
}

inline const TextInputInvocation* ListenerInvocation::asTextInput() const
{
    return std::get_if<TextInputInvocation>(&m_storage);
}

inline const FocusInvocation* ListenerInvocation::asFocus() const
{
    return std::get_if<FocusInvocation>(&m_storage);
}

inline const ReportedEventInvocation* ListenerInvocation::asReportedEvent()
    const
{
    return std::get_if<ReportedEventInvocation>(&m_storage);
}

inline const ViewModelChangeInvocation* ListenerInvocation::asViewModelChange()
    const
{
    return std::get_if<ViewModelChangeInvocation>(&m_storage);
}

inline const NoneInvocation* ListenerInvocation::asNone() const
{
    return std::get_if<NoneInvocation>(&m_storage);
}

inline const GamepadInvocation* ListenerInvocation::asGamepad() const
{
    return std::get_if<GamepadInvocation>(&m_storage);
}

inline const SemanticInvocation* ListenerInvocation::asSemantic() const
{
    return std::get_if<SemanticInvocation>(&m_storage);
}

} // namespace rive

#endif
