/*
 * Copyright 2022 Rive
 */

// Common definitions and functions shared by multiple shaders.

#define PI 3.14159265359
#define _2PI 6.28318530718
#define PI_OVER_2 1.57079632679
#define ONE_OVER_SQRT_2 0.70710678118 // 1/sqrt(2)

#ifndef @RENDER_MODE_MSAA
#define AA_RADIUS float(.5)
#else
#define AA_RADIUS float(.0)
#endif

// Defined as a macro because 'uniforms' isn't always available at global scope.
#define RENDER_TARGET_COORD_TO_CLIP_COORD(COORD)                               \
    pixel_coord_to_clip_coord(COORD,                                           \
                              uniforms.renderTargetInverseViewportX,           \
                              uniforms.renderTargetInverseViewportY)

#ifdef @TESS_TEXTURE_FLOATING_POINT
#define TEXTURE_TESSDATA4(SET, IDX, NAME) TEXTURE_RGBA32F(SET, IDX, NAME)
#define TESSDATA4 float4
#define FLOAT_AS_TESSDATA(X) X
#define TESSDATA_AS_FLOAT(X) X
#define UINT_AS_TESSDATA(X) uintBitsToFloat(X)
#define TESSDATA_AS_UINT(X) floatBitsToUint(X)
#else
#define TEXTURE_TESSDATA4(SET, IDX, NAME) TEXTURE_RGBA32UI(SET, IDX, NAME)
#define TESSDATA4 uint4
#define FLOAT_AS_TESSDATA(X) floatBitsToUint(X)
#define TESSDATA_AS_FLOAT(X) uintBitsToFloat(X)
#define UINT_AS_TESSDATA(X) X
#define TESSDATA_AS_UINT(X) X
#endif

// Gathers a 4xN matrix of texels, in the same order as the textureGather() API.
// clang-format off
#define TEXTURE_GATHER_MATRIX(NAME, COORD, COMPONENTS)                         \
    TEXEL_FETCH(NAME, int2(COORD) + int2(-1, 0))COMPONENTS,                    \
        TEXEL_FETCH(NAME, int2(COORD) + int2(0, 0))COMPONENTS,                 \
        TEXEL_FETCH(NAME, int2(COORD) + int2(0, -1))COMPONENTS,                \
        TEXEL_FETCH(NAME, int2(COORD) + int2(-1, -1))COMPONENTS
// clang-format on

// This is a macro because we can't (at least for now) forward texture refs to a
// function in a way that works in all the languages we support.
// This is a macro because we can't (at least for now) forward texture refs to a
// function in a way that works in all the languages we support.
#define FEATHER(X)                                                             \
    TEXTURE_SAMPLE_LOD_1D_ARRAY(@featherTexture,                               \
                                featherSampler,                                \
                                X,                                             \
                                FEATHER_FUNCTION_ARRAY_INDEX,                  \
                                float(FEATHER_FUNCTION_ARRAY_INDEX),           \
                                .0)                                            \
        .r
#define INVERSE_FEATHER(X)                                                     \
    TEXTURE_SAMPLE_LOD_1D_ARRAY(@featherTexture,                               \
                                featherSampler,                                \
                                X,                                             \
                                FEATHER_INVERSE_FUNCTION_ARRAY_INDEX,          \
                                float(FEATHER_INVERSE_FUNCTION_ARRAY_INDEX),   \
                                .0)                                            \
        .r

#ifdef GLSL
// GLSL has different semantics around precision. Normalize type conversions
// across languages with "cast_*_to_*()" methods.
INLINE half cast_float_to_half(float x) { return x; }
INLINE half cast_uint_to_half(uint x) { return float(x); }
INLINE half cast_ushort_to_half(ushort x) { return float(x); }
INLINE half cast_int_to_half(int x) { return float(x); }
INLINE half4 cast_float4_to_half4(float4 xyzw) { return xyzw; }
INLINE half2 cast_float2_to_half2(float2 xy) { return xy; }
INLINE half4 cast_uint4_to_half4(uint4 xyzw) { return vec4(xyzw); }
INLINE ushort cast_half_to_ushort(half x) { return uint(x); }
INLINE ushort cast_uint_to_ushort(uint x) { return x; }
#else
INLINE half cast_float_to_half(float x) { return (half)x; }
INLINE half cast_uint_to_half(uint x) { return (half)x; }
INLINE half cast_ushort_to_half(ushort x) { return (half)x; }
INLINE half cast_int_to_half(int x) { return (half)x; }
INLINE half4 cast_float4_to_half4(float4 xyzw) { return (half4)xyzw; }
INLINE half2 cast_float2_to_half2(float2 xy) { return (half2)xy; }
INLINE half4 cast_uint4_to_half4(uint4 xyzw) { return (half4)xyzw; }
INLINE ushort cast_half_to_ushort(half x) { return (ushort)x; }
INLINE ushort cast_uint_to_ushort(uint x) { return (ushort)x; }
#endif

