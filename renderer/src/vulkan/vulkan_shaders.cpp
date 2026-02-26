/*
 * Copyright 2023 Rive
 */

#include "vulkan_shaders.hpp"

#include "rive/span.hpp"

namespace rive::gpu::spirv
{
namespace embedded
{
// Draw setup shaders.
#include "generated/shaders/spirv/color_ramp.vert.h"
#include "generated/shaders/spirv/color_ramp.frag.h"
#include "generated/shaders/spirv/tessellate.vert.h"
#include "generated/shaders/spirv/tessellate.frag.h"
#include "generated/shaders/spirv/render_atlas.vert.h"
#include "generated/shaders/spirv/render_atlas_fill.frag.h"
#include "generated/shaders/spirv/render_atlas_stroke.frag.h"

// InterlockMode::rasterOrdering shaders.
#include "generated/shaders/spirv/draw_path.vert.h"
#include "generated/shaders/spirv/draw_path.frag.h"
#include "generated/shaders/spirv/draw_interior_triangles.vert.h"
#include "generated/shaders/spirv/draw_interior_triangles.frag.h"
#include "generated/shaders/spirv/draw_atlas_blit.vert.h"
#include "generated/shaders/spirv/draw_atlas_blit.frag.h"
#include "generated/shaders/spirv/draw_image_mesh.vert.h"
#include "generated/shaders/spirv/draw_image_mesh.frag.h"

// InterlockMode::atomics shaders.
#include "generated/shaders/spirv/atomic_draw_path.vert.h"
#include "generated/shaders/spirv/atomic_draw_path.frag.h"
#include "generated/shaders/spirv/atomic_draw_path.fixedcolor_frag.h"
#include "generated/shaders/spirv/atomic_draw_interior_triangles.vert.h"
#include "generated/shaders/spirv/atomic_draw_interior_triangles.frag.h"
#include "generated/shaders/spirv/atomic_draw_interior_triangles.fixedcolor_frag.h"
#include "generated/shaders/spirv/atomic_draw_atlas_blit.vert.h"
#include "generated/shaders/spirv/atomic_draw_atlas_blit.frag.h"
#include "generated/shaders/spirv/atomic_draw_atlas_blit.fixedcolor_frag.h"
#include "generated/shaders/spirv/atomic_draw_image_rect.vert.h"
#include "generated/shaders/spirv/atomic_draw_image_rect.frag.h"
#include "generated/shaders/spirv/atomic_draw_image_rect.fixedcolor_frag.h"
#include "generated/shaders/spirv/atomic_draw_image_mesh.vert.h"
#include "generated/shaders/spirv/atomic_draw_image_mesh.frag.h"
#include "generated/shaders/spirv/atomic_draw_image_mesh.fixedcolor_frag.h"
#include "generated/shaders/spirv/atomic_resolve.vert.h"
#include "generated/shaders/spirv/atomic_resolve.frag.h"
#include "generated/shaders/spirv/atomic_resolve.fixedcolor_frag.h"
#include "generated/shaders/spirv/atomic_resolve_coalesced.vert.h"
#include "generated/shaders/spirv/atomic_resolve_coalesced.frag.h"

// InterlockMode::clockwise shaders.
#ifndef RIVE_ANDROID
#include "generated/shaders/spirv/draw_clockwise_path.vert.h"
#include "generated/shaders/spirv/draw_clockwise_path.frag.h"
#include "generated/shaders/spirv/draw_clockwise_path.fixedcolor_frag.h"
#include "generated/shaders/spirv/draw_clockwise_clip.frag.h"
#include "generated/shaders/spirv/draw_clockwise_clip.fixedcolor_frag.h"
#include "generated/shaders/spirv/draw_clockwise_interior_triangles.vert.h"
#include "generated/shaders/spirv/draw_clockwise_interior_triangles.frag.h"
#include "generated/shaders/spirv/draw_clockwise_interior_triangles.fixedcolor_frag.h"
#include "generated/shaders/spirv/draw_clockwise_interior_triangles_clip.frag.h"
#include "generated/shaders/spirv/draw_clockwise_interior_triangles_clip.fixedcolor_frag.h"
#include "generated/shaders/spirv/draw_clockwise_atlas_blit.vert.h"
#include "generated/shaders/spirv/draw_clockwise_atlas_blit.frag.h"
#include "generated/shaders/spirv/draw_clockwise_atlas_blit.fixedcolor_frag.h"
#include "generated/shaders/spirv/draw_clockwise_image_mesh.vert.h"
#include "generated/shaders/spirv/draw_clockwise_image_mesh.frag.h"
#include "generated/shaders/spirv/draw_clockwise_image_mesh.fixedcolor_frag.h"
#endif

// InterlockMode::clockwiseAtomic shaders.
#include "generated/shaders/spirv/draw_clockwise_atomic_path.vert.h"
#include "generated/shaders/spirv/draw_clockwise_atomic_path.frag.h"
#include "generated/shaders/spirv/draw_clockwise_atomic_path_borrowed_coverage.frag.h"
#include "generated/shaders/spirv/draw_clockwise_atomic_interior_triangles.vert.h"
#include "generated/shaders/spirv/draw_clockwise_atomic_interior_triangles.frag.h"
#include "generated/shaders/spirv/draw_clockwise_atomic_interior_triangles_borrowed_coverage.frag.h"
#include "generated/shaders/spirv/draw_clockwise_atomic_atlas_blit.vert.h"
#include "generated/shaders/spirv/draw_clockwise_atomic_atlas_blit.fixedcolor_frag.h"
#include "generated/shaders/spirv/draw_clockwise_atomic_image_mesh.vert.h"
#include "generated/shaders/spirv/draw_clockwise_atomic_image_mesh.fixedcolor_frag.h"

// InterlockMode::msaa shaders.
#include "generated/shaders/spirv/draw_msaa_path.vert.h"
#include "generated/shaders/spirv/draw_msaa_path.frag.h"
#include "generated/shaders/spirv/draw_msaa_path.fixedcolor_frag.h"
#include "generated/shaders/spirv/draw_msaa_path.noclipdistance_vert.h"
#include "generated/shaders/spirv/draw_msaa_stencil.vert.h"
#include "generated/shaders/spirv/draw_msaa_stencil.frag.h"
#include "generated/shaders/spirv/draw_msaa_stencil.fixedcolor_frag.h"
#include "generated/shaders/spirv/draw_msaa_atlas_blit.vert.h"
#include "generated/shaders/spirv/draw_msaa_atlas_blit.frag.h"
#include "generated/shaders/spirv/draw_msaa_atlas_blit.fixedcolor_frag.h"
#include "generated/shaders/spirv/draw_msaa_atlas_blit.noclipdistance_vert.h"
#include "generated/shaders/spirv/draw_msaa_image_mesh.vert.h"
#include "generated/shaders/spirv/draw_msaa_image_mesh.frag.h"
#include "generated/shaders/spirv/draw_msaa_image_mesh.fixedcolor_frag.h"
#include "generated/shaders/spirv/draw_msaa_image_mesh.noclipdistance_vert.h"
#include "generated/shaders/spirv/draw_fullscreen_quad.vert.h"
#include "generated/shaders/spirv/draw_input_attachment.frag.h"
#include "generated/shaders/spirv/draw_msaa_color_seed_attachment.frag.h"
#include "generated/shaders/spirv/draw_msaa_resolve.frag.h"
} // namespace embedded

// Draw setup shaders.
rive::Span<const uint32_t> color_ramp_vert =
    rive::make_span(embedded::color_ramp_vert);
rive::Span<const uint32_t> color_ramp_frag =
    rive::make_span(embedded::color_ramp_frag);
rive::Span<const uint32_t> tessellate_vert =
    rive::make_span(embedded::tessellate_vert);
rive::Span<const uint32_t> tessellate_frag =
    rive::make_span(embedded::tessellate_frag);
rive::Span<const uint32_t> render_atlas_vert =
    rive::make_span(embedded::render_atlas_vert);
rive::Span<const uint32_t> render_atlas_fill_frag =
    rive::make_span(embedded::render_atlas_fill_frag);
rive::Span<const uint32_t> render_atlas_stroke_frag =
    rive::make_span(embedded::render_atlas_stroke_frag);

// InterlockMode::rasterOrdering shaders.
rive::Span<const uint32_t> draw_path_vert =
    rive::make_span(embedded::draw_path_vert);
rive::Span<const uint32_t> draw_path_frag =
    rive::make_span(embedded::draw_path_frag);
rive::Span<const uint32_t> draw_interior_triangles_vert =
    rive::make_span(embedded::draw_interior_triangles_vert);
rive::Span<const uint32_t> draw_interior_triangles_frag =
    rive::make_span(embedded::draw_interior_triangles_frag);
rive::Span<const uint32_t> draw_atlas_blit_vert =
    rive::make_span(embedded::draw_atlas_blit_vert);
rive::Span<const uint32_t> draw_atlas_blit_frag =
    rive::make_span(embedded::draw_atlas_blit_frag);
rive::Span<const uint32_t> draw_image_mesh_vert =
    rive::make_span(embedded::draw_image_mesh_vert);
rive::Span<const uint32_t> draw_image_mesh_frag =
    rive::make_span(embedded::draw_image_mesh_frag);

// InterlockMode::atomics shaders.
rive::Span<const uint32_t> atomic_draw_path_vert =
    rive::make_span(embedded::atomic_draw_path_vert);
rive::Span<const uint32_t> atomic_draw_path_frag =
    rive::make_span(embedded::atomic_draw_path_frag);
rive::Span<const uint32_t> atomic_draw_path_fixedcolor_frag =
    rive::make_span(embedded::atomic_draw_path_fixedcolor_frag);
rive::Span<const uint32_t> atomic_draw_interior_triangles_vert =
    rive::make_span(embedded::atomic_draw_interior_triangles_vert);
rive::Span<const uint32_t> atomic_draw_interior_triangles_frag =
    rive::make_span(embedded::atomic_draw_interior_triangles_frag);
rive::Span<const uint32_t> atomic_draw_interior_triangles_fixedcolor_frag =
    rive::make_span(embedded::atomic_draw_interior_triangles_fixedcolor_frag);
rive::Span<const uint32_t> atomic_draw_atlas_blit_vert =
    rive::make_span(embedded::atomic_draw_atlas_blit_vert);
rive::Span<const uint32_t> atomic_draw_atlas_blit_frag =
    rive::make_span(embedded::atomic_draw_atlas_blit_frag);
rive::Span<const uint32_t> atomic_draw_atlas_blit_fixedcolor_frag =
    rive::make_span(embedded::atomic_draw_atlas_blit_fixedcolor_frag);
rive::Span<const uint32_t> atomic_draw_image_rect_vert =
    rive::make_span(embedded::atomic_draw_image_rect_vert);
rive::Span<const uint32_t> atomic_draw_image_rect_frag =
    rive::make_span(embedded::atomic_draw_image_rect_frag);
rive::Span<const uint32_t> atomic_draw_image_rect_fixedcolor_frag =
    rive::make_span(embedded::atomic_draw_image_rect_fixedcolor_frag);
rive::Span<const uint32_t> atomic_draw_image_mesh_vert =
    rive::make_span(embedded::atomic_draw_image_mesh_vert);
rive::Span<const uint32_t> atomic_draw_image_mesh_frag =
    rive::make_span(embedded::atomic_draw_image_mesh_frag);
rive::Span<const uint32_t> atomic_draw_image_mesh_fixedcolor_frag =
    rive::make_span(embedded::atomic_draw_image_mesh_fixedcolor_frag);
rive::Span<const uint32_t> atomic_resolve_vert =
    rive::make_span(embedded::atomic_resolve_vert);
rive::Span<const uint32_t> atomic_resolve_frag =
    rive::make_span(embedded::atomic_resolve_frag);
rive::Span<const uint32_t> atomic_resolve_fixedcolor_frag =
    rive::make_span(embedded::atomic_resolve_fixedcolor_frag);
rive::Span<const uint32_t> atomic_resolve_coalesced_vert =
    rive::make_span(embedded::atomic_resolve_coalesced_vert);
rive::Span<const uint32_t> atomic_resolve_coalesced_frag =
    rive::make_span(embedded::atomic_resolve_coalesced_frag);

#ifndef RIVE_ANDROID
// InterlockMode::clockwise shaders.
rive::Span<const uint32_t> draw_clockwise_path_vert =
    rive::make_span(embedded::draw_clockwise_path_vert);
rive::Span<const uint32_t> draw_clockwise_path_frag =
    rive::make_span(embedded::draw_clockwise_path_frag);
rive::Span<const uint32_t> draw_clockwise_path_fixedcolor_frag =
    rive::make_span(embedded::draw_clockwise_path_fixedcolor_frag);
rive::Span<const uint32_t> draw_clockwise_clip_frag =
    rive::make_span(embedded::draw_clockwise_clip_frag);
rive::Span<const uint32_t> draw_clockwise_clip_fixedcolor_frag =
    rive::make_span(embedded::draw_clockwise_clip_fixedcolor_frag);
rive::Span<const uint32_t> draw_clockwise_interior_triangles_vert =
    rive::make_span(embedded::draw_clockwise_interior_triangles_vert);
rive::Span<const uint32_t> draw_clockwise_interior_triangles_frag =
    rive::make_span(embedded::draw_clockwise_interior_triangles_frag);
rive::Span<const uint32_t> draw_clockwise_interior_triangles_fixedcolor_frag =
    rive::make_span(
        embedded::draw_clockwise_interior_triangles_fixedcolor_frag);
rive::Span<const uint32_t> draw_clockwise_interior_triangles_clip_frag =
    rive::make_span(embedded::draw_clockwise_interior_triangles_clip_frag);
rive::Span<const uint32_t>
    draw_clockwise_interior_triangles_clip_fixedcolor_frag = rive::make_span(
        embedded::draw_clockwise_interior_triangles_clip_fixedcolor_frag);
rive::Span<const uint32_t> draw_clockwise_atlas_blit_vert =
    rive::make_span(embedded::draw_clockwise_atlas_blit_vert);
rive::Span<const uint32_t> draw_clockwise_atlas_blit_frag =
    rive::make_span(embedded::draw_clockwise_atlas_blit_frag);
rive::Span<const uint32_t> draw_clockwise_atlas_blit_fixedcolor_frag =
    rive::make_span(embedded::draw_clockwise_atlas_blit_fixedcolor_frag);
rive::Span<const uint32_t> draw_clockwise_image_mesh_vert =
    rive::make_span(embedded::draw_clockwise_image_mesh_vert);
rive::Span<const uint32_t> draw_clockwise_image_mesh_frag =
    rive::make_span(embedded::draw_clockwise_image_mesh_frag);
rive::Span<const uint32_t> draw_clockwise_image_mesh_fixedcolor_frag =
    rive::make_span(embedded::draw_clockwise_image_mesh_fixedcolor_frag);
#endif

// InterlockMode::clockwiseAtomic shaders.
rive::Span<const uint32_t> draw_clockwise_atomic_path_vert =
    rive::make_span(embedded::draw_clockwise_atomic_path_vert);
rive::Span<const uint32_t> draw_clockwise_atomic_path_frag =
    rive::make_span(embedded::draw_clockwise_atomic_path_frag);
rive::Span<const uint32_t> draw_clockwise_atomic_path_borrowed_coverage_frag =
    rive::make_span(
        embedded::draw_clockwise_atomic_path_borrowed_coverage_frag);
rive::Span<const uint32_t> draw_clockwise_atomic_interior_triangles_vert =
    rive::make_span(embedded::draw_clockwise_atomic_interior_triangles_vert);
rive::Span<const uint32_t> draw_clockwise_atomic_interior_triangles_frag =
    rive::make_span(embedded::draw_clockwise_atomic_interior_triangles_frag);
rive::Span<const uint32_t>
    draw_clockwise_atomic_interior_triangles_borrowed_coverage_frag =
        rive::make_span(
            embedded::
                draw_clockwise_atomic_interior_triangles_borrowed_coverage_frag);
rive::Span<const uint32_t> draw_clockwise_atomic_atlas_blit_vert =
    rive::make_span(embedded::draw_clockwise_atomic_atlas_blit_vert);
rive::Span<const uint32_t> draw_clockwise_atomic_atlas_blit_fixedcolor_frag =
    rive::make_span(embedded::draw_clockwise_atomic_atlas_blit_fixedcolor_frag);
rive::Span<const uint32_t> draw_clockwise_atomic_image_mesh_vert =
    rive::make_span(embedded::draw_clockwise_atomic_image_mesh_vert);
rive::Span<const uint32_t> draw_clockwise_atomic_image_mesh_fixedcolor_frag =
    rive::make_span(embedded::draw_clockwise_atomic_image_mesh_fixedcolor_frag);

// InterlockMode::msaa shaders.
rive::Span<const uint32_t> draw_msaa_path_vert =
    rive::make_span(embedded::draw_msaa_path_vert);
rive::Span<const uint32_t> draw_msaa_path_noclipdistance_vert =
    rive::make_span(embedded::draw_msaa_path_noclipdistance_vert);
rive::Span<const uint32_t> draw_msaa_path_frag =
    rive::make_span(embedded::draw_msaa_path_frag);
rive::Span<const uint32_t> draw_msaa_path_fixedcolor_frag =
    rive::make_span(embedded::draw_msaa_path_fixedcolor_frag);
rive::Span<const uint32_t> draw_msaa_stencil_vert =
    rive::make_span(embedded::draw_msaa_stencil_vert);
rive::Span<const uint32_t> draw_msaa_stencil_frag =
    rive::make_span(embedded::draw_msaa_stencil_frag);
rive::Span<const uint32_t> draw_msaa_stencil_fixedcolor_frag =
    rive::make_span(embedded::draw_msaa_stencil_fixedcolor_frag);
rive::Span<const uint32_t> draw_msaa_atlas_blit_vert =
    rive::make_span(embedded::draw_msaa_atlas_blit_vert);
rive::Span<const uint32_t> draw_msaa_atlas_blit_noclipdistance_vert =
    rive::make_span(embedded::draw_msaa_atlas_blit_noclipdistance_vert);
rive::Span<const uint32_t> draw_msaa_atlas_blit_frag =
    rive::make_span(embedded::draw_msaa_atlas_blit_frag);
rive::Span<const uint32_t> draw_msaa_atlas_blit_fixedcolor_frag =
    rive::make_span(embedded::draw_msaa_atlas_blit_fixedcolor_frag);
rive::Span<const uint32_t> draw_msaa_image_mesh_vert =
    rive::make_span(embedded::draw_msaa_image_mesh_vert);
rive::Span<const uint32_t> draw_msaa_image_mesh_noclipdistance_vert =
    rive::make_span(embedded::draw_msaa_image_mesh_noclipdistance_vert);
rive::Span<const uint32_t> draw_msaa_image_mesh_frag =
    rive::make_span(embedded::draw_msaa_image_mesh_frag);
rive::Span<const uint32_t> draw_msaa_image_mesh_fixedcolor_frag =
    rive::make_span(embedded::draw_msaa_image_mesh_fixedcolor_frag);
rive::Span<const uint32_t> draw_fullscreen_quad_vert =
    rive::make_span(embedded::draw_fullscreen_quad_vert);
rive::Span<const uint32_t> draw_input_attachment_frag =
    rive::make_span(embedded::draw_input_attachment_frag);
rive::Span<const uint32_t> draw_msaa_color_seed_attachment_frag =
    rive::make_span(embedded::draw_msaa_color_seed_attachment_frag);
rive::Span<const uint32_t> draw_msaa_resolve_frag =
    rive::make_span(embedded::draw_msaa_resolve_frag);

void hotload_shaders(rive::Span<const uint32_t> spirvData)
{
    size_t spirvIndex = 0;
    auto readNextBytecodeSpan = [spirvData,
                                 &spirvIndex]() -> rive::Span<const uint32_t> {
        size_t insnCount = spirvData[spirvIndex++];
        const uint32_t* insnData = spirvData.data() + spirvIndex;
        spirvIndex += insnCount;
        return rive::make_span(insnData, insnCount);
    };

    spirv::color_ramp_vert = readNextBytecodeSpan();
    spirv::color_ramp_frag = readNextBytecodeSpan();
    spirv::tessellate_vert = readNextBytecodeSpan();
    spirv::tessellate_frag = readNextBytecodeSpan();
    spirv::render_atlas_vert = readNextBytecodeSpan();
    spirv::render_atlas_fill_frag = readNextBytecodeSpan();
    spirv::render_atlas_stroke_frag = readNextBytecodeSpan();
    spirv::draw_path_vert = readNextBytecodeSpan();
    spirv::draw_path_frag = readNextBytecodeSpan();
    spirv::draw_interior_triangles_vert = readNextBytecodeSpan();
    spirv::draw_interior_triangles_frag = readNextBytecodeSpan();
    spirv::draw_atlas_blit_vert = readNextBytecodeSpan();
    spirv::draw_atlas_blit_frag = readNextBytecodeSpan();
    spirv::draw_image_mesh_vert = readNextBytecodeSpan();
    spirv::draw_image_mesh_frag = readNextBytecodeSpan();

    spirv::atomic_draw_path_vert = readNextBytecodeSpan();
    spirv::atomic_draw_path_frag = readNextBytecodeSpan();
    spirv::atomic_draw_path_fixedcolor_frag = readNextBytecodeSpan();
    spirv::atomic_draw_interior_triangles_vert = readNextBytecodeSpan();
    spirv::atomic_draw_interior_triangles_frag = readNextBytecodeSpan();
    spirv::atomic_draw_interior_triangles_fixedcolor_frag =
        readNextBytecodeSpan();
    spirv::atomic_draw_atlas_blit_vert = readNextBytecodeSpan();
    spirv::atomic_draw_atlas_blit_frag = readNextBytecodeSpan();
    spirv::atomic_draw_atlas_blit_fixedcolor_frag = readNextBytecodeSpan();
    spirv::atomic_draw_image_rect_vert = readNextBytecodeSpan();
    spirv::atomic_draw_image_rect_frag = readNextBytecodeSpan();
    spirv::atomic_draw_image_rect_fixedcolor_frag = readNextBytecodeSpan();
    spirv::atomic_draw_image_mesh_vert = readNextBytecodeSpan();
    spirv::atomic_draw_image_mesh_frag = readNextBytecodeSpan();
    spirv::atomic_draw_image_mesh_fixedcolor_frag = readNextBytecodeSpan();
    spirv::atomic_resolve_vert = readNextBytecodeSpan();
    spirv::atomic_resolve_frag = readNextBytecodeSpan();
    spirv::atomic_resolve_fixedcolor_frag = readNextBytecodeSpan();
    spirv::atomic_resolve_coalesced_vert = readNextBytecodeSpan();
    spirv::atomic_resolve_coalesced_frag = readNextBytecodeSpan();

#ifndef RIVE_ANDROID
    spirv::draw_clockwise_path_vert = readNextBytecodeSpan();
    spirv::draw_clockwise_path_frag = readNextBytecodeSpan();
    spirv::draw_clockwise_path_fixedcolor_frag = readNextBytecodeSpan();
    spirv::draw_clockwise_clip_frag = readNextBytecodeSpan();
    spirv::draw_clockwise_clip_fixedcolor_frag = readNextBytecodeSpan();
    spirv::draw_clockwise_interior_triangles_vert = readNextBytecodeSpan();
    spirv::draw_clockwise_interior_triangles_frag = readNextBytecodeSpan();
    spirv::draw_clockwise_interior_triangles_fixedcolor_frag =
        readNextBytecodeSpan();
    spirv::draw_clockwise_interior_triangles_clip_frag = readNextBytecodeSpan();
    spirv::draw_clockwise_interior_triangles_clip_fixedcolor_frag =
        readNextBytecodeSpan();
    spirv::draw_clockwise_atlas_blit_vert = readNextBytecodeSpan();
    spirv::draw_clockwise_atlas_blit_frag = readNextBytecodeSpan();
    spirv::draw_clockwise_atlas_blit_fixedcolor_frag = readNextBytecodeSpan();
    spirv::draw_clockwise_image_mesh_vert = readNextBytecodeSpan();
    spirv::draw_clockwise_image_mesh_frag = readNextBytecodeSpan();
    spirv::draw_clockwise_image_mesh_fixedcolor_frag = readNextBytecodeSpan();
#endif

    spirv::draw_clockwise_atomic_path_vert = readNextBytecodeSpan();
    spirv::draw_clockwise_atomic_path_frag = readNextBytecodeSpan();
    spirv::draw_clockwise_atomic_path_borrowed_coverage_frag =
        readNextBytecodeSpan();
    spirv::draw_clockwise_atomic_interior_triangles_vert =
        readNextBytecodeSpan();
    spirv::draw_clockwise_atomic_interior_triangles_frag =
        readNextBytecodeSpan();
    spirv::draw_clockwise_atomic_interior_triangles_borrowed_coverage_frag =
        readNextBytecodeSpan();
    spirv::draw_clockwise_atomic_atlas_blit_vert = readNextBytecodeSpan();
    spirv::draw_clockwise_atomic_atlas_blit_fixedcolor_frag =
        readNextBytecodeSpan();
    spirv::draw_clockwise_atomic_image_mesh_vert = readNextBytecodeSpan();
    spirv::draw_clockwise_atomic_image_mesh_fixedcolor_frag =
        readNextBytecodeSpan();

    spirv::draw_msaa_path_vert = readNextBytecodeSpan();
    spirv::draw_msaa_path_noclipdistance_vert = readNextBytecodeSpan();
    spirv::draw_msaa_path_frag = readNextBytecodeSpan();
    spirv::draw_msaa_path_fixedcolor_frag = readNextBytecodeSpan();
    spirv::draw_msaa_stencil_vert = readNextBytecodeSpan();
    spirv::draw_msaa_stencil_frag = readNextBytecodeSpan();
    spirv::draw_msaa_stencil_fixedcolor_frag = readNextBytecodeSpan();
    spirv::draw_msaa_atlas_blit_vert = readNextBytecodeSpan();
    spirv::draw_msaa_atlas_blit_noclipdistance_vert = readNextBytecodeSpan();
    spirv::draw_msaa_atlas_blit_frag = readNextBytecodeSpan();
    spirv::draw_msaa_atlas_blit_fixedcolor_frag = readNextBytecodeSpan();
    spirv::draw_msaa_image_mesh_vert = readNextBytecodeSpan();
    spirv::draw_msaa_image_mesh_noclipdistance_vert = readNextBytecodeSpan();
    spirv::draw_msaa_image_mesh_frag = readNextBytecodeSpan();
    spirv::draw_msaa_image_mesh_fixedcolor_frag = readNextBytecodeSpan();
    spirv::draw_fullscreen_quad_vert = readNextBytecodeSpan();
    spirv::draw_input_attachment_frag = readNextBytecodeSpan();
    spirv::draw_msaa_color_seed_attachment_frag = readNextBytecodeSpan();
    spirv::draw_msaa_resolve_frag = readNextBytecodeSpan();
}
} // namespace rive::gpu::spirv
