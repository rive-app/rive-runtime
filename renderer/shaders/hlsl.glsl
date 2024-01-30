/*
 * Copyright 2023 Rive
 */

// This header provides GLSL-specific #defines and declarations that enable our shaders to be
// compiled on MSL and GLSL both.

// HLSL warns that it will unroll the loops through r,g,b values in advanced_blend.glsl, but
// unrolling these loops is exactly what we want.
#pragma $warning($disable : 3550)

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

#define INLINE $inline
#define OUT(ARG_TYPE) out ARG_TYPE

#define ATTR_BLOCK_BEGIN(NAME)                                                                     \
    struct NAME                                                                                    \
    {
#define ATTR(IDX, TYPE, NAME) TYPE NAME : NAME
#define ATTR_BLOCK_END                                                                             \
    }                                                                                              \
    ;
#define ATTR_LOAD(T, A, N, I)
#define ATTR_UNPACK(ID, attrs, NAME, TYPE) TYPE NAME = attrs.NAME

#define UNIFORM_BUFFER_REGISTER(IDX) $register($b##IDX)

#define UNIFORM_BLOCK_BEGIN(IDX, NAME)                                                             \
    $cbuffer NAME : UNIFORM_BUFFER_REGISTER(IDX)                                                   \
    {                                                                                              \
        struct                                                                                     \
        {

#define UNIFORM_BLOCK_END(NAME)                                                                    \
    }                                                                                              \
    NAME;                                                                                          \
    }

#define VARYING_BLOCK_BEGIN                                                                        \
    struct Varyings                                                                                \
    {

#define NO_PERSPECTIVE $noperspective
#define @OPTIONALLY_FLAT $nointerpolation
#define FLAT $nointerpolation
#define VARYING(IDX, TYPE, NAME) TYPE NAME : $TEXCOORD##IDX

#define VARYING_BLOCK_END                                                                          \
    float4 _pos : $SV_Position;                                                                    \
    }                                                                                              \
    ;

#define VARYING_INIT(NAME, TYPE) TYPE NAME
#define VARYING_PACK(NAME) _varyings.NAME = NAME
#define VARYING_UNPACK(NAME, TYPE) TYPE NAME = _varyings.NAME

#ifdef @VERTEX
#define VERTEX_TEXTURE_BLOCK_BEGIN
#define VERTEX_TEXTURE_BLOCK_END
#endif

#ifdef @FRAGMENT
#define FRAG_TEXTURE_BLOCK_BEGIN
#define FRAG_TEXTURE_BLOCK_END
#endif

#define TEXTURE_RGBA32UI(IDX, NAME) uniform $Texture2D<uint4> NAME : $register($t##IDX)
#define TEXTURE_RGBA32F(IDX, NAME) uniform $Texture2D<float4> NAME : $register($t##IDX)
#define TEXTURE_RGBA8(IDX, NAME) uniform $Texture2D<$unorm float4> NAME : $register($t##IDX)

// SAMPLER_LINEAR and SAMPLER_MIPMAP are the same because in d3d11, sampler parameters are defined
// at the API level.
#define SAMPLER(TEXTURE_IDX, NAME) $SamplerState NAME : $register($s##TEXTURE_IDX);
#define SAMPLER_LINEAR SAMPLER
#define SAMPLER_MIPMAP SAMPLER

#define TEXEL_FETCH(NAME, COORD) NAME[COORD]
#define TEXTURE_SAMPLE(NAME, SAMPLER_NAME, COORD) NAME.$Sample(SAMPLER_NAME, COORD)
#define TEXTURE_SAMPLE_LOD(NAME, SAMPLER_NAME, COORD, LOD)                                         \
    NAME.$SampleLevel(SAMPLER_NAME, COORD, LOD)
#define TEXTURE_SAMPLE_GRAD(NAME, SAMPLER_NAME, COORD, DDX, DDY)                                   \
    NAME.$SampleGrad(SAMPLER_NAME, COORD, DDX, DDY)

#define PLS_INTERLOCK_BEGIN
#define PLS_INTERLOCK_END

#ifdef @ENABLE_RASTERIZER_ORDERED_VIEWS
#define PLS_TEX2D $RasterizerOrderedTexture2D
#else
#define PLS_TEX2D $RWTexture2D
#endif

#define PLS_BLOCK_BEGIN
#ifdef @ENABLE_TYPED_UAV_LOAD_STORE
#define PLS_DECL4F(IDX, NAME) uniform PLS_TEX2D<$unorm half4> NAME : $register($u##IDX)
#else
#define PLS_DECL4F(IDX, NAME) uniform PLS_TEX2D<uint> NAME : $register($u##IDX)
#endif
#define PLS_DECLUI(IDX, NAME) uniform PLS_TEX2D<uint> NAME : $register($u##IDX)
#define PLS_BLOCK_END

#ifdef @ENABLE_TYPED_UAV_LOAD_STORE
#define PLS_LOAD4F(P, _plsCoord) P[_plsCoord]
#else
#define PLS_LOAD4F(P, _plsCoord) unpackUnorm4x8(P[_plsCoord])
#endif
#define PLS_LOADUI(P, _plsCoord) P[_plsCoord]
#ifdef @ENABLE_TYPED_UAV_LOAD_STORE
#define PLS_STORE4F(P, V, _plsCoord) P[_plsCoord] = (V)
#else
#define PLS_STORE4F(P, V, _plsCoord) P[_plsCoord] = packUnorm4x8(V)
#endif
#define PLS_STOREUI(P, V, _plsCoord) P[_plsCoord] = (V)

