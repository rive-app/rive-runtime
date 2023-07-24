/*
 * Copyright 2023 Rive
 */

// This header provides GLSL-specific #defines and declarations that enable our shaders to be
// compiled on MSL and GLSL both.

// HLSL warns that it will unroll the loops through r,g,b values in advanced_blend.glsl, but
// unrolling these loops is exactly what we want.
#pragma warning(disable : 3550)

// #define native hlsl types if their names are being rewritten.
#define _ARE_TOKEN_NAMES_PRESERVED
#ifndef $_ARE_TOKEN_NAMES_PRESERVED
#define half $half
#define half2 $half2
#define half3 $half3
#define half4 $half4
#define short $short
#define short2 $short2
#define short3 $short3
#define short4 $short4
#define ushort $ushort
#define ushort2 $ushort2
#define ushort3 $ushort3
#define ushort4 $ushort4
#define float2 $float2
#define float3 $float3
#define float4 $float4
#define bool2 $bool2
#define bool3 $bool3
#define bool4 $bool4
#define uint2 $uint2
#define uint3 $uint3
#define uint4 $uint4
#define int2 $int2
#define int3 $int3
#define int4 $int4
#define float4x2 $float4x2
#define ushort $ushort
#define float2x2 $float2x2
#define half3x4 $half3x4
#endif

$typedef float3 packed_float3;
#define make_half float
#define make_half2 float2
#define make_half3 float3
#define make_half4 float4

$typedef $min16int short;
#define make_short $min16int
#define make_short2 $min16int2
#define make_short3 $min16int3
#define make_short4 $min16int4

$typedef $min16uint ushort;
#define make_ushort $min16uint
#define make_ushort2 $min16uint2
#define make_ushort3 $min16uint3
#define make_ushort4 $min16uint4

#define make_half3x4 $half3x4

#define ATTR_BLOCK_BEGIN(NAME)                                                                     \
    struct NAME                                                                                    \
    {
#define ATTR(IDX, TYPE, NAME) TYPE NAME : NAME
#define ATTR_BLOCK_END                                                                             \
    }                                                                                              \
    ;
#define ATTR_LOAD(T, A, N, I)
#define ATTR_UNPACK(ID, attrs, NAME, TYPE) TYPE NAME = attrs.NAME

#define UNIFORM_BLOCK_BEGIN(NAME)                                                                  \
    $cbuffer NAME : $register($b0)                                                                 \
    {                                                                                              \
        struct                                                                                     \
        {

#define UNIFORM_BLOCK_END(NAME)                                                                    \
    }                                                                                              \
    NAME;                                                                                          \
    }

#define VARYING_BLOCK_BEGIN(NAME)                                                                  \
    struct NAME                                                                                    \
    {

#define NO_PERSPECTIVE $noperspective
#define @OPTIONALLY_FLAT $nointerpolation
#define FLAT $nointerpolation
#define VARYING(IDX, TYPE, NAME) TYPE NAME : $TEXCOORD##IDX

#define VARYING_BLOCK_END(_pos)                                                                    \
    float4 _pos : $SV_Position;                                                                    \
    }                                                                                              \
    ;

#define VARYING_INIT(varyings, NAME, TYPE) TYPE NAME
#define VARYING_PACK(varyings, NAME) varyings.NAME = NAME
#define VARYING_UNPACK(varyings, NAME, TYPE) TYPE NAME = varyings.NAME

#ifdef @VERTEX
#define VERTEX_TEXTURE_BLOCK_BEGIN(NAME)
#define VERTEX_TEXTURE_BLOCK_END
#endif

#ifdef @FRAGMENT
#define FRAG_TEXTURE_BLOCK_BEGIN(NAME)
#define FRAG_TEXTURE_BLOCK_END
#endif

#define TEXTURE_RGBA32UI(IDX, NAME) uniform $Texture2D<uint4> NAME : $register($t##IDX)
#define TEXTURE_RGBA32F(IDX, NAME) uniform $Texture2D<float4> NAME : $register($t##IDX)
#define TEXTURE_RGBA8(IDX, NAME) uniform $Texture2D<$unorm float4> NAME : $register($t##IDX)

#define TEXEL_FETCH(TEXTURE_BLOCK, NAME, COORD) NAME[COORD]

#define GRADIENT_SAMPLER_DECL(IDX, NAME) $SamplerState NAME : $register($s##IDX);

