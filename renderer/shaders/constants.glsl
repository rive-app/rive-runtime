/*
 * Copyright 2022 Rive
 */

#define TESS_TEXTURE_WIDTH 2048.
#define TESS_TEXTURE_WIDTH_LOG2 11

#define GRAD_TEXTURE_WIDTH 512.

// Tells shaders that a cubic should actually be drawn as the single, non-AA triangle: [p0, p1, p3].
// This is used to squeeze in more rare triangles, like "grout" triangles from self intersections on
// interior triangulation, where it wouldn't be worth it to put them in their own dedicated draw
// call.
#define RETROFITTED_TRIANGLE_CONTOUR_FLAG (1u << 31u)

// Tells the tessellation shader to re-run Wang's formula on the given curve, figure out how many
// segments it actually needs, and make any excess segments degenerate by co-locating their vertices
// at T=0. (Used on the "outerCurve" patches that are drawn with interior triangulations.)
#define CULL_EXCESS_TESSELLATION_SEGMENTS_CONTOUR_FLAG (1u << 30u)

// Flags for specifying the join type.
#define JOIN_TYPE_MASK (3u << 28u)
#define MITER_CLIP_JOIN_CONTOUR_FLAG (3u << 28u)
#define MITER_REVERT_JOIN_CONTOUR_FLAG (2u << 28u)
#define BEVEL_JOIN_CONTOUR_FLAG (1u << 28u)

// When a join is being used to emulate a stroke cap, the shader emits additional vertices at T=0
// and T=1 for round joins, and changes the miter limit to 1 for miter-clip joins.
#define EMULATED_STROKE_CAP_CONTOUR_FLAG (1u << 27u)

// Internal contour flags.
#define MIRRORED_CONTOUR_CONTOUR_FLAG (1u << 26u)
#define JOIN_TANGENT_0_CONTOUR_FLAG (1u << 25u)
#define JOIN_TANGENT_INNER_CONTOUR_FLAG (1u << 24u)
#define LEFT_JOIN_CONTOUR_FLAG (1u << 23u)
#define RIGHT_JOIN_CONTOUR_FLAG (1u << 22u)
#define CONTOUR_ID_MASK 0xffffu

// Tells the GPU that a given path has an even-odd fill rule.
#define EVEN_ODD_PATH_FLAG (1u << 31u)

// Says which part of the patch a vertex belongs to.
#define STROKE_VERTEX 0
#define FAN_VERTEX 1
#define FAN_MIDPOINT_VERTEX 2

// Says which part of the patch a vertex belongs to.
#define STROKE_VERTEX 0
#define FAN_VERTEX 1
#define FAN_MIDPOINT_VERTEX 2

// Mirrors pls::PaintType.
#define SOLID_COLOR_PAINT_TYPE 0u
#define LINEAR_GRADIENT_PAINT_TYPE 1u
#define RADIAL_GRADIENT_PAINT_TYPE 2u
#define IMAGE_PAINT_TYPE 3u
#define CLIP_UPDATE_PAINT_TYPE 4u

// Index of each pixel local storage plane.
#define FRAMEBUFFER_PLANE_IDX 0
#define COVERAGE_PLANE_IDX 1
#define ORIGINAL_DST_COLOR_PLANE_IDX 2
#define CLIP_PLANE_IDX 3

// Index at which we access each resource.
#define TESS_VERTEX_TEXTURE_IDX 0
#define PATH_TEXTURE_IDX 1
#define CONTOUR_TEXTURE_IDX 2
#define GRAD_TEXTURE_IDX 3
#define IMAGE_TEXTURE_IDX 4
#define FLUSH_UNIFORM_BUFFER_IDX 5
#define DRAW_UNIFORM_BUFFER_IDX 6
#define IMAGE_MESH_UNIFORM_BUFFER_IDX 7

// Samplers are accessed at the same index as their corresponding texture, so we put them in a
// separate binding set.
#define SAMPLER_BINDINGS_SET 1

// PLS textures are accessed at the same index as their PLS planes, so we put them in a separate
// binding set.
#define PLS_TEXTURE_BINDINGS_SET 2

// acos(1/4), because the miter limit is always 4.
#define MITER_ANGLE_LIMIT 1.318116071652817965746

// Raw bit representation of the largest denormalized fp16 value. We offset all (1-based) path IDs
// by this value in order to avoid denorms, which have been empirically unreliable on Android as ID
// values.
#define MAX_DENORM_F16 1023u

// Blend modes. Mirrors rive::BlendMode, but 0-based and contiguous for tighter packing.
#define BLEND_SRC_OVER 0u
#define BLEND_MODE_SCREEN 1u
#define BLEND_MODE_OVERLAY 2u
#define BLEND_MODE_DARKEN 3u
#define BLEND_MODE_LIGHTEN 4u
#define BLEND_MODE_COLORDODGE 5u
#define BLEND_MODE_COLORBURN 6u
#define BLEND_MODE_HARDLIGHT 7u
#define BLEND_MODE_SOFTLIGHT 8u
#define BLEND_MODE_DIFFERENCE 9u
#define BLEND_MODE_EXCLUSION 10u
#define BLEND_MODE_MULTIPLY 11u
#define BLEND_MODE_HUE 12u
#define BLEND_MODE_SATURATION 13u
#define BLEND_MODE_COLOR 14u
#define BLEND_MODE_LUMINOSITY 15u
