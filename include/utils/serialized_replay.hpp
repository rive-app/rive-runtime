/*
 * Copyright 2026 Rive
 */

#ifndef _RIVE_SERIALIZED_REPLAY_HPP_
#define _RIVE_SERIALIZED_REPLAY_HPP_

#include "rive/factory.hpp"
#include "rive/renderer.hpp"
#include "rive/span.hpp"
#include <cstdint>
#include <functional>

// Replays a SerializingFactory SRIV stream against a real Factory and
// Renderer. The stream records at the Factory and Renderer abstraction level
// keyed by object id, so it is renderer implementation agnostic. Frame marker
// ops invoke hooks so a host can drive begin and flush around each frame.
namespace rive
{

struct SerializedReplayHooks
{
    std::function<void()> onFrame = nullptr;
    std::function<void(uint32_t width, uint32_t height)> onFrameSize = nullptr;
};

// Returns false on a bad header, unknown opcode, or truncated stream. The
// partial replay up to that point still happened.
bool replaySerializedCommands(Span<const uint8_t> stream,
                              Factory* factory,
                              Renderer* renderer,
                              const SerializedReplayHooks& hooks = {});

} // namespace rive

#endif
