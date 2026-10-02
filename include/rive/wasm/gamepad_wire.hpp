#ifndef _RIVE_WASM_GAMEPAD_WIRE_HPP_
#define _RIVE_WASM_GAMEPAD_WIRE_HPP_

#include <cstdint>

namespace rive
{

/// Host to module layout for callGamepadEvent's invocation payload. Four byte
/// fields only, so the struct is identical on both sides of the wasm
/// boundary; buttonValues then axes follow as floats.
struct GamepadWire
{
    static constexpr uint32_t kindConnected = 0;
    static constexpr uint32_t kindEvent = 1;
    static constexpr uint32_t kindDisconnected = 2;

    uint32_t kind = 0;
    int32_t deviceId = 0;
    uint32_t mapping = 0;
    uint32_t buttonMaskLo = 0;
    uint32_t buttonMaskHi = 0;
    uint32_t changeKind = 0;
    uint32_t changeIndex = 0;
    float changeValue = 0;
    uint32_t hasStandardButtonIntent = 0;
    uint32_t standardButton = 0;
    uint32_t hasStandardAxisIntent = 0;
    uint32_t standardAxis = 0;
    uint32_t buttonCount = 0;
    uint32_t axisCount = 0;
};

} // namespace rive

#endif
