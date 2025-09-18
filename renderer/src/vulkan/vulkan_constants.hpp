/*
 * Copyright 2025 Rive
 */

#pragma once

#include <stdint.h>
#include "shaders/constants.glsl"

namespace rive::gpu
{
// This is the render pass attachment index for the final color output in the
// "coalesced" atomic resolve.
// NOTE: This attachment is still referenced as color attachment 0 by the
// resolve subpass, so the shader doesn't need to know about it.
// NOTE: Atomic mode does not use SCRATCH_COLOR_PLANE_IDX, which is why we chose
// to alias this one.
constexpr static uint32_t COALESCED_ATOMIC_RESOLVE_IDX =
    SCRATCH_COLOR_PLANE_IDX;

// MSAA doesn't use other planes besides color, so depth & resolve start right
// after color.
constexpr static uint32_t MSAA_DEPTH_STENCIL_IDX = COLOR_PLANE_IDX + 1;
constexpr static uint32_t MSAA_RESOLVE_IDX = COLOR_PLANE_IDX + 2;

// The most attachments we currently use is 4, in rasterOrdering mode.
constexpr static uint32_t MAX_FRAMEBUFFER_ATTACHMENTS = PLS_PLANE_COUNT;

} // namespace rive::gpu