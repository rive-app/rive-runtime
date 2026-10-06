/*
 * Copyright 2026 Rive
 */

#pragma once

#ifdef __EMSCRIPTEN__

#include "rive/span.hpp"
#include <cstdint>
#include <functional>

namespace rive
{
// Pixels are premultiplied RGBA8 and only valid during the call; on failure
// they are empty and error is set.
using BrowserImageDecoded = std::function<void(uint32_t id,
                                               uint32_t width,
                                               uint32_t height,
                                               Span<const uint8_t> pixels,
                                               const char* error)>;

// Decodes with the browser's own image decoder. done runs from a later event
// loop turn, never inside this call. Returns the id it passes to done.
uint32_t startBrowserImageDecode(const uint8_t* bytes,
                                 uint32_t byteCount,
                                 BrowserImageDecoded done);

// The browser still finishes the decode, but done never runs.
void cancelBrowserImageDecode(uint32_t id);
} // namespace rive

#endif
