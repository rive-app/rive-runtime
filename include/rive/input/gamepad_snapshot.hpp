#ifndef _RIVE_GAMEPAD_SNAPSHOT_HPP_
#define _RIVE_GAMEPAD_SNAPSHOT_HPP_

#include <cstddef>
#include <cstdint>
#include <vector>

namespace rive
{

/// A single field change in a gamepad "update" wire message or event.
enum class GamepadInputChangeKind : uint8_t
{
    button = 0,
    axis = 1,
};

struct GamepadInputChange
{
    GamepadInputChangeKind kind = GamepadInputChangeKind::button;
    /// W3C button or axis index (0-based).
    uint8_t index = 0;
    /// `GamepadButton.value` (0-1) or axis analog value.
    float value = 0.f;
};

/// How closely the embedder could map hardware to the W3C Standard Gamepad
/// layout.
enum class GamepadMappingKind : uint8_t
{
    /// Buttons and axes follow https://w3c.github.io/gamepad/#remapping
    standard = 0,
    /// Non-standard device or unmapped — semantic filters may be unreliable.
    unknown = 1,
};

/// Neutral gamepad snapshot in `ListenerInvocation` and embedder state.
///
/// - `deviceId`: embedder-defined stable id for the logical device session
///   (hot-plug may allocate a new id; document embedder policy alongside the
///   integration).
/// - `buttonMask`: bit \e i set when W3C standard gamepad button \e i is
/// pressed
///   (see https://w3c.github.io/gamepad/#remapping ).
/// - `buttonValues`: parallel to W3C `buttons[]` order, each entry is
///   `GamepadButton.value` (0–1), including analog triggers and pressure.
/// - `axes`: ordered analog values; length is embedder-defined (W3C standard
///   layout uses at least six: left stick, right stick, then left/right
///   triggers). Typical ranges: sticks [-1,1], triggers [0,1].
/// - `mapping`: whether the embedder normalized to the standard layout; when
///   `unknown`, semantic helpers that map names to indices may not apply.
struct GamepadSnapshot
{
    int deviceId = 0;
    uint64_t buttonMask = 0;
    std::vector<float> buttonValues;
    std::vector<float> axes;
    GamepadMappingKind mapping = GamepadMappingKind::standard;
};

} // namespace rive

#endif
