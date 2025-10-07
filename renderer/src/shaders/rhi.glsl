/*
 * Copyright 2023 Rive
 */

// This header provides GLSL-specific #defines and declarations that enable our
// shaders to be compiled on MSL and GLSL both.

// HLSL warns that it will unroll the loops through r,g,b values in
// advanced_blend.glsl, but unrolling these loops is exactly what we want.
#pragma $warning($disable : 3550)

// Don't warn about uninitialized variables. If we leave one uninitialized it's
// because we know what we're doing and don't want to pay the cost of
// initializing it.
#pragma $warning($disable : 4000)

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
#define half3x3 $half3x3
#define half2x3 $half2x3
#define half4x4 $half4x4
#endif

$typedef float3 packed_float3;

#ifdef @ENABLE_MIN_16_PRECISION

#if NEEDS_USHORT_DEFINE

$typedef $min16uint ushort;

#endif // NEEDS_USHORT_DEFINE

#else

#if NEEDS_USHORT_DEFINE

$typedef $uint ushort;

#endif // NEEDS_USHORT_DEFINE

#endif // ENABLE_MIN_16_PRECISION

#define SPLAT(A, B) A##B

#define INLINE $inline
#define OUT(ARG_TYPE) out ARG_TYPE
#define INOUT(ARG_TYPE) inout ARG_TYPE

#define ATTR_BLOCK_BEGIN(NAME)                                                 \
    struct NAME                                                                \
    {
#define ATTR(IDX, TYPE, NAME) TYPE NAME : SPLAT(ATTRIBUTE, IDX)
#define ATTR_BLOCK_END                                                         \
    }                                                                          \
    ;
#define ATTR_LOAD(T, A, N, I)
#define ATTR_UNPACK(ID, attrs, NAME, TYPE) TYPE NAME = attrs.NAME

#define UNIFORM_BLOCK_BEGIN(IDX, NAME)                                         \
    $cbuffer NAME                                                              \
    {                                                                          \
        struct                                                                 \
        {

#define UNIFORM_BLOCK_END(NAME)                                                \
    }                                                                          \
    NAME;                                                                      \
    }

#define VARYING_BLOCK_BEGIN                                                    \
    struct Varyings                                                            \
    {

#define NO_PERSPECTIVE $noperspective
#define @OPTIONALLY_FLAT $nointerpolation
#define FLAT $nointerpolation
#define VARYING(IDX, TYPE, NAME) TYPE NAME : SPLAT($TEXCOORD, IDX)

#define VARYING_BLOCK_END                                                      \
    float4 _pos : $SV_Position;                                                \
    }                                                                          \
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

#define DYNAMIC_SAMPLER_BLOCK_BEGIN
#define DYNAMIC_SAMPLER_BLOCK_END

#define TEXTURE_RGBA32UI(SET, IDX, NAME) uniform $Texture2D<uint4> NAME
#define TEXTURE_RGBA32F(SET, IDX, NAME) uniform $Texture2D<float4> NAME
#define TEXTURE_RGBA8(SET, IDX, NAME) uniform $Texture2D<UNORM half4> NAME
#define TEXTURE_R16F(SET, IDX, NAME) uniform $Texture2D<half> NAME
#define TEXTURE_R16F_1D_ARRAY(SET, IDX, NAME) uniform $Texture2DArray<half> NAME
#define SAMPLED_R16F_REF(NAME, SAMPLER_NAME)                                   \
    $Texture2D<half> NAME, $SamplerState SAMPLER_NAME
#define SAMPLED_R16F(NAME, SAMPLER_NAME) NAME, SAMPLER_NAME

// SAMPLER_LINEAR and SAMPLER_MIPMAP are the same because in d3d11, sampler
// parameters are defined at the API level.
#define SAMPLER(IDX, NAME) $SamplerState NAME;
#define SAMPLER_LINEAR SAMPLER
#define SAMPLER_MIPMAP SAMPLER
#define SAMPLER_DYNAMIC(SET, IDX, NAME) SAMPLER(IDX, NAME)

#define TEXEL_FETCH(NAME, COORD) NAME[COORD]
#define TEXTURE_SAMPLE(NAME, SAMPLER_NAME, COORD)                              \
    NAME.$Sample(SAMPLER_NAME, COORD)
