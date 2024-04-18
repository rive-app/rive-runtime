/*
 * Copyright 2022 Rive
 */

// Common functions shared by multiple shaders.

#define PI float(3.141592653589793238)

#ifndef @USING_DEPTH_STENCIL
#define AA_RADIUS float(.5)
#else
#define AA_RADIUS float(.0)
#endif

INLINE uint contour_data_idx(uint contourIDWithFlags)
{
    return (contourIDWithFlags & CONTOUR_ID_MASK) - 1u;
}

INLINE float2 unchecked_mix(float2 a, float2 b, float t) { return (b - a) * t + a; }

INLINE half id_bits_to_f16(uint idBits, uint pathIDGranularity)
{
    return idBits == 0u ? .0 : unpackHalf2x16((idBits + MAX_DENORM_F16) * pathIDGranularity).r;
}

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

INLINE half4 premultiply(half4 color) { return make_half4(color.rgb * color.a, color.a); }

INLINE half4 unmultiply(half4 color)
{
    if (color.a != .0)
        color.rgb *= 1.0 / color.a;
    return color;
}

INLINE float2x2 make_float2x2(float4 x) { return float2x2(x.xy, x.zw); }
INLINE uint make_uint(ushort x) { return x; }

INLINE half min_value(half4 min4)
{
    half2 min2 = min(min4.xy, min4.zw);
    half min1 = min(min2.x, min2.y);
    return min1;
}

INLINE float manhattan_width(float2 x) { return abs(x.x) + abs(x.y); }

#ifdef @VERTEX
UNIFORM_BLOCK_BEGIN(FLUSH_UNIFORM_BUFFER_IDX, @FlushUniforms)
float gradInverseViewportY;
float tessInverseViewportY;
float renderTargetInverseViewportX;
float renderTargetInverseViewportY;
uint renderTargetWidth;
uint renderTargetHeight;
uint colorClearValue;          // Only used if clears are implemented as draws.
uint coverageClearValue;       // Only used if clears are implemented as draws.
int4 renderTargetUpdateBounds; // drawBounds, or renderTargetBounds if there is a clear. (LTRB.)
uint pathIDGranularity;        // Spacing between adjacent path IDs (1 if IEEE compliant).
float vertexDiscardValue;
UNIFORM_BLOCK_END(uniforms)

#define RENDER_TARGET_COORD_TO_CLIP_COORD(COORD)                                                   \
    float4((COORD).x* uniforms.renderTargetInverseViewportX - 1.,                                  \
           (COORD).y * -uniforms.renderTargetInverseViewportY +                                    \
               sign(uniforms.renderTargetInverseViewportY),                                        \
           .0,                                                                                     \
           1.)

#ifndef @USING_DEPTH_STENCIL
// Calculates the Manhattan distance in pixels from the given pixelPosition, to the point at each
// edge of the clipRect where coverage = 0.
//
// clipRectInverseMatrix transforms from pixel coordinates to a space where the clipRect is the
// normalized rectangle: [-1, -1, 1, 1].
INLINE float4 find_clip_rect_coverage_distances(float2x2 clipRectInverseMatrix,
                                                float2 clipRectInverseTranslate,
                                                float2 pixelPosition)
{
    float2 clipRectAAWidth = abs(clipRectInverseMatrix[0]) + abs(clipRectInverseMatrix[1]);
    if (clipRectAAWidth.x != .0 && clipRectAAWidth.y != .0)
    {
        float2 r = 1. / clipRectAAWidth;
        float2 clipRectCoord = MUL(clipRectInverseMatrix, pixelPosition) + clipRectInverseTranslate;
        // When the center of a pixel falls exactly on an edge, coverage should be .5.
        const float coverageWhenDistanceIsZero = .5;
        return float4(clipRectCoord, -clipRectCoord) * r.xyxy + r.xyxy + coverageWhenDistanceIsZero;
    }
    else
    {
        // The caller gave us a singular clipRectInverseMatrix. This is a special case where we are
        // expected to use tx and ty as uniform coverage.
        return clipRectInverseTranslate.xyxy;
    }
}

#else // USING_DEPTH_STENCIL

INLINE float normalize_z_index(uint zIndex) { return 1. - float(zIndex) * (2. / 32768.); }

#ifdef @ENABLE_CLIP_RECT
INLINE void set_clip_rect_plane_distances(float2x2 clipRectInverseMatrix,
                                          float2 clipRectInverseTranslate,
                                          float2 pixelPosition)
{
    if (clipRectInverseMatrix != float2x2(0))
    {
        float2 clipRectCoord =
            MUL(clipRectInverseMatrix, pixelPosition) + clipRectInverseTranslate.xy;
        gl_ClipDistance[0] = clipRectCoord.x + 1.;
        gl_ClipDistance[1] = clipRectCoord.y + 1.;
        gl_ClipDistance[2] = 1. - clipRectCoord.x;
        gl_ClipDistance[3] = 1. - clipRectCoord.y;
    }
    else
    {
        // "clipRectInverseMatrix == 0" is a special case:
        //     "clipRectInverseTranslate.x == 1" => all in.
        //     "clipRectInverseTranslate.x == 0" => all out.
        gl_ClipDistance[0] = gl_ClipDistance[1] = gl_ClipDistance[2] = gl_ClipDistance[3] =
            clipRectInverseTranslate.x - .5;
    }
}
#endif // ENABLE_CLIP_RECT
#endif // USING_DEPTH_STENCIL
#endif // VERTEX

#ifdef @DRAW_IMAGE
UNIFORM_BLOCK_BEGIN(IMAGE_DRAW_UNIFORM_BUFFER_IDX, @ImageDrawUniforms)
float4 viewMatrix;
float2 translate;
float opacity;
float padding;
// clipRectInverseMatrix transforms from pixel coordinates to a space where the clipRect is the
// normalized rectangle: [-1, -1, 1, 1].
float4 clipRectInverseMatrix;
float2 clipRectInverseTranslate;
uint clipID;
uint blendMode;
uint zIndex;
UNIFORM_BLOCK_END(imageDrawUniforms)
#endif