INLINE half make_half(half x) { return x; }

INLINE half2 make_half2(half2 xy) { return xy; }

INLINE half2 make_half2(half x, half y)
{
    half2 ret;
    ret.x = x, ret.y = y;
    return ret;
}

INLINE half2 make_half2(half x)
{
    half2 ret;
    ret.x = x, ret.y = x;
    return ret;
}

INLINE float2 make_float2(float x) { return float2(x, x); }

INLINE half3 make_half3(half x, half y, half z)
{
    half3 ret;
    ret.x = x, ret.y = y, ret.z = z;
    return ret;
}

INLINE half3 make_half3(half x)
{
    half3 ret;
    ret.x = x, ret.y = x, ret.z = x;
    return ret;
}

INLINE half4 make_half4(half x, half y, half z, half w)
{
    half4 ret;
    ret.x = x, ret.y = y, ret.z = z, ret.w = w;
    return ret;
}

INLINE half4 make_half4(half3 xyz, half w)
{
    half4 ret;
    ret.xyz = xyz;
    ret.w = w;
    return ret;
}

INLINE half4 make_half4(half x)
{
    half4 ret;
    ret.x = x, ret.y = x, ret.z = x, ret.w = x;
    return ret;
}

INLINE half4 make_half4(half4 x) { return x; }

INLINE bool2 make_bool2(bool b) { return bool2(b, b); }

INLINE half3x3 make_half3x3(half3 a, half3 b, half3 c)
{
    half3x3 ret;
    ret[0] = a;
    ret[1] = b;
    ret[2] = c;
    return ret;
}

INLINE half2x3 make_half2x3(half3 a, half3 b)
{
    half2x3 ret;
    ret[0] = a;
    ret[1] = b;
    return ret;
}

INLINE half4x4 make_half4x4(half4 a, half4 b, half4 c, half4 d)
{
    half4x4 ret;
    ret[0] = a;
    ret[1] = b;
    ret[2] = c;
    ret[3] = d;
    return ret;
}

INLINE float2x2 make_float2x2(float4 x) { return float2x2(x.xy, x.zw); }

INLINE uint make_uint(ushort x) { return x; }

INLINE float2 unchecked_mix(float2 a, float2 b, float t)
{
    return (b - a) * t + a;
}

INLINE half id_bits_to_f16(uint idBits, uint pathIDGranularity)
{
    return idBits == 0u
               ? .0
               : unpackHalf2x16((idBits + MAX_DENORM_F16) * pathIDGranularity)
                     .r;
}

INLINE float atan2(float2 v)
{
    v = normalize(v);
    float theta = acos(clamp(v.x, -1., 1.));
    return v.y >= .0 ? theta : -theta;
}

INLINE half4 premultiply(half4 color)
{
    return make_half4(color.rgb * color.a, color.a);
}

INLINE half3 unmultiply_rgb(half4 premul)
{
    // We *could* return preciesly 1 when premul.rgb == premul.a, but we can
    // also be approximate here. The blend modes that depend on this exact level
    // of precision (colordodge and colorburn) account for it with dstPremul.
    return premul.rgb * (premul.a != .0 ? 1. / premul.a : .0);
}

INLINE half min_value(half4 min4)
{
    half2 min2 = min(min4.xy, min4.zw);
    half min1 = min(min2.x, min2.y);
    return min1;
}

INLINE float manhattan_width(float2 x) { return abs(x.x) + abs(x.y); }

#ifndef $UNIFORM_DEFINITIONS_AUTO_GENERATED
UNIFORM_BLOCK_BEGIN(FLUSH_UNIFORM_BUFFER_IDX, @FlushUniforms)
float gradInverseViewportY;
float tessInverseViewportY;
float renderTargetInverseViewportX;
float renderTargetInverseViewportY;
uint renderTargetWidth;
uint renderTargetHeight;
uint colorClearValue;           // Only used if clears are implemented as draws.
uint coverageClearValue;        // Only used if clears are implemented as draws.
int4 renderTargetUpdateBounds;  // drawBounds, or renderTargetBounds if there is
                                // a clear. (LTRB.)
