/*
 * Copyright 2026 Rive
 */

#pragma once

#include <cstdint>

namespace rive
{
// Gives the system back the whole pages inside [begin, end) of a private
// mapping; they read as zero when touched again.
void releaseSystemPages(uint8_t* begin, uint8_t* end);
} // namespace rive