#define TEXTURE_SAMPLE_LOD(NAME, SAMPLER_NAME, COORD, LOD)                     \
    NAME.$SampleLevel(SAMPLER_NAME, COORD, LOD)
#define TEXTURE_SAMPLE_LODBIAS(NAME, SAMPLER_NAME, COORD, LODBIAS)             \
    NAME.$SampleBias(SAMPLER_NAME, COORD, LODBIAS)
#define TEXTURE_REF_SAMPLE_LOD TEXTURE_SAMPLE_LOD
#define TEXTURE_SAMPLE_GRAD(NAME, SAMPLER_NAME, COORD, DDX, DDY)               \
    NAME.$SampleGrad(SAMPLER_NAME, COORD, DDX, DDY)
#define TEXTURE_GATHER(NAME, SAMPLER_NAME, COORD, TEXTURE_INVERSE_SIZE)        \
    NAME.$Gather(SAMPLER_NAME, (COORD) * (TEXTURE_INVERSE_SIZE))
#define TEXTURE_SAMPLE_LOD_1D_ARRAY(NAME,                                      \
                                    SAMPLER_NAME,                              \
                                    X,                                         \
                                    ARRAY_INDEX,                               \
                                    ARRAY_INDEX_NORMALIZED,                    \
                                    LOD)                                       \
    NAME.$SampleLevel(SAMPLER_NAME, float3(X, 0.5, ARRAY_INDEX), LOD)

#define TEXTURE_SAMPLE_DYNAMIC(TEXTURE, SAMPLER_NAME, COORD)                   \
    TEXTURE_SAMPLE(TEXTURE, SAMPLER_NAME, COORD)
#define TEXTURE_SAMPLE_DYNAMIC_LOD(TEXTURE, SAMPLER_NAME, COORD, LOD)          \
    TEXTURE_SAMPLE_LOD(TEXTURE, SAMPLER_NAME, COORD, LOD)
#define TEXTURE_SAMPLE_DYNAMIC_LODBIAS(TEXTURE, SAMPLER_NAME, COORD, LODBIAS)  \
    TEXTURE_SAMPLE_LODBIAS(TEXTURE, SAMPLER_NAME, COORD, LODBIAS)

#define PLS_INTERLOCK_BEGIN
#define PLS_INTERLOCK_END

#ifdef @ENABLE_RASTERIZER_ORDERED_VIEWS
#define PLS_TEX2D $RasterizerOrderedTexture2D
#else
#define PLS_TEX2D $RWTexture2D
#endif

#define PLS_BLOCK_BEGIN
#ifdef @ENABLE_TYPED_UAV_LOAD_STORE
#define PLS_DECL4F(IDX, NAME) uniform PLS_TEX2D<UNORM half4> NAME
#else
#define PLS_DECL4F(IDX, NAME) uniform PLS_TEX2D<uint> NAME
#endif
#define PLS_DECL4F_READONLY PLS_DECL4F
#define PLS_DECLUI(IDX, NAME) uniform PLS_TEX2D<uint> NAME
#define PLS_DECLUI_ATOMIC PLS_DECLUI
#define PLS_LOADUI_ATOMIC PLS_LOADUI
#define PLS_STOREUI_ATOMIC PLS_STOREUI
#define PLS_BLOCK_END

#ifdef @ENABLE_TYPED_UAV_LOAD_STORE
#define PLS_LOAD4F(PLANE) PLANE[_plsCoord]
#else
#define PLS_LOAD4F(PLANE) unpackUnorm4x8(PLANE[_plsCoord])
#endif
#define PLS_LOADUI(PLANE) PLANE[_plsCoord]
#ifdef @ENABLE_TYPED_UAV_LOAD_STORE
#define PLS_STORE4F(PLANE, VALUE) PLANE[_plsCoord] = (VALUE)
#else
#define PLS_STORE4F(PLANE, VALUE) PLANE[_plsCoord] = packUnorm4x8(VALUE)
#endif
#define PLS_STOREUI(PLANE, VALUE) PLANE[_plsCoord] = (VALUE)