float2 atlasTextureInverseSize; // 1 / [atlasWidth, atlasHeight]
float2 atlasContentInverseViewport; // 2 / atlasContentBounds
uint coverageBufferPrefix;
uint pathIDGranularity; // Spacing between adjacent path IDs (1 if IEEE
                        // compliant).
float vertexDiscardValue;
// Debugging.
uint wireframeEnabled;
UNIFORM_BLOCK_END(uniforms)
#endif

#ifdef @VERTEX

INLINE float4 pixel_coord_to_clip_coord(float2 pixelCoord,
                                        float inverseViewportX,
                                        float inverseViewportY)
{
    return float4(pixelCoord.x * inverseViewportX - 1.,
                  pixelCoord.y * inverseViewportY - sign(inverseViewportY),
                  0.,
                  1.);
}

#ifndef @RENDER_MODE_MSAA
// Calculates the Manhattan distance in pixels from the given pixelPosition, to
// the point at each edge of the clipRect where coverage = 0.
//
// clipRectInverseMatrix transforms from pixel coordinates to a space where the
// clipRect is the normalized rectangle: [-1, -1, 1, 1].
INLINE float4 find_clip_rect_coverage_distances(float2x2 clipRectInverseMatrix,
                                                float2 clipRectInverseTranslate,
                                                float2 pixelPosition)
{
    float2 clipRectAAWidth =
        abs(clipRectInverseMatrix[0]) + abs(clipRectInverseMatrix[1]);
    if (clipRectAAWidth.x != .0 && clipRectAAWidth.y != .0)
    {
        float2 r = 1. / clipRectAAWidth;
        float2 clipRectCoord = MUL(clipRectInverseMatrix, pixelPosition) +
                               clipRectInverseTranslate;
        // When the center of a pixel falls exactly on an edge, coverage should
        // be .5.
        const float coverageWhenDistanceIsZero = .5;
        return float4(clipRectCoord, -clipRectCoord) * r.xyxy + r.xyxy +
               coverageWhenDistanceIsZero;
    }
    else
    {
        // The caller gave us a singular clipRectInverseMatrix. This is a
        // special case where we are expected to use tx and ty as uniform
        // coverage.
        return clipRectInverseTranslate.xyxy;
    }
}

#else // !@RENDER_MODE_MSAA => @RENDER_MODE_MSAA

INLINE float normalize_z_index(uint zIndex)
{
    return 1. - float(zIndex) * (2. / 32768.);
}

#ifdef @ENABLE_CLIP_RECT
INLINE void set_clip_rect_plane_distances(float2x2 clipRectInverseMatrix,
                                          float2 clipRectInverseTranslate,
                                          float2 pixelPosition)
{
    if (clipRectInverseMatrix != float2x2(0))
    {
        float2 clipRectCoord = MUL(clipRectInverseMatrix, pixelPosition) +
                               clipRectInverseTranslate.xy;
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
        gl_ClipDistance[0] = gl_ClipDistance[1] = gl_ClipDistance[2] =
            gl_ClipDistance[3] = clipRectInverseTranslate.x - .5;
    }
}
#endif // ENABLE_CLIP_RECT

#endif // @RENDER_MODE_MSAA
#endif // VERTEX

#ifdef @FRAGMENT
#ifdef @NEEDS_GAMMA_CORRECTION
half gamma_to_linear(half color)
{
    return (color <= 0.04045) ? color / 12.92
                              : pow(abs((color + 0.055) / 1.055), 2.4);
}

half3 gamma_to_linear(half3 color)
{
    return make_half3(gamma_to_linear(color.r),
                      gamma_to_linear(color.g),
                      gamma_to_linear(color.b));
}

half4 gamma_to_linear(half4 color)
{
    return make_half4(gamma_to_linear(color.rgb), color.a);
}
#endif // NEEDS_GAMMA_CORRECTION
#endif // FRAGMENT

#ifdef @DRAW_IMAGE
#ifndef $UNIFORM_DEFINITIONS_AUTO_GENERATED
UNIFORM_BLOCK_BEGIN(IMAGE_DRAW_UNIFORM_BUFFER_IDX, @ImageDrawUniforms)
float4 viewMatrix;
float2 translate;
float opacity;
float padding;
// clipRectInverseMatrix transforms from pixel coordinates to a space where the
// clipRect is the normalized rectangle: [-1, -1, 1, 1].
float4 clipRectInverseMatrix;
float2 clipRectInverseTranslate;
uint clipID;
uint blendMode;
uint zIndex;
UNIFORM_BLOCK_END(imageDrawUniforms)
#endif
#endif