#define TEXTURE_SAMPLE(TEXTURE_BLOCK, NAME, SAMPLER_NAME, COORD) NAME.$Sample(SAMPLER_NAME, COORD)

#define PLS_INTERLOCK_BEGIN
#define PLS_INTERLOCK_END

#ifdef @DRAW_INTERIOR_TRIANGLES
// The interior triangles don't overlap, so therefore don't need rasterizer ordering.
#define PLS_TEX2D $RWTexture2D
#else
#define PLS_TEX2D $RasterizerOrderedTexture2D
#endif

#define PLS_BLOCK_BEGIN
#define PLS_DECL4F(IDX, NAME) uniform PLS_TEX2D<$unorm float4> NAME : $register($u##IDX)
#define PLS_DECL2F(IDX, NAME) uniform PLS_TEX2D<uint> NAME : $register($u##IDX)
#define PLS_BLOCK_END

#define PLS_LOAD4F(P) P[_plsCoord]
#define PLS_LOAD2F(P) unpackHalf2x16(P[_plsCoord])
#define PLS_STORE4F(P, V) P[_plsCoord] = V
#define PLS_STORE2F(P, X, Y) P[_plsCoord] = packHalf2x16(float2(X, Y))

#define PLS_PRESERVE_VALUE(P)

#ifdef @ENABLE_BASE_INSTANCE_POLYFILL
#define BASE_INSTANCE_POLYFILL_DECL(NAME)                                                          \
    $cbuffer NAME##_cbuff : $register($b1)                                                         \
    {                                                                                              \
        uint NAME;                                                                                 \
        uint NAME##_pad0;                                                                          \
        uint NAME##_pad1;                                                                          \
        uint NAME##_pad2;                                                                          \
    }

#endif

#define VERTEX_MAIN(NAME,                                                                          \
                    Uniforms,                                                                      \
                    uniforms,                                                                      \
                    Attrs,                                                                         \
                    attrs,                                                                         \
                    Varyings,                                                                      \
                    varyings,                                                                      \
                    VertexTextures,                                                                \
                    textures,                                                                      \
                    _vertexID,                                                                     \
                    _instanceID,                                                                   \
                    _pos)                                                                          \
    Varyings NAME(Attrs attrs, uint _vertexID : $SV_VertexID, uint _instanceID : $SV_InstanceID)   \
    {                                                                                              \
        Varyings varyings;                                                                         \
        float4 _pos;

#define EMIT_VERTEX(varyings, _pos)                                                                \
    }                                                                                              \
    varyings._pos = _pos;                                                                          \
    return varyings;

#define FRAG_DATA_MAIN(DATA_TYPE, NAME, Varyings, varyings)                                        \
    DATA_TYPE NAME(Varyings varyings) : $SV_Target                                                 \
    {

#define EMIT_FRAG_DATA(VALUE)                                                                      \
    return VALUE;                                                                                  \
    }

#define PLS_MAIN(NAME, Varyings, varyings, FragmentTextures, textures, _pos)                       \
    [$earlydepthstencil] void NAME(Varyings varyings) {                                            \
        int2 _plsCoord = int2(floor(varyings._pos.xy));

#define EMIT_PLS }

#define INLINE $inline

#define uintBitsToFloat $asfloat
#define intBitsToFloat $asfloat
#define floatBitsToInt $asint
#define floatBitsToUint $asuint
#define fract $frac
#define mix $lerp
#define inversesqrt $rsqrt
#define notEqual(A, B) ((A) != (B))
#define lessThanEqual(A, B) ((A) <= (B))
#define greaterThanEqual(A, B) ((A) >= (B))

// HLSL matrices are stored in row-major order, and therefore transposed from their counterparts
// in GLSL and Metal. We can work around this entirely by reversing the arguments to mul().
#define MUL(A, B) $mul(B, A)

INLINE half2 unpackHalf2x16(uint u)
{
    uint y = (u >> 16);
    uint x = u & 0xffffu;
    return make_half2($f16tof32(x), $f16tof32(y));
}

INLINE uint packHalf2x16(float2 v)
{
    uint x = $f32tof16(v.x);
    uint y = $f32tof16(v.y);
    return (y << 16) | x;
}

INLINE float atan(float y, float x) { return $atan2(y, x); }

INLINE float2x2 inverse(float2x2 m)
{
    float2x2 adjoint = float2x2(m[1][1], -m[0][1], -m[1][0], m[0][0]);
    return adjoint * (1. / determinant(m));
}
