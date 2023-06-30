/*
 * Copyright 2022 Rive
 */

// Common definitions shared by multiple shaders.

#define TESS_TEXTURE_WIDTH 2048.
#define TESS_TEXTURE_WIDTH_LOG2 11

#define FIRST_VERTEX_OF_CONTOUR_FLAG (1u << 31)
#define JOIN_TYPE_MASK (3u << 29)
#define MITER_CLIP_JOIN (3u << 29)
#define MITER_REVERT_JOIN (2u << 29)
#define BEVEL_JOIN (1u << 29)
#define EMULATED_STROKE_CAP_FLAG (1u << 28)
#define JOIN_TANGENT_0_FLAG (1u << 27)
#define JOIN_TANGENT_INNER_FLAG (1u << 26)
#define LEFT_JOIN_FLAG (1u << 25)
#define RIGHT_JOIN_FLAG (1u << 24)
#define CONTOUR_ID_MASK 0xffffu

#define PI 3.141592653589793238

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