INLINE uint pls_atomic_max(PLS_TEX2D<uint> plane, int2 _plsCoord, uint x)
{
    uint originalValue;
    $InterlockedMax(plane[_plsCoord], x, originalValue);
    return originalValue;
}

#define PLS_ATOMIC_MAX(PLANE, X, _plsCoord) pls_atomic_max(PLANE, _plsCoord, X)

INLINE uint pls_atomic_add(PLS_TEX2D<uint> plane, int2 _plsCoord, uint x)
{
    uint originalValue;
    $InterlockedAdd(plane[_plsCoord], x, originalValue);
    return originalValue;
}

#define PLS_ATOMIC_ADD(PLANE, X, _plsCoord) pls_atomic_add(PLANE, _plsCoord, X)

#define PLS_PRESERVE_VALUE(P, _plsCoord)

#define VERTEX_MAIN(NAME, Attrs, attrs, _vertexID, _instanceID)                                    \
    $cbuffer DrawUniforms : UNIFORM_BUFFER_REGISTER(DRAW_UNIFORM_BUFFER_IDX)                       \
    {                                                                                              \
        uint baseInstance;                                                                         \
        uint NAME##_pad0;                                                                          \
        uint NAME##_pad1;                                                                          \
        uint NAME##_pad2;                                                                          \
    }                                                                                              \
    Varyings NAME(Attrs attrs, uint _vertexID                                                      \
                  : $SV_VertexID, uint _instanceIDWithoutBase                                      \
                  : $SV_InstanceID)                                                                \
    {                                                                                              \
        uint _instanceID = _instanceIDWithoutBase + baseInstance;                                  \
        Varyings _varyings;

#define IMAGE_MESH_VERTEX_MAIN(NAME,                                                               \
                               MeshUniforms,                                                       \
                               meshUniforms,                                                       \
                               PositionAttr,                                                       \
                               position,                                                           \
                               UVAttr,                                                             \
                               uv,                                                                 \
                               _vertexID)                                                          \
    Varyings NAME(PositionAttr position, UVAttr uv, uint _vertexID : $SV_VertexID)                 \
    {                                                                                              \
        Varyings _varyings;                                                                        \
        float4 _pos;

#define EMIT_VERTEX(POSITION)                                                                      \
    _varyings._pos = POSITION;                                                                     \
    }                                                                                              \
    return _varyings;

#define FRAG_DATA_MAIN(DATA_TYPE, NAME)                                                            \
    DATA_TYPE NAME(Varyings _varyings) : $SV_Target                                                \
    {

#define EMIT_FRAG_DATA(VALUE)                                                                      \
    return VALUE;                                                                                  \
    }

#define PLS_MAIN(NAME, _fragCoord, _plsCoord) [$earlydepthstencil] void NAME(Varyings _varyings) { \
        float2 _fragCoord = _varyings._pos.xy;\
        int2 _plsCoord = int2(floor(_fragCoord));

#define IMAGE_DRAW_PLS_MAIN(NAME, MeshUniforms, meshUniforms, _fragCoord, _plsCoord)               \
    PLS_MAIN(NAME, _fragCoord, _plsCoord)

#define EMIT_PLS }

#define PLS_MAIN_WITH_FRAG_COLOR(NAME, _fragCoord, _plsCoord)                                      \
    [$earlydepthstencil] half4 NAME(Varyings _varyings) : $SV_Target                               \
    {                                                                                              \
        float2 _fragCoord = _varyings._pos.xy;                                                     \
        int2 _plsCoord = int2(floor(_fragCoord));                                                  \
        half4 _fragColor;

#define IMAGE_DRAW_PLS_MAIN_WITH_FRAG_COLOR(NAME, MeshUniforms, meshUniforms, _pos, _plsCoord)     \
    PLS_MAIN_WITH_FRAG_COLOR(NAME, _fragCoord, _plsCoord)

#define EMIT_PLS_WITH_FRAG_COLOR                                                                   \
    }                                                                                              \
    return _fragColor;

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

#define STORAGE_BUFFER_BLOCK_BEGIN
#define STORAGE_BUFFER_BLOCK_END

#define STORAGE_BUFFER_U32x2(IDX, GLSL_STRUCT_NAME, NAME)                                          \
    $StructuredBuffer<uint2> NAME : $register($t##IDX)
#define STORAGE_BUFFER_U32x4(IDX, GLSL_STRUCT_NAME, NAME)                                          \
    $StructuredBuffer<uint4> NAME : $register($t##IDX)
#define STORAGE_BUFFER_F32x4(IDX, GLSL_STRUCT_NAME, NAME)                                          \
    $StructuredBuffer<float4> NAME : $register($t##IDX)

#define STORAGE_BUFFER_LOAD4(NAME, I) NAME[I]
#define STORAGE_BUFFER_LOAD2(NAME, I) NAME[I]

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

INLINE half4 unpackUnorm4x8(uint u)
{
    uint4 vals = uint4(u & 0xffu, (u >> 8) & 0xffu, (u >> 16) & 0xffu, u >> 24);
    return float4(vals) * (1. / 255.);
}

INLINE uint packUnorm4x8(half4 color)
{
    uint4 vals = (uint4(color * 255.) & 0xff) << uint4(0, 8, 16, 24);
    vals.rg |= vals.ba;
    vals.r |= vals.g;
    return vals.r;
}

INLINE float atan(float y, float x) { return $atan2(y, x); }

INLINE float2x2 inverse(float2x2 m)
{
    float2x2 adjoint = float2x2(m[1][1], -m[0][1], -m[1][0], m[0][0]);
    return adjoint * (1. / determinant(m));
}
