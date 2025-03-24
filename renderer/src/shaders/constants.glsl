/*
 * Copyright 2022 Rive
 */

#define TESS_TEXTURE_WIDTH float(2048)
#define TESS_TEXTURE_WIDTH_LOG2 11

#define GRAD_TEXTURE_WIDTH float(512)
#define GRAD_TEXTURE_INVERSE_WIDTH float(0.001953125)

// Number of standard deviations on either side of the middle of the feather
// texture. The feather texture integrates the normal distribution from
// -FEATHER_TEXTURE_STDDEVS to +FEATHER_TEXTURE_STDDEVS in the domain x=0..1.
#define FEATHER_TEXTURE_STDDEVS float(3)

// Indices of function tables in the feather texture1d array.
// NOTE: This will be a texture2d if texture1d isn't supported.
#define FEATHER_FUNCTION_ARRAY_INDEX 0
#define FEATHER_INVERSE_FUNCTION_ARRAY_INDEX 1
#define FEATHER_TEXTURE_1D_ARRAY_LENGTH 2

// Number of additional tessellation "helper" vertices that need to be allocated
// for a feather join.
#define FEATHER_JOIN_HELPER_VERTEX_COUNT 3u

// Amount to increase "joinSegmentCount" in a feather join so the number of
// literal vertices allocated increases by FEATHER_JOIN_HELPER_VERTEX_COUNT.
#define FEATHER_JOIN_HELPER_SEGMENT_COUNT                                      \
    (FEATHER_JOIN_HELPER_VERTEX_COUNT + 1u)

// Feather joins are split into a backward and forward section. Both sections
// need at least one segment, thus a minimum of 2 (plus helper vertices).
#define FEATHER_JOIN_MIN_SEGMENT_COUNT (2u + FEATHER_JOIN_HELPER_SEGMENT_COUNT)

// Width to use for a texture that emulates a storage buffer.
//
// Minimize width since the texture needs to be updated in entire rows from the
// resource buffer. Since these only serve paths and contours, both of those are
// limited to 16-bit indices, 2048 is the min specified texture size in ES3, and
// no path buffer uses more than 4 texels, we can safely use a width of 128.
#define STORAGE_TEXTURE_WIDTH 128
#define STORAGE_TEXTURE_SHIFT_Y 7
#define STORAGE_TEXTURE_MASK_X 0x7fu

// Flags that state whether/how we need to render solid-color borders to the
// left and/or right side of a GradientSpan. (Borders of complex gradients
// stretch all the way to the left/right edges of the texture, whereas borders
// of simple gradients just need to stretch 1px to the left/right of the
// span.)
#define GRAD_SPAN_FLAG_LEFT_BORDER 0x80000000u
#define GRAD_SPAN_FLAG_RIGHT_BORDER 0x40000000u
#define GRAD_SPAN_FLAG_COMPLEX_BORDER 0x20000000u
#define GRAD_SPAN_FLAGS_MASK                                                   \
    (GRAD_SPAN_FLAG_LEFT_BORDER | GRAD_SPAN_FLAG_RIGHT_BORDER |                \
     GRAD_SPAN_FLAG_COMPLEX_BORDER)

// Tells shaders that a cubic should actually be drawn as the single, non-AA
// triangle: [p0, p1, p3]. This is used to squeeze in more rare triangles, like
// "grout" triangles from self intersections on interior triangulation, where it
// wouldn't be worth it to put them in their own dedicated draw call.
#define RETROFITTED_TRIANGLE_CONTOUR_FLAG (1u << 31u)

// Tells the tessellation shader to re-run Wang's formula on the given curve,
// figure out how many segments it actually needs, and make any excess segments
// degenerate by co-locating their vertices at T=0. (Used on the "outerCurve"
// patches that are drawn with interior triangulations.)
#define CULL_EXCESS_TESSELLATION_SEGMENTS_CONTOUR_FLAG (1u << 30u)

