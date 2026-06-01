#ifndef _RIVE_GAMEPAD_LISTENER_HPP_
#define _RIVE_GAMEPAD_LISTENER_HPP_

namespace rive
{

class ListenerInvocation;
class ScriptedDrawable;

/// Interface for objects notified of gamepad snapshots on a `FocusData`.
class GamepadListener
{
public:
    virtual ~GamepadListener() = default;

    /// Called when the associated `FocusData` receives a gamepad event.
    /// Return true to stop bubbling to ancestor focus nodes (optional).
    /// `outDispatchedScriptedDrawable` (when non-null) is filled with the
    /// `ScriptedDrawable` that ended up receiving the event so callers can
    /// avoid re-dispatching to it during a broadcast pass.
    virtual bool gamepadDispatch(
        const ListenerInvocation& invocation,
        ScriptedDrawable** outDispatchedScriptedDrawable = nullptr) = 0;
};

} // namespace rive

#endif
