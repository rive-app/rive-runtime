/*
 * Copyright 2025 Rive
 */
#pragma once
#include "shaders/constants.glsl"
namespace rive::gpu
{
// D3D11 doesn't let us bind the framebuffer UAV to slot 0 when there is a color
// output. Use the (unused in this case) SCRATCH_COLOR_PLANE_IDX instead when we
// are doing a coalesced resolve and transfer.
#define COALESCED_OFFSCREEN_COLOR_PLANE_IDX SCRATCH_COLOR_PLANE_IDX

constexpr static UINT PATCH_VERTEX_DATA_SLOT = 0;
constexpr static UINT TRIANGLE_VERTEX_DATA_SLOT = 1;
constexpr static UINT IMAGE_RECT_VERTEX_DATA_SLOT = 2;
constexpr static UINT IMAGE_MESH_VERTEX_DATA_SLOT = 3;
constexpr static UINT IMAGE_MESH_UV_DATA_SLOT = 4;
} // namespace rive::gpu