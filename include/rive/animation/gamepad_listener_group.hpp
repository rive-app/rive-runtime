#ifndef _RIVE_GAMEPAD_LISTENER_GROUP_HPP_
#define _RIVE_GAMEPAD_LISTENER_GROUP_HPP_

#include "rive/input/gamepad_listener.hpp"

namespace rive
{
class FocusData;
class StateMachineListener;
class StateMachineInstance;

/// Routes gamepad snapshots to a state machine listener on `FocusData`,
/// mirroring `KeyboardListenerGroup`.
class GamepadListenerGroup : public GamepadListener
{
public:
    GamepadListenerGroup(FocusData* focusData,
                         const StateMachineListener* listener,
                         StateMachineInstance* stateMachineInstance);
    ~GamepadListenerGroup() override;

    const StateMachineListener* listener() const { return m_listener; }
    FocusData* focusData() const { return m_focusData; }

    bool gamepadDispatch(
        const ListenerInvocation& invocation,
        ScriptedDrawable** outDispatchedScriptedDrawable = nullptr) override;

private:
    FocusData* m_focusData;
    const StateMachineListener* m_listener;
    StateMachineInstance* m_stateMachineInstance;
};

} // namespace rive

#endif
