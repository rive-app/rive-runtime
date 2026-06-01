#include "rive/animation/listener_invocation.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/core/binary_reader.hpp"
#include "rive/input/focus_manager.hpp"
#include "rive/input/gamepad_batch.hpp"
#include "rive/input/gamepad_snapshot.hpp"
#include "rive/input/standard_gamepad.hpp"
#include "rive/scripted/scripted_drawable.hpp"
#include "rive/span.hpp"

namespace rive
{

static void recomputeButtonMaskFromValues(GamepadSnapshot& s)
{
    s.buttonMask = 0;
    for (size_t i = 0; i < s.buttonValues.size(); i++)
    {
        if (s.buttonValues[i] >= 0.5f)
        {
            s.buttonMask |= (uint64_t(1) << i);
        }
    }
}

static void fillStandardIntent(const GamepadSnapshot& snap,
                               GamepadEventInvocation& ev)
{
    ev.hasStandardButtonIntent = false;
    ev.hasStandardAxisIntent = false;
    if (snap.mapping != GamepadMappingKind::standard)
    {
        return;
    }
    if (ev.change.kind == GamepadInputChangeKind::button)
    {
        if (ev.change.index <=
            static_cast<uint8_t>(StandardGamepadButton::start))
        {
            ev.hasStandardButtonIntent = true;
            ev.standardButton =
                static_cast<StandardGamepadButton>(ev.change.index);
        }
    }
    else
    {
        if (ev.change.index <=
            static_cast<uint8_t>(StandardGamepadAxis::rightTrigger))
        {
            ev.hasStandardAxisIntent = true;
            ev.standardAxis = static_cast<StandardGamepadAxis>(ev.change.index);
        }
    }
}

static bool readConnectedPayload(BinaryReader& reader,
                                 GamepadSnapshot& outSnapshot)
{
    outSnapshot = GamepadSnapshot{};
    outSnapshot.deviceId = static_cast<int32_t>(reader.readUint32());
    const uint8_t mappingByte = reader.readByte();
    const uint8_t nButtons = reader.readByte();
    const uint8_t nAxes = reader.readByte();
    reader.readByte(); // 1-byte padding to align the float arrays.
    if (reader.hasError() || nButtons > kGamepadBatchMaxButtons ||
        nAxes > kGamepadBatchMaxAxes)
    {
        return false;
    }
    outSnapshot.mapping = mappingByte == 0 ? GamepadMappingKind::standard
                                           : GamepadMappingKind::unknown;
    outSnapshot.buttonValues.resize(nButtons);
    outSnapshot.axes.resize(nAxes);
    for (uint8_t b = 0; b < nButtons; b++)
    {
        outSnapshot.buttonValues[b] = reader.readFloat32();
    }
    for (uint8_t a = 0; a < nAxes; a++)
    {
        outSnapshot.axes[a] = reader.readFloat32();
    }
    if (reader.hasError())
    {
        return false;
    }
    // buttonMask is not on the wire — derive it from the per-button values.
    recomputeButtonMaskFromValues(outSnapshot);
    return true;
}

static bool applyUpdateChange(GamepadSnapshot& snap,
                              const GamepadInputChange& ch)
{
    if (ch.kind == GamepadInputChangeKind::button)
    {
        if (ch.index >= kGamepadBatchMaxButtons)
        {
            return false;
        }
        if (snap.buttonValues.size() <= ch.index)
        {
            snap.buttonValues.resize(ch.index + 1, 0.f);
        }
        snap.buttonValues[ch.index] = ch.value;
        recomputeButtonMaskFromValues(snap);
    }
    else
    {
        if (ch.index >= kGamepadBatchMaxAxes)
        {
            return false;
        }
        if (snap.axes.size() <= ch.index)
        {
            snap.axes.resize(ch.index + 1, 0.f);
        }
        snap.axes[ch.index] = ch.value;
    }
    return true;
}

static GamepadEventInvocation makeEventInvocation(
    const GamepadSnapshot& afterApply,
    const GamepadInputChange& ch)
{
    GamepadEventInvocation ev;
    ev.fullState = afterApply;
    ev.change = ch;
    fillStandardIntent(afterApply, ev);
    return ev;
}

// Decode a little-endian binary batch of gamepad events produced by the
// embedder (e.g. the JS runtime — see registerGamepadInteractions.ts) and
// dispatch them through the focus manager.
//
// Wire format (all multi-byte values little-endian, fixed-width — no LEB128):
//   uint32  version  -- must equal kGamepadBatchWireVersion
//   then a stream of records, each starting with a 1-byte GamepadRecordType:
//
//     connected (0):
//       int32   deviceId
//       uint8   mapping           (0 = standard, else unknown)
//       uint8   nButtons          (<= kGamepadBatchMaxButtons)
//       uint8   nAxes             (<= kGamepadBatchMaxAxes)
//       uint8   padding           (alignment filler)
//       float32 buttonValues[nButtons]
//       float32 axes[nAxes]
//
//     update (1):
//       int32   deviceId
//       uint8   nChanges
//       repeated nChanges times:
//         uint8   kind            (0 = button, 1 = axis)
//         uint8   index
//         float32 value
//
//     disconnected (2):
//       int32   deviceId
//
// Returns false (and stops dispatching) on any malformed record: short read,
// wrong version, unknown device on update, out-of-range button/axis index,
// or unknown record type. A truncated batch leaves any records dispatched so
// far in place.
bool StateMachineInstance::submitGamepadsFromBuffer(const uint8_t* data,
                                                    size_t n)
{
    if (data == nullptr)
    {
        return false;
    }
    BinaryReader reader(Span<const uint8_t>(data, n));
    const uint32_t version = reader.readUint32();
    if (reader.hasError() || version != kGamepadBatchWireVersion)
    {
        return false;
    }
    FocusManager* fm = focusManager();
    if (fm == nullptr)
    {
        return false;
    }
    // reachedEnd() returns true once the cursor hits end-of-buffer OR the
    // reader has flagged an overflow, so any partial-record read inside the
    // loop will both `return false` and naturally terminate iteration.
    while (!reader.reachedEnd())
    {
        const uint8_t typeByte = reader.readByte();
        if (reader.hasError())
        {
            return false;
        }
        const auto rec = static_cast<GamepadRecordType>(typeByte);
        switch (rec)
        {
            case GamepadRecordType::connected:
            {
                GamepadSnapshot snap;
                if (!readConnectedPayload(reader, snap))
                {
                    return false;
                }
                m_embedderGamepads[snap.deviceId] = snap;
                {
                    auto invocation =
                        ListenerInvocation::gamepadConnected(snap);
                    ScriptedDrawable* dispatched = nullptr;
                    (void)fm->gamepadDispatch(invocation, &dispatched);
                    broadcastGamepadToScriptedDrawables(invocation, dispatched);
                }
                break;
            }
            case GamepadRecordType::update:
            {
                const int32_t deviceId =
                    static_cast<int32_t>(reader.readUint32());
                const uint8_t nChanges = reader.readByte();
                if (reader.hasError())
                {
                    return false;
                }
                // Updates must target a gamepad we've previously seen
                // connected; otherwise we have no snapshot to mutate.
                auto it = m_embedderGamepads.find(deviceId);
                if (it == m_embedderGamepads.end())
                {
                    return false;
                }
                std::vector<GamepadInputChange> changes;
                changes.reserve(nChanges);
                for (uint8_t i = 0; i < nChanges; i++)
                {
                    const uint8_t chKindB = reader.readByte();
                    const uint8_t chIndex = reader.readByte();
                    const float fval = reader.readFloat32();
                    if (reader.hasError())
                    {
                        return false;
                    }
                    GamepadInputChange ch;
                    ch.kind = chKindB == 0 ? GamepadInputChangeKind::button
                                           : GamepadInputChangeKind::axis;
                    ch.index = chIndex;
                    ch.value = fval;
                    changes.push_back(ch);
                }
                // Apply all changes to the stored snapshot first so that every
                // dispatched event sees the same fully-updated `fullState`...
                GamepadSnapshot& snap = it->second;
                for (const auto& ch : changes)
                {
                    if (!applyUpdateChange(snap, ch))
                    {
                        return false;
                    }
                }
                // ...then dispatch one event per change carrying that final
                // state plus the specific change that occurred.
                const GamepadSnapshot finalSnap = snap;
                for (const auto& ch : changes)
                {
                    GamepadEventInvocation evi =
                        makeEventInvocation(finalSnap, ch);
                    auto invocation =
                        ListenerInvocation::gamepadEvent(std::move(evi));
                    ScriptedDrawable* dispatched = nullptr;
                    (void)fm->gamepadDispatch(invocation, &dispatched);
                    broadcastGamepadToScriptedDrawables(invocation, dispatched);
                }
                break;
            }
            case GamepadRecordType::disconnected:
            {
                const int32_t deviceId =
                    static_cast<int32_t>(reader.readUint32());
                if (reader.hasError())
                {
                    return false;
                }
                m_embedderGamepads.erase(deviceId);
                {
                    auto invocation =
                        ListenerInvocation::gamepadDisconnected(deviceId);
                    ScriptedDrawable* dispatched = nullptr;
                    (void)fm->gamepadDispatch(invocation, &dispatched);
                    broadcastGamepadToScriptedDrawables(invocation, dispatched);
                }
                break;
            }
            default:
                // Unknown record type — wire format mismatch, bail out.
                return false;
        }
    }
    return true;
}

HitResult StateMachineInstance::broadcastGamepadToScriptedDrawables(
    const ListenerInvocation& invocation,
    ScriptedDrawable* alreadyDispatched)
{
    bool hitSomething = false;
    bool hitOpaque = false;
    // Walk m_hitComponents purely for the nested-propagation overrides
    // (HitNestedArtboard / HitArtboardComponentList recurse into their
    // child SMIs). HitScriptedDrawable's gamepad path is intentionally a
    // no-op — direct dispatch happens via m_gamepadScriptedDrawables below
    // so scripts without pointer handlers are still reached.
    for (const auto& hitShape : m_hitComponents)
    {
        HitResult hitResult =
            hitShape->processGamepadInvocation(invocation, alreadyDispatched);
        if (hitResult != HitResult::none)
        {
            hitSomething = true;
            if (hitResult == HitResult::hitOpaque)
            {
                hitOpaque = true;
            }
        }
    }

#ifdef WITH_RIVE_SCRIPTING
    for (ScriptedDrawable* drawable : m_gamepadScriptedDrawables)
    {
        if (drawable == nullptr || drawable == alreadyDispatched)
        {
            continue;
        }
        switch (invocation.kind())
        {
            case ListenerInvocationKind::gamepadConnected:
                if (!drawable->wantsGamePadConnect())
                {
                    continue;
                }
                break;
            case ListenerInvocationKind::gamepadEvent:
                if (!drawable->wantsGamePadEvent())
                {
                    continue;
                }
                break;
            case ListenerInvocationKind::gamepadDisconnected:
                if (!drawable->wantsGamePadDisconnect())
                {
                    continue;
                }
                break;
            default:
                continue;
        }
        if (drawable->gamepadDispatch(invocation))
        {
            hitSomething = true;
        }
    }
#endif

    return hitSomething ? hitOpaque ? HitResult::hitOpaque : HitResult::hit
                        : HitResult::none;
}
} // namespace rive
