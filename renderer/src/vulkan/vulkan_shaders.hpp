/*
 * Copyright 2023 Rive
 */

#pragma once

#include <vulkan/vulkan.h>
#include "rive/span.hpp"

namespace rive::gpu::spirv
{
// Draw setup shaders.
extern rive::Span<const uint32_t> color_ramp_vert;
extern rive::Span<const uint32_t> color_ramp_frag;
extern rive::Span<const uint32_t> tessellate_vert;
extern rive::Span<const uint32_t> tessellate_frag;
extern rive::Span<const uint32_t> render_atlas_vert;
extern rive::Span<const uint32_t> render_atlas_fill_frag;
extern rive::Span<const uint32_t> render_atlas_stroke_frag;

// InterlockMode::rasterOrdering shaders.
extern rive::Span<const uint32_t> draw_path_vert;
extern rive::Span<const uint32_t> draw_path_frag;
extern rive::Span<const uint32_t> draw_interior_triangles_vert;
extern rive::Span<const uint32_t> draw_interior_triangles_frag;
extern rive::Span<const uint32_t> draw_atlas_blit_vert;
extern rive::Span<const uint32_t> draw_atlas_blit_frag;
extern rive::Span<const uint32_t> draw_image_mesh_vert;
extern rive::Span<const uint32_t> draw_image_mesh_frag;

// InterlockMode::atomics shaders.
extern rive::Span<const uint32_t> atomic_draw_path_vert;
extern rive::Span<const uint32_t> atomic_draw_path_frag;
extern rive::Span<const uint32_t> atomic_draw_path_fixedcolor_frag;
extern rive::Span<const uint32_t> atomic_draw_interior_triangles_vert;
extern rive::Span<const uint32_t> atomic_draw_interior_triangles_frag;
extern rive::Span<const uint32_t>
    atomic_draw_interior_triangles_fixedcolor_frag;
extern rive::Span<const uint32_t> atomic_draw_atlas_blit_vert;
extern rive::Span<const uint32_t> atomic_draw_atlas_blit_frag;
extern rive::Span<const uint32_t> atomic_draw_atlas_blit_fixedcolor_frag;
extern rive::Span<const uint32_t> atomic_draw_image_rect_vert;
extern rive::Span<const uint32_t> atomic_draw_image_rect_frag;
extern rive::Span<const uint32_t> atomic_draw_image_rect_fixedcolor_frag;
extern rive::Span<const uint32_t> atomic_draw_image_mesh_vert;
extern rive::Span<const uint32_t> atomic_draw_image_mesh_frag;
extern rive::Span<const uint32_t> atomic_draw_image_mesh_fixedcolor_frag;
extern rive::Span<const uint32_t> atomic_resolve_vert;
extern rive::Span<const uint32_t> atomic_resolve_frag;
extern rive::Span<const uint32_t> atomic_resolve_fixedcolor_frag;
extern rive::Span<const uint32_t> atomic_resolve_coalesced_vert;
extern rive::Span<const uint32_t> atomic_resolve_coalesced_frag;

#ifndef RIVE_ANDROID
// InterlockMode::clockwise shaders.
extern rive::Span<const uint32_t> draw_clockwise_path_vert;
extern rive::Span<const uint32_t> draw_clockwise_path_frag;
extern rive::Span<const uint32_t> draw_clockwise_path_fixedcolor_frag;
extern rive::Span<const uint32_t> draw_clockwise_clip_frag;
extern rive::Span<const uint32_t> draw_clockwise_clip_fixedcolor_frag;
extern rive::Span<const uint32_t> draw_clockwise_interior_triangles_vert;
extern rive::Span<const uint32_t> draw_clockwise_interior_triangles_frag;
extern rive::Span<const uint32_t>
    draw_clockwise_interior_triangles_fixedcolor_frag;
extern rive::Span<const uint32_t> draw_clockwise_interior_triangles_clip_frag;
extern rive::Span<const uint32_t>
    draw_clockwise_interior_triangles_clip_fixedcolor_frag;
extern rive::Span<const uint32_t> draw_clockwise_atlas_blit_vert;
extern rive::Span<const uint32_t> draw_clockwise_atlas_blit_frag;
extern rive::Span<const uint32_t> draw_clockwise_atlas_blit_fixedcolor_frag;
extern rive::Span<const uint32_t> draw_clockwise_image_mesh_vert;
extern rive::Span<const uint32_t> draw_clockwise_image_mesh_frag;
extern rive::Span<const uint32_t> draw_clockwise_image_mesh_fixedcolor_frag;
#endif

// InterlockMode::clockwiseAtomic shaders.
extern rive::Span<const uint32_t> draw_clockwise_atomic_path_vert;
extern rive::Span<const uint32_t> draw_clockwise_atomic_path_frag;
extern rive::Span<const uint32_t> draw_clockwise_atomic_interior_triangles_vert;
extern rive::Span<const uint32_t> draw_clockwise_atomic_interior_triangles_frag;
extern rive::Span<const uint32_t> draw_clockwise_atomic_atlas_blit_vert;
extern rive::Span<const uint32_t> draw_clockwise_atomic_atlas_blit_frag;
extern rive::Span<const uint32_t> draw_clockwise_atomic_image_mesh_vert;
extern rive::Span<const uint32_t> draw_clockwise_atomic_image_mesh_frag;

// InterlockMode::msaa shaders.
extern rive::Span<const uint32_t> draw_msaa_path_vert;
extern rive::Span<const uint32_t> draw_msaa_path_noclipdistance_vert;
extern rive::Span<const uint32_t> draw_msaa_path_frag;
extern rive::Span<const uint32_t> draw_msaa_path_fixedcolor_frag;
extern rive::Span<const uint32_t> draw_msaa_stencil_vert;
extern rive::Span<const uint32_t> draw_msaa_stencil_frag;
extern rive::Span<const uint32_t> draw_msaa_stencil_fixedcolor_frag;
extern rive::Span<const uint32_t> draw_msaa_atlas_blit_vert;
extern rive::Span<const uint32_t> draw_msaa_atlas_blit_noclipdistance_vert;
extern rive::Span<const uint32_t> draw_msaa_atlas_blit_frag;
extern rive::Span<const uint32_t> draw_msaa_atlas_blit_fixedcolor_frag;
extern rive::Span<const uint32_t> draw_msaa_image_mesh_vert;
extern rive::Span<const uint32_t> draw_msaa_image_mesh_noclipdistance_vert;
extern rive::Span<const uint32_t> draw_msaa_image_mesh_frag;
extern rive::Span<const uint32_t> draw_msaa_image_mesh_fixedcolor_frag;
extern rive::Span<const uint32_t> draw_fullscreen_quad_vert;
extern rive::Span<const uint32_t> draw_input_attachment_frag;
extern rive::Span<const uint32_t> draw_msaa_color_seed_attachment_frag;
extern rive::Span<const uint32_t> draw_msaa_resolve_frag;

// Reload global SPIRV buffers from runtime data.
void hotload_shaders(rive::Span<const uint32_t> spirvData);
} // namespace rive::gpu::spirv
