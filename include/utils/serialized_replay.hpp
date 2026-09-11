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

    // Cache-as-bitmap: the stream carries offscreen canvas content inline,
    // bracketed by canvasContentBegin/End, and composites the canvas with an
    // ordinary drawImage of its id. A host with offscreen support opens the
    // canvas's frame here and returns the renderer its draws replay into,
    // plus the image a later composite should sample through *image.
    // Returning null drops the content and the composite, which is what a
    // host with no offscreen target does -- the rest of the frame still
    // replays.
    std::function<Renderer*(uint64_t id,
                            uint32_t width,
                            uint32_t height,
                            uint32_t clearColor,
                            rcp<RenderImage>* image)>
        onCanvasContentBegin = nullptr;
    std::function<void(uint64_t id)> onCanvasContentEnd = nullptr;
};

// Returns false on a bad header, unknown opcode, or truncated stream. The
// partial replay up to that point still happened.
bool replaySerializedCommands(Span<const uint8_t> stream,
                              Factory* factory,
                              Renderer* renderer,
                              const SerializedReplayHooks& hooks = {});

} // namespace rive

#endif
