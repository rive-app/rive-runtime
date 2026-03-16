#ifndef _RIVE_FOCUS_LISTENER_GROUP_HPP_
#define _RIVE_FOCUS_LISTENER_GROUP_HPP_

#include "rive/input/focus_listener.hpp"
#include "rive/listener_type.hpp"

namespace rive
{
class FocusData;
class StateMachineListener;
class StateMachineInstance;

/// A FocusListenerGroup manages a focus or blur listener that's attached
/// to a FocusData. When the FocusData gains or loses focus, the listener's
/// actions are queued for deferred execution.
class FocusListenerGroup : public FocusListener
{
public:
    FocusListenerGroup(FocusData* focusData,
                       const StateMachineListener* listener,
                       StateMachineInstance* stateMachineInstance);
    ~FocusListenerGroup() override;

    /// Get the listener this group is managing.
    const StateMachineListener* listener() const { return m_listener; }

    /// Get the FocusData this group is attached to.
    FocusData* focusData() const { return m_focusData; }

    /// Check if this is a focus listener (vs blur listener).
    bool isFocusListener() const { return m_isFocusListener; }

    /// Check if this is a blur listener (vs focus listener).
    bool isBlurListener() const { return m_isBlurListener; }

    // FocusListener interface
    void onFocused() override;
    void onBlurred() override;

private:
    FocusData* m_focusData;
    const StateMachineListener* m_listener;
    StateMachineInstance* m_stateMachineInstance;
    bool m_isFocusListener;
    bool m_isBlurListener;
};

} // namespace rive

#endif
