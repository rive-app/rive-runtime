#ifndef _RIVE_GAMEPAD_BUTTON_PHASE_HPP_
#define _RIVE_GAMEPAD_BUTTON_PHASE_HPP_

#include <cstdint>

namespace rive
{
/// Bitmask for \c GamepadInputBase::buttonPhase() — which button transitions a
/// gamepad listener input matches. Only meaningful when the input's `kind` is
/// `GamepadInputKind::button`.
struct GamepadButtonPhaseMask
{
    /// Button transitioned to pressed (value >= 0.5).
    static constexpr uint32_t down = 1u << 0;
    /// Button transitioned to released (value < 0.5).
    static constexpr uint32_t up = 1u << 1;
    static constexpr uint32_t all = down | up;
};
} // namespace rive

#endif
