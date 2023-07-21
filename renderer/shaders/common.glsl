/*
 * Copyright 2022 Rive
 */

// Common definitions shared by multiple shaders.

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
#define CLIP_REPLACE_PAINT_TYPE 3u

// acos(1/4), because the miter limit is always 4.
#define MITER_ANGLE_LIMIT 1.318116071652817965746

// Raw bit representation of the largest denormalized fp16 value. We offset all (1-based) path IDs
// by this value in order to avoid denorms, which have been empirically unreliable on Android as ID
// values.
#define MAX_DENORM_F16 1023u

INLINE int2 contour_texel_coord(uint contourIDWithFlags)
{
    uint contourTexelIdx = (contourIDWithFlags & CONTOUR_ID_MASK) - 1u;
    return int2(contourTexelIdx & 0xffu, contourTexelIdx >> 8);
}

INLINE int2 path_texel_coord(uint pathIDBits)
{
    uint pathIdx = pathIDBits - 1u;
    return int2((pathIdx & 0x7fu) * 3u, pathIdx >> 7);
}

INLINE float2 unchecked_mix(float2 a, float2 b, float t) { return (b - a) * t + a; }

INLINE float atan2(float2 v)
{
    float bias = .0;
    if (abs(v.x) > abs(v.y))
    {
        v = float2(v.y, -v.x);
        bias = PI / 2.;
    }
    return atan(v.y, v.x) + bias;
}

INLINE half4 unmultiply(half4 color)
{
    if (color.a != .0)
        color.rgb *= 1.0 / color.a;
    return color;
}

INLINE float2x2 make_float2x2(float4 f) { return float2x2(f.xy, f.zw); }

#ifdef @VERTEX
UNIFORM_BLOCK_BEGIN(@Uniforms)
float gradInverseViewportY;
float tessInverseViewportY;
float renderTargetInverseViewportX;
float renderTargetInverseViewportY;
float gradTextureInverseHeight;
uint pathIDGranularity; // Spacing between adjacent path IDs (1 if IEEE compliant).
float vertexDiscardValue;
uint pad;
UNIFORM_BLOCK_END(uniforms)
#endif
