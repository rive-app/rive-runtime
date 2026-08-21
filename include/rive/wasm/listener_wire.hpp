#ifndef _RIVE_WASM_LISTENER_WIRE_HPP_
#define _RIVE_WASM_LISTENER_WIRE_HPP_

#include <cstdint>

namespace rive
{

/// Host to module layout for callListenerPerform's invocation payload. Four
/// byte fields only, so the struct is identical on both sides of the wasm
/// boundary. kind is the ListenerInvocationKind value; the fields the kind
/// does not use stay zero. textInput appends textLength string bytes after
/// the struct; the gamepad kinds append a GamepadWire plus its floats.
struct ListenerWire
{
    uint32_t kind = 0;
    float posX = 0;
    float posY = 0;
    float prevX = 0;
    float prevY = 0;
    int32_t pointerId = 0;
    uint32_t hitEvent = 0;
    float timeStamp = 0;
    uint32_t key = 0;
    uint32_t modifiers = 0;
    uint32_t isPressed = 0;
    uint32_t isRepeat = 0;
    uint32_t isFocus = 0;
    float delaySeconds = 0;
    uint32_t semanticAction = 0;
    uint32_t textLength = 0;
};

} // namespace rive

#endif
