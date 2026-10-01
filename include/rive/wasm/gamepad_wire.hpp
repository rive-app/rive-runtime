#ifndef _RIVE_WASM_GAMEPAD_WIRE_HPP_
#define _RIVE_WASM_GAMEPAD_WIRE_HPP_

#include "rive/animation/listener_invocation.hpp"

#include <cstdint>
#include <cstring>
#include <optional>

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

/// Flattens a gamepad invocation into the wire; false for other kinds. The
/// snapshot, null on a disconnect, supplies the floats after the wire.
inline bool packGamepadWire(const ListenerInvocation& invocation,
                            GamepadWire& wire,
                            const GamepadSnapshot*& snapshot)
{
    if (const GamepadConnectedInvocation* c = invocation.asGamepadConnected())
    {
        wire.kind = GamepadWire::kindConnected;
        snapshot = &c->snapshot;
    }
    else if (const GamepadEventInvocation* e = invocation.asGamepadEvent())
    {
        wire.kind = GamepadWire::kindEvent;
        snapshot = &e->fullState;
        wire.changeKind = (uint32_t)e->change.kind;
        wire.changeIndex = e->change.index;
        wire.changeValue = e->change.value;
        wire.hasStandardButtonIntent = e->hasStandardButtonIntent ? 1 : 0;
        wire.standardButton = (uint32_t)e->standardButton;
        wire.hasStandardAxisIntent = e->hasStandardAxisIntent ? 1 : 0;
        wire.standardAxis = (uint32_t)e->standardAxis;
    }
    else if (const GamepadDisconnectedInvocation* d =
                 invocation.asGamepadDisconnected())
    {
        wire.kind = GamepadWire::kindDisconnected;
        wire.deviceId = d->deviceId;
    }
    else
    {
        return false;
    }
    if (snapshot != nullptr)
    {
        wire.deviceId = snapshot->deviceId;
        wire.mapping = (uint32_t)snapshot->mapping;
        wire.buttonMaskLo = (uint32_t)snapshot->buttonMask;
        wire.buttonMaskHi = (uint32_t)(snapshot->buttonMask >> 32);
        wire.buttonCount = (uint32_t)snapshot->buttonValues.size();
        wire.axisCount = (uint32_t)snapshot->axes.size();
    }
    return true;
}

inline uint32_t gamepadPayloadSize(const GamepadWire& wire)
{
    return (uint32_t)(sizeof(wire) +
                      (wire.buttonCount + wire.axisCount) * sizeof(float));
}

inline void writeGamepadPayload(uint8_t* out,
                                const GamepadWire& wire,
                                const GamepadSnapshot* snapshot)
{
    memcpy(out, &wire, sizeof(wire));
    if (snapshot == nullptr)
    {
        return;
    }
    uint8_t* values = out + sizeof(wire);
    if (wire.buttonCount != 0)
    {
        memcpy(values,
               snapshot->buttonValues.data(),
               wire.buttonCount * sizeof(float));
    }
    if (wire.axisCount != 0)
    {
        memcpy(values + wire.buttonCount * sizeof(float),
               snapshot->axes.data(),
               wire.axisCount * sizeof(float));
    }
}

/// False when byteCount cannot hold the wire and the floats it declares.
inline bool decodeGamepadWire(const uint8_t* data,
                              size_t byteCount,
                              GamepadWire& wire)
{
    if (byteCount < sizeof(wire))
    {
        return false;
    }
    memcpy(&wire, data, sizeof(wire));
    uint64_t floatCount = uint64_t(wire.buttonCount) + wire.axisCount;
    return uint64_t(byteCount) >= sizeof(wire) + floatCount * sizeof(float);
}

// The payload may sit at any guest address, so floats are copied, not read.
inline GamepadSnapshot decodeGamepadSnapshot(const GamepadWire& wire,
                                             const uint8_t* data)
{
    GamepadSnapshot snapshot;
    snapshot.deviceId = wire.deviceId;
    snapshot.mapping = (GamepadMappingKind)wire.mapping;
    snapshot.buttonMask =
        (uint64_t(wire.buttonMaskHi) << 32) | wire.buttonMaskLo;
    const uint8_t* values = data + sizeof(wire);
    snapshot.buttonValues.resize(wire.buttonCount);
    snapshot.axes.resize(wire.axisCount);
    if (wire.buttonCount != 0)
    {
        memcpy(snapshot.buttonValues.data(),
               values,
               wire.buttonCount * sizeof(float));
    }
    if (wire.axisCount != 0)
    {
        memcpy(snapshot.axes.data(),
               values + wire.buttonCount * sizeof(float),
               wire.axisCount * sizeof(float));
    }
    return snapshot;
}

inline GamepadEventInvocation decodeGamepadEvent(const GamepadWire& wire,
                                                 const uint8_t* data)
{
    GamepadEventInvocation event;
    event.fullState = decodeGamepadSnapshot(wire, data);
    event.change.kind = (GamepadInputChangeKind)wire.changeKind;
    event.change.index = (uint8_t)wire.changeIndex;
    event.change.value = wire.changeValue;
    event.hasStandardButtonIntent = wire.hasStandardButtonIntent != 0;
    event.standardButton = (StandardGamepadButton)wire.standardButton;
    event.hasStandardAxisIntent = wire.hasStandardAxisIntent != 0;
    event.standardAxis = (StandardGamepadAxis)wire.standardAxis;
    return event;
}

/// The invocation a decoded wire carries; empty for an unknown kind.
inline std::optional<ListenerInvocation> decodeGamepadInvocation(
    const GamepadWire& wire,
    const uint8_t* data)
{
    switch (wire.kind)
    {
        case GamepadWire::kindConnected:
            return ListenerInvocation::gamepadConnected(
                decodeGamepadSnapshot(wire, data));
        case GamepadWire::kindEvent:
            return ListenerInvocation::gamepadEvent(
                decodeGamepadEvent(wire, data));
        case GamepadWire::kindDisconnected:
            return ListenerInvocation::gamepadDisconnected(wire.deviceId);
    }
    return std::nullopt;
}

} // namespace rive

#endif
