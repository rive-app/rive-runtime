#ifndef _RIVE_FOCUS_LISTENER_HPP_
#define _RIVE_FOCUS_LISTENER_HPP_

namespace rive
{

/// Interface for objects that want to be notified of focus changes on a
/// FocusData.
class FocusListener
{
public:
    virtual ~FocusListener() = default;

    /// Called when the associated FocusData gains focus.
    virtual void onFocused() = 0;

    /// Called when the associated FocusData loses focus.
    virtual void onBlurred() = 0;
};

} // namespace rive

#endif