// Flags for specifying the join type.
#define JOIN_TYPE_MASK (7u << 27u)
#define MITER_CLIP_JOIN_CONTOUR_FLAG (5u << 27u)
#define MITER_REVERT_JOIN_CONTOUR_FLAG (4u << 27u)
#define BEVEL_JOIN_CONTOUR_FLAG (3u << 27u)
#define ROUND_JOIN_CONTOUR_FLAG (2u << 27u)
#define FEATHER_JOIN_CONTOUR_FLAG (1u << 27u)

// When a join is being used to emulate a stroke cap, the shader emits
// additional vertices at T=0 and T=1 for round joins, and changes the miter
// limit to 1 for miter-clip joins.
#define EMULATED_STROKE_CAP_CONTOUR_FLAG (1u << 26u)

// Flip the sign on interpolated fragment coverage for fills. Ignored on
// strokes. This is used when reversing the winding direction of a path.
#define NEGATE_PATH_FILL_COVERAGE_FLAG (1u << 25u)

// Internal contour flags.
#define MIRRORED_CONTOUR_CONTOUR_FLAG (1u << 24u)
#define JOIN_TANGENT_0_CONTOUR_FLAG (1u << 23u)
#define JOIN_TANGENT_INNER_CONTOUR_FLAG (1u << 22u)
#define LEFT_JOIN_CONTOUR_FLAG (1u << 21u)
#define RIGHT_JOIN_CONTOUR_FLAG (1u << 20u)
#define CONTOUR_ID_MASK 0xffffu

// This is guaranteed to not collide with a neighboring contour ID.
#define INVALID_CONTOUR_ID_WITH_FLAGS 0u

// Says which part of the patch a vertex belongs to.
#define STROKE_VERTEX 0
#define FAN_VERTEX 1
#define FAN_MIDPOINT_VERTEX 2

// Says which part of the patch a vertex belongs to.
#define STROKE_VERTEX 0
#define FAN_VERTEX 1
#define FAN_MIDPOINT_VERTEX 2

// Mirrors pls::PaintType.
#define CLIP_UPDATE_PAINT_TYPE 0u
#define SOLID_COLOR_PAINT_TYPE 1u
#define LINEAR_GRADIENT_PAINT_TYPE 2u
#define RADIAL_GRADIENT_PAINT_TYPE 3u
#define IMAGE_PAINT_TYPE 4u

// Paint flags, found in the x-component value of @paintBuffer.
#define PAINT_FLAG_NON_ZERO_FILL 0x100u
#define PAINT_FLAG_EVEN_ODD_FILL 0x200u
#define PAINT_FLAG_HAS_CLIP_RECT 0x400u

// PLS draw resources are either updated per flush or per draw. They go into set
// 0 or set 1, depending on how often they are updated.
#define PER_FLUSH_BINDINGS_SET 0
#define PER_DRAW_BINDINGS_SET 1

// Index at which we access each resource.
// (Enumerate buffers first because GLES allows a hard limit on buffer index
// bindings as low as 7.)
#define FLUSH_UNIFORM_BUFFER_IDX 0
#define PATH_BASE_INSTANCE_UNIFORM_BUFFER_IDX 1
#define IMAGE_DRAW_UNIFORM_BUFFER_IDX 2
#define PATH_BUFFER_IDX 3
#define PAINT_BUFFER_IDX 4
#define PAINT_AUX_BUFFER_IDX 5
#define CONTOUR_BUFFER_IDX 6
// Coverage buffer used in coverageAtomic mode.
#define COVERAGE_BUFFER_IDX 7
#define TESS_VERTEX_TEXTURE_IDX 8
#define GRAD_TEXTURE_IDX 9
#define FEATHER_TEXTURE_IDX 10
#define ATLAS_TEXTURE_IDX 11
#define IMAGE_TEXTURE_IDX 12
#define DST_COLOR_TEXTURE_IDX 13
#define DEFAULT_BINDINGS_SET_SIZE 14

// Metal doesn't allow us to bind buffers index 0 or 1. Offset them by 2.
#define METAL_BUFFER_IDX(IDX) (2 + IDX)

