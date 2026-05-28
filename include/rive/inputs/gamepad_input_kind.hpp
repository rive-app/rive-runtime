#ifndef _RIVE_GAMEPAD_INPUT_KIND_HPP_
#define _RIVE_GAMEPAD_INPUT_KIND_HPP_

#include <cstdint>

namespace rive
{
/// Values for \c GamepadInputBase::kind() — which gamepad event kind a single
/// `GamepadInput` filters. `button` / `axis` mirror `GamepadInputChangeKind`
/// (see \c gamepad_snapshot.hpp) so the value can be compared directly against
/// `GamepadEventInvocation::change.kind`.
enum class GamepadInputKind : uint32_t
{
    button = 0,
    axis = 1,
    connected = 2,
    disconnected = 3,
};

/// Values for \c GamepadInputBase::mapping() — how `inputIndex` is interpreted
/// for `button` / `axis` kinds.
enum class GamepadInputMapping : uint32_t
{
    /// `inputIndex` is a W3C standard slot (matches the corresponding
    /// `StandardGamepadButton` / `StandardGamepadAxis` enum value).
    standard = 0,
    /// `inputIndex` is the raw W3C index into the buttons/axes arrays.
    index = 1,
};
} // namespace rive

#endif
