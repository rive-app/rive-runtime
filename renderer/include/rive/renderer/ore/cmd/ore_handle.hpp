/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/renderer/cmd/handle_flags.hpp"
#include <cstdint>

// Shared handle type for the deferred command streams. A render command
// handle indexes the OreCommandBuffer keep alive table; a resource command
// handle is the client id of a deferred created resource.
namespace rive::ore::cmd
{
using ResourceHandle = uint32_t;
constexpr ResourceHandle kInvalidHandle = ~0u;

// Flags a resource that already exists at record time and so is not in the
// creation stream; the low bits index a side table of the real objects. Test
// after kInvalidHandle, which also has the high bit set.
constexpr ResourceHandle kRealResourceFlag = rive::cmd::kHandleForeignFlag;
constexpr ResourceHandle kRealResourceMask = rive::cmd::kHandleForeignMask;
} // namespace rive::ore::cmd