INLINE uint pls_atomic_max(PLS_TEX2D<uint> plane, int2 _plsCoord, uint x)
{
    uint originalValue;
    $InterlockedMax(plane[_plsCoord], x, originalValue);
    return originalValue;
}

#define PLS_ATOMIC_MAX(PLANE, X) pls_atomic_max(PLANE, _plsCoord, X)

INLINE uint pls_atomic_add(PLS_TEX2D<uint> plane, int2 _plsCoord, uint x)
{
    uint originalValue;
    $InterlockedAdd(plane[_plsCoord], x, originalValue);
    return originalValue;
}

#define PLS_ATOMIC_ADD(PLANE, X) pls_atomic_add(PLANE, _plsCoord, X)

#define PLS_PRESERVE_4F(PLANE)
#define PLS_PRESERVE_UI(PLANE)

#define VERTEX_CONTEXT_DECL
#define VERTEX_CONTEXT_UNPACK

#define TEXTURE_CONTEXT_DECL
#define TEXTURE_CONTEXT_FORWARD

#define VERTEX_MAIN(NAME, Attrs, attrs, _vertexID, _instanceID)                \
                                                                               \
    uint $baseInstance;                                                        \
                                                                               \
    Varyings NAME(Attrs attrs,                                                 \
                  uint _vertexID : $SV_VertexID,                               \
                  uint _instanceIDWithoutBase : $SV_InstanceID)                \
    {                                                                          \
        uint _instanceID = _instanceIDWithoutBase + $baseInstance;             \
        Varyings _varyings;

#define IMAGE_RECT_VERTEX_MAIN(NAME, Attrs, attrs, _vertexID, _instanceID)     \
    Varyings NAME(Attrs attrs, uint _vertexID : $SV_VertexID)                  \
    {                                                                          \
        Varyings _varyings;                                                    \
        float4 _pos;

#define IMAGE_MESH_VERTEX_MAIN(NAME,                                           \
                               PositionAttr,                                   \
                               position,                                       \
                               UVAttr,                                         \
                               uv,                                             \
                               _vertexID)                                      \
    Varyings NAME(PositionAttr position,                                       \
                  UVAttr uv,                                                   \
                  uint _vertexID : $SV_VertexID)                               \
    {                                                                          \
        Varyings _varyings;                                                    \
        float4 _pos;

#define EMIT_VERTEX(POSITION)                                                  \
    _varyings._pos = POSITION;                                                 \
    }                                                                          \
    return _varyings;

#define FRAG_DATA_MAIN(DATA_TYPE, NAME)                                        \
    DATA_TYPE NAME(Varyings _varyings) : $SV_Target                            \
    {

#define EMIT_FRAG_DATA(VALUE)                                                  \
    return VALUE;                                                              \
    }

#define FRAGMENT_CONTEXT_DECL , float2 _fragCoord
#define FRAGMENT_CONTEXT_UNPACK , _fragCoord

#define PLS_CONTEXT_DECL , int2 _plsCoord
#define PLS_CONTEXT_UNPACK , _plsCoord

#define PLS_MAIN(NAME)                                                         \
    EARLYDEPTHSTENCIL void NAME(Varyings _varyings)                            \
    {                                                                          \
        float2 _fragCoord = _varyings._pos.xy;                                 \
        int2 _plsCoord = int2(floor(_fragCoord));

#define PLS_MAIN_WITH_IMAGE_UNIFORMS(NAME) PLS_MAIN(NAME)

#define EMIT_PLS }

#define PLS_FRAG_COLOR_MAIN(NAME)                                              \
    EARLYDEPTHSTENCIL half4 NAME(Varyings _varyings) : $SV_Target              \
    {                                                                          \
        float2 _fragCoord = _varyings._pos.xy;                                 \
        int2 _plsCoord = int2(floor(_fragCoord));                              \
        half4 _fragColor;

#define PLS_FRAG_COLOR_MAIN_WITH_IMAGE_UNIFORMS(NAME) PLS_FRAG_COLOR_MAIN(NAME)

#define EMIT_PLS_AND_FRAG_COLOR                                                \
    }                                                                          \
    return _fragColor;

#define uintBitsToFloat $asfloat
#define intBitsToFloat $asfloat
#define floatBitsToInt $asint
#define floatBitsToUint $asuint
#define inversesqrt $rsqrt
#define equal(A, B) ((A) == (B))
#define notEqual(A, B) ((A) != (B))
#define lessThanEqual(A, B) ((A) <= (B))
#define lessThan(A, B) ((A) < (B))
#define greaterThan(A, B) ((A) > (B))
#define greaterThanEqual(A, B) ((A) >= (B))

// HLSL matrices are stored in row-major order, and therefore transposed from
// their counterparts in GLSL and Metal. We can work around this entirely by
// reversing the arguments to mul().
#define MUL(A, B) $mul(B, A)

#define VERTEX_STORAGE_BUFFER_BLOCK_BEGIN
#define VERTEX_STORAGE_BUFFER_BLOCK_END

#define FRAG_STORAGE_BUFFER_BLOCK_BEGIN
#define FRAG_STORAGE_BUFFER_BLOCK_END

#define STORAGE_BUFFER_U32x2(IDX, GLSL_STRUCT_NAME, NAME)                      \
    $StructuredBuffer<uint2> NAME
#define STORAGE_BUFFER_U32x4(IDX, GLSL_STRUCT_NAME, NAME)                      \
    $StructuredBuffer<uint4> NAME
#define STORAGE_BUFFER_F32x4(IDX, GLSL_STRUCT_NAME, NAME)                      \
    $StructuredBuffer<float4> NAME

#define STORAGE_BUFFER_LOAD4(NAME, I) NAME[I]
#define STORAGE_BUFFER_LOAD2(NAME, I) NAME[I]

INLINE half2 unpackHalf2x16(uint u)
{
    uint y = (u >> 16);
    uint x = u & 0xffffu;
    return half2($f16tof32(x), $f16tof32(y));
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
    return half4(vals) * (1. / 255.);
}

INLINE uint packUnorm4x8(half4 color)
{
    uint4 vals = (uint4(color * 255.) & 0xff) << uint4(0, 8, 16, 24);
    vals.rg |= vals.ba;
    vals.r |= vals.g;
    return vals.r;
}

INLINE float2x2 inverse(float2x2 m)
{
    float2x2 adjoint = float2x2(m[1][1], -m[0][1], -m[1][0], m[0][0]);
    return adjoint * (1. / determinant(m));
}

// Redirects for intrinsics that have different names in HLSL

INLINE float mix(float x, float y, float s) { return $lerp(x, y, s); }
INLINE float2 mix(float2 x, float2 y, float2 s) { return $lerp(x, y, s); }
INLINE float3 mix(float3 x, float3 y, float3 s) { return $lerp(x, y, s); }
INLINE float4 mix(float4 x, float4 y, float4 s) { return $lerp(x, y, s); }

INLINE float fract(float x) { return $frac(x); }
INLINE float2 fract(float2 x) { return $frac(x); }
INLINE float3 fract(float3 x) { return $frac(x); }
INLINE float4 fract(float4 x) { return $frac(x); }

INLINE float mod(float x, float y) { return $fmod(x, y); }

// Reimplement intrinsics for half types.
// This shadows the intrinsic function for floats, so we also have to declare
// that overload.

INLINE float rive_sign(float x) { return sign(x); }
INLINE float2 rive_sign(float2 x) { return sign(x); }
INLINE float3 rive_sign(float3 x) { return sign(x); }
INLINE float4 rive_sign(float4 x) { return sign(x); }

#define sign rive_sign

INLINE float rive_abs(float x) { return abs(x); }
INLINE float2 rive_abs(float2 x) { return abs(x); }
INLINE float3 rive_abs(float3 x) { return abs(x); }
INLINE float4 rive_abs(float4 x) { return abs(x); }

#define abs rive_abs

INLINE float rive_sqrt(float x) { return sqrt(x); }
INLINE float2 rive_sqrt(float2 x) { return sqrt(x); }
INLINE float3 rive_sqrt(float3 x) { return sqrt(x); }
INLINE float4 rive_sqrt(float4 x) { return sqrt(x); }

#define sqrt rive_sqrt
