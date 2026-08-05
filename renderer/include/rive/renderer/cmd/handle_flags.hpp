/*
 * Copyright 2026 Rive
 */

#pragma once

#include <cstdint>

// Handle bit partition shared by the 2D and Ore streams: the top bit flags a
// foreign table (canvas image or already real resource), and minted ids stay
// below it. The id allocator enforces that.
namespace rive::cmd
{
constexpr uint32_t kHandleForeignFlag = 0x80000000u;
constexpr uint32_t kHandleForeignMask = 0x7fffffffu;
} // namespace rive::cmd
