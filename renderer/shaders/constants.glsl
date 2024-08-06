/*
 * Copyright 2022 Rive
 */

#define TESS_TEXTURE_WIDTH float(2048)
#define TESS_TEXTURE_WIDTH_LOG2 11

#define GRAD_TEXTURE_WIDTH float(512)
#define GRAD_TEXTURE_INVERSE_WIDTH float(0.001953125)

// Width to use for a texture that emulates a storage buffer.
//
// Minimize width since the texture needs to be updated in entire rows from the resource buffer.
// Since these only serve paths and contours, both of those are limited to 16-bit indices, 2048
// is the min specified texture size in ES3, and no path buffer uses more than 4 texels, we can
// safely use a width of 128.
#define STORAGE_TEXTURE_WIDTH 128
#define STORAGE_TEXTURE_SHIFT_Y 7
#define STORAGE_TEXTURE_MASK_X 0x7fu

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

// Paint flags, found in the x-component value of @paintBuffer.
#define PAINT_FLAG_EVEN_ODD 0x100u
#define PAINT_FLAG_HAS_CLIP_RECT 0x200u

// PLS draw resources are either updated per flush or per draw. They go into set 0
// or set 1, depending on how often they are updated.
#define PER_FLUSH_BINDINGS_SET 0
#define PER_DRAW_BINDINGS_SET 1

// Index at which we access each resource.
#define TESS_VERTEX_TEXTURE_IDX 0
#define GRAD_TEXTURE_IDX 1
#define IMAGE_TEXTURE_IDX 2
#define PATH_BUFFER_IDX 3
#define PAINT_BUFFER_IDX 4
#define PAINT_AUX_BUFFER_IDX 5
#define CONTOUR_BUFFER_IDX 6
#define FLUSH_UNIFORM_BUFFER_IDX 7
#define PATH_BASE_INSTANCE_UNIFORM_BUFFER_IDX 8
#define IMAGE_DRAW_UNIFORM_BUFFER_IDX 9
#define DST_COLOR_TEXTURE_IDX 10
#define DEFAULT_BINDINGS_SET_SIZE 11

// Samplers are accessed at the same index as their corresponding texture, so we put them in a
// separate binding set.
#define SAMPLER_BINDINGS_SET 2

// PLS textures are accessed at the same index as their PLS planes, so we put them in a separate
// binding set.
#define PLS_TEXTURE_BINDINGS_SET 3

#define BINDINGS_SET_COUNT 4

// Index of each pixel local storage plane.
#define COLOR_PLANE_IDX 0
#define CLIP_PLANE_IDX 1
#define SCRATCH_COLOR_PLANE_IDX 2
#define COVERAGE_PLANE_IDX 3

// acos(1/4), because the miter limit is always 4.
#define MITER_ANGLE_LIMIT float(1.318116071652817965746)

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

// Fixed-point coverage values for the experimental atomic mode.
// Atomic mode uses 7:9 fixed point, so the winding number breaks if a shape has more than 64
// levels of self overlap in either winding direction at any point.
#define FIXED_COVERAGE_FACTOR float(512)
#define FIXED_COVERAGE_INVERSE_FACTOR float(0.001953125)
#define FIXED_COVERAGE_ZERO float(1 << 15)
#define FIXED_COVERAGE_ONE (FIXED_COVERAGE_FACTOR + FIXED_COVERAGE_ZERO)

// Binding points for storage buffers.
#define PAINT_STORAGE_BUFFER_IDX 8
#define PAINT_MATRIX_STORAGE_BUFFER_IDX 9
#define PAINT_TRANSLATE_STORAGE_BUFFER_IDX 10
#define CLIPRECT_MATRIX_STORAGE_BUFFER_IDX 11
#define CLIPRECT_TRANSLATE_STORAGE_BUFFER_IDX 12

// Indices for SPIRV specialization constants (used in lieu of #defines in Vulkan.)
#define CLIPPING_SPECIALIZATION_IDX 0
#define CLIP_RECT_SPECIALIZATION_IDX 1
#define ADVANCED_BLEND_SPECIALIZATION_IDX 2
#define EVEN_ODD_SPECIALIZATION_IDX 3
#define NESTED_CLIPPING_SPECIALIZATION_IDX 4
#define HSL_BLEND_MODES_SPECIALIZATION_IDX 5
#define SPECIALIZATION_COUNT 6
