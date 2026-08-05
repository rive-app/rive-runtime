/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/renderer/cmd/handle_flags.hpp"
#include <cstdint>

// Dense resource id for the deferred 2D command stream.
namespace rive::cmd
{
using RenderHandle = uint32_t;
constexpr RenderHandle kInvalidRenderHandle = ~0u;

// A canvas drawImage carries this flag; the low bits index the canvas table.
// Test after kInvalidRenderHandle, which also has the high bit set.
constexpr RenderHandle kCanvasHandleFlag = kHandleForeignFlag;
constexpr RenderHandle kCanvasHandleMask = kHandleForeignMask;
} // namespace rive::cmd
