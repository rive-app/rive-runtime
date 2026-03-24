#ifndef _RIVE_KEYBOARD_KEY_PHASE_HPP_
#define _RIVE_KEYBOARD_KEY_PHASE_HPP_

#include <cstdint>

namespace rive
{
/// Bitmask for \c KeyboardInputBase::keyPhase() (property key 972).
struct KeyboardKeyPhaseMask
{
    static constexpr uint32_t down = 1u << 0;
    static constexpr uint32_t repeat = 1u << 1;
    static constexpr uint32_t up = 1u << 2;
    static constexpr uint32_t all = down | repeat | up;
};
} // namespace rive

#endif
