/*
 * Copyright 2022 Rive
 */

#define TESS_TEXTURE_WIDTH 2048.
#define TESS_TEXTURE_WIDTH_LOG2 11

// Flags that must stay in sync with pls.hpp.
#define RETROFITTED_TRIANGLE_FLAG (1u << 31)
#define CULL_EXCESS_TESSELLATION_SEGMENTS_FLAG (1u << 30)
#define JOIN_TYPE_MASK (3u << 28)
#define MITER_CLIP_JOIN (3u << 28)
#define MITER_REVERT_JOIN (2u << 28)
#define BEVEL_JOIN (1u << 28)
#define EMULATED_STROKE_CAP_FLAG (1u << 27)

// Internal flags.
#define MIRRORED_CONTOUR_FLAG (1u << 26)
#define JOIN_TANGENT_0_FLAG (1u << 25)
#define JOIN_TANGENT_INNER_FLAG (1u << 24)
#define LEFT_JOIN_FLAG (1u << 23)
#define RIGHT_JOIN_FLAG (1u << 22)
#define CONTOUR_ID_MASK 0xffffu

#define PI 3.141592653589793238

#define GRAD_TEXTURE_WIDTH 512.

#define EVEN_ODD_FLAG (1u << 31)
#define SOLID_COLOR_PAINT_TYPE 0u
#define LINEAR_GRADIENT_PAINT_TYPE 1u
#define RADIAL_GRADIENT_PAINT_TYPE 2u
#define IMAGE_PAINT_TYPE 3u
#define CLIP_UPDATE_PAINT_TYPE 4u

// Index at which we access each texture.
#define TESS_VERTEX_TEXTURE_IDX 0
#define PATH_TEXTURE_IDX 1
#define CONTOUR_TEXTURE_IDX 2
#define GRAD_TEXTURE_IDX 3
#define IMAGE_TEXTURE_IDX 4

// Index at which we access each uniform buffer.
#define FLUSH_UNIFORM_BUFFER_IDX 0
#define IMAGE_MESH_UNIFORM_BUFFER_IDX 1
#define BASE_INSTANCE_UNIFORM_BUFFER_IDX 2

// acos(1/4), because the miter limit is always 4.
#define MITER_ANGLE_LIMIT 1.318116071652817965746

// Raw bit representation of the largest denormalized fp16 value. We offset all (1-based) path IDs
// by this value in order to avoid denorms, which have been empirically unreliable on Android as ID
// values.
#define MAX_DENORM_F16 1023u