// Samplers are accessed at the same index as their corresponding texture, so we
// put them in a separate binding set.
#define SAMPLER_BINDINGS_SET 2

// PLS textures are accessed at the same index as their PLS planes, so we put
// them in a separate binding set.
#define PLS_TEXTURE_BINDINGS_SET 3

#define BINDINGS_SET_COUNT 4

// Index of each pixel local storage plane.
#define COLOR_PLANE_IDX 0
#define CLIP_PLANE_IDX 1
#define SCRATCH_COLOR_PLANE_IDX 2
#define COVERAGE_PLANE_IDX 3
#define PIXEL_LOCAL_STORAGE_PLANE_COUNT 4
#define DEPTH_STENCIL_IDX PIXEL_LOCAL_STORAGE_PLANE_COUNT

// Rive has a hard-coded miter limit of 4 in the editor and all runtimes.
#define RIVE_MITER_LIMIT float(4)
// acos(1/4), because the miter limit is 4.
#define MITER_ANGLE_LIMIT float(1.318116071652817965746)

// Raw bit representation of the largest denormalized fp16 value. We offset all
// (1-based) path IDs by this value in order to avoid denorms, which have been
// empirically unreliable on Android as ID values.
#define MAX_DENORM_F16 1023u

// Blend modes. Mirrors rive::BlendMode, but 0-based and contiguous for tighter
// packing.
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

// Fixed-point coverage values for atomic mode.
// Atomic mode uses 6:11 fixed point, so the winding number breaks if a shape
// has more than 32 levels of self overlap in either winding direction at any
// point.
#define FIXED_COVERAGE_PRECISION float(2048)
#define FIXED_COVERAGE_INVERSE_PRECISION float(0.00048828125)
#define FIXED_COVERAGE_ZERO float(1 << 16)
#define FIXED_COVERAGE_ZERO_UINT (1u << 16)
#define FIXED_COVERAGE_ONE (FIXED_COVERAGE_PRECISION + FIXED_COVERAGE_ZERO)
#define FIXED_COVERAGE_BIT_COUNT 17u
#define FIXED_COVERAGE_MASK 0x1ffffu

// Fixed-point coverage values for clockwiseAtomic mode.
// clockwiseAtomic mode uses 6:11 fixed point, so the winding number breaks if a
// shape has more than 32 levels of self overlap in either winding direction at
// any point.
#define CLOCKWISE_COVERAGE_BIT_COUNT 17u
#define CLOCKWISE_COVERAGE_MASK 0x1ffffu
#define CLOCKWISE_COVERAGE_PRECISION float(2048)
#define CLOCKWISE_COVERAGE_INVERSE_PRECISION float(0.00048828125)
#define CLOCKWISE_FILL_ZERO_VALUE (1u << 16)

// Vendor IDs for driver workarounds.
#define VULKAN_VENDOR_AMD 0x1002u
#define VULKAN_VENDOR_IMG_TEC 0x1010u
#define VULKAN_VENDOR_NVIDIA 0x10DEu
#define VULKAN_VENDOR_ARM 0x13B5u
#define VULKAN_VENDOR_QUALCOMM 0x5143u
#define VULKAN_VENDOR_INTEL 0x8086u

// Indices for SPIRV specialization constants (used in lieu of #defines in
// Vulkan.)
#define CLIPPING_SPECIALIZATION_IDX 0
#define CLIP_RECT_SPECIALIZATION_IDX 1
#define ADVANCED_BLEND_SPECIALIZATION_IDX 2
#define FEATHER_SPECIALIZATION_IDX 3
#define EVEN_ODD_SPECIALIZATION_IDX 4
#define NESTED_CLIPPING_SPECIALIZATION_IDX 5
#define HSL_BLEND_MODES_SPECIALIZATION_IDX 6
#define CLOCKWISE_FILL_SPECIALIZATION_IDX 7
#define BORROWED_COVERAGE_PREPASS_SPECIALIZATION_IDX 8
#define VULKAN_VENDOR_ID_SPECIALIZATION_IDX 9
#define SPECIALIZATION_COUNT 10
