/*
 * Copyright 2023 Rive
 */

// This header provides Metal-specific #defines and declarations that enable our shaders to be
// compiled on MSL and GLSL both.

#define METAL

// #define native metal types if their names are being rewritten.
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
#define packed_float3 $packed_float3
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

#define make_half4 half4
#define make_half3 half3
#define make_half2 half2
#define make_half half

#define make_ushort ushort

#define INLINE $inline

#define notEqual(A, B) ((A) != (B))
#define lessThanEqual(A, B) ((A) <= (B))
#define greaterThanEqual(A, B) ((A) >= (B))
#define MUL(A, B) ((A) * (B))
#define atan $atan2
#define inversesqrt $rsqrt

#define UNIFORM_BLOCK_BEGIN(IDX, NAME)                                                             \
    struct NAME                                                                                    \
    {
#define UNIFORM_BLOCK_END(NAME)                                                                    \
    }                                                                                              \
    ;

#define ATTR_BLOCK_BEGIN(N)                                                                        \
    struct N                                                                                       \
    {
#define ATTR(IDX, TYPE, NAME) TYPE NAME
#define ATTR_BLOCK_END                                                                             \
    }                                                                                              \
    ;
#define ATTR_UNPACK(ID, attrs, NAME, TYPE) TYPE NAME = attrs[ID].NAME

#define VARYING_BLOCK_BEGIN(N)                                                                     \
    struct N                                                                                       \
    {
#define VARYING(IDX, TYPE, NAME) TYPE NAME
#define FLAT [[flat]]
#define NO_PERSPECTIVE [[$center_no_perspective]]
// No-persective interpolation appears to break the guarantee that a varying == "x" when all
// barycentric values also == "x". Using default (perspective-correct) interpolation is also faster
// than flat on M1.
#define @OPTIONALLY_FLAT
#define VARYING_BLOCK_END(_pos)                                                                    \
    float4 _pos [[$position]] [[$invariant]];                                                      \
    }                                                                                              \
    ;

#define VARYING_INIT(varyings, NAME, TYPE) $thread TYPE& NAME = varyings.NAME
#define VARYING_PACK(varyings, NAME)
#define VARYING_UNPACK(varyings, NAME, TYPE) TYPE NAME = varyings.NAME

#define VERTEX_TEXTURE_BLOCK_BEGIN(N)                                                              \
    struct N                                                                                       \
    {
#define VERTEX_TEXTURE_BLOCK_END                                                                   \
    }                                                                                              \
    ;

#define FRAG_TEXTURE_BLOCK_BEGIN(N)                                                                \
    struct N                                                                                       \
    {
#define FRAG_TEXTURE_BLOCK_END                                                                     \
    }                                                                                              \
    ;

#define TEXTURE_RGBA32UI(IDX, NAME) [[$texture(IDX)]] $texture2d<uint> NAME
#define TEXTURE_RGBA32F(IDX, NAME) [[$texture(IDX)]] $texture2d<float> NAME
#define TEXTURE_RGBA8(IDX, NAME) [[$texture(IDX)]] $texture2d<half> NAME

#define SAMPLER_LINEAR(TEXTURE_IDX, NAME)                                                          \
    $constexpr $sampler NAME($filter::$linear, $mip_filter::$none);
#define SAMPLER_MIPMAP(TEXTURE_IDX, NAME)                                                          \
    $constexpr $sampler NAME($filter::$linear, $mip_filter::$linear);

#define TEXEL_FETCH(TEXTURE_BLOCK, NAME, COORD) TEXTURE_BLOCK.NAME.$read(uint2(COORD))
#define TEXTURE_SAMPLE(TEXTURE_BLOCK, NAME, SAMPLER_NAME, COORD)                                   \
    TEXTURE_BLOCK.NAME.$sample(SAMPLER_NAME, COORD)

#define PLS_BLOCK_BEGIN                                                                            \
    struct PLS                                                                                     \
    {
#define PLS_DECL4F(IDX, NAME) [[$color(IDX)]] half4 NAME
#define PLS_DECLUI(IDX, NAME) [[$color(IDX)]] uint NAME
#define PLS_BLOCK_END                                                                              \
    }                                                                                              \
    ;

#define PLS_LOAD4F(P) _inpls.P
#define PLS_LOADUI(P) _inpls.P
#define PLS_STORE4F(P, V) _pls.P = (V)
#define PLS_STOREUI(P, V) _pls.P = (V)
#define PLS_PRESERVE_VALUE(P) _pls.P = _inpls.P
#define PLS_INTERLOCK_BEGIN
#define PLS_INTERLOCK_END

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
    $__attribute__(($visibility("default"))) Varyings $vertex NAME(                                \
        uint _vertexID [[$vertex_id]],                                                             \
        uint _instanceID [[$instance_id]],                                                         \
        $constant Uniforms& uniforms [[$buffer(FLUSH_UNIFORM_BUFFER_IDX)]],                        \
        $constant Attrs* attrs [[$buffer(1)]],                                                     \
        VertexTextures textures)                                                                   \
    {                                                                                              \
        Varyings varyings;                                                                         \
        float4 _pos;

#define IMAGE_MESH_VERTEX_MAIN(NAME,                                                               \
                               Uniforms,                                                           \
                               uniforms,                                                           \
                               MeshUniforms,                                                       \
                               meshUniforms,                                                       \
                               PositionAttr,                                                       \
                               position,                                                           \
                               UVAttr,                                                             \
                               uv,                                                                 \
                               Varyings,                                                           \
                               varyings,                                                           \
                               _vertexID,                                                          \
                               _pos)                                                               \
    $__attribute__(($visibility("default"))) Varyings $vertex NAME(                                \
        uint _vertexID [[$vertex_id]],                                                             \
        $constant Uniforms& uniforms [[$buffer(FLUSH_UNIFORM_BUFFER_IDX)]],                        \
        $constant MeshUniforms& meshUniforms [[$buffer(IMAGE_MESH_UNIFORM_BUFFER_IDX)]],           \
        $constant PositionAttr* position [[$buffer(2)]],                                           \
        $constant UVAttr* uv [[$buffer(3)]])                                                       \
    {                                                                                              \
        Varyings varyings;                                                                         \
        float4 _pos;

#define EMIT_VERTEX(varyings, _pos)                                                                \
    }                                                                                              \
    varyings._pos = _pos;                                                                          \
    return varyings;

#define FRAG_DATA_MAIN(DATA_TYPE, NAME, Varyings, varyings)                                        \
    DATA_TYPE $__attribute__(($visibility("default"))) $fragment NAME(Varyings varyings            \
                                                                      [[$stage_in]])               \
    {

#define EMIT_FRAG_DATA(VALUE)                                                                      \
    return VALUE;                                                                                  \
    }

#define PLS_MAIN(NAME, Varyings, varyings, FragmentTextures, textures, _pos)                       \
    $__attribute__(($visibility("default"))) PLS $fragment NAME(PLS _inpls,                        \
                                                                Varyings varyings [[$stage_in]],   \
                                                                FragmentTextures textures)         \
    {                                                                                              \
        PLS _pls;

#define IMAGE_MESH_PLS_MAIN(NAME,                                                                  \
                            MeshUniforms,                                                          \
                            meshUniforms,                                                          \
                            Varyings,                                                              \
                            varyings,                                                              \
                            FragmentTextures,                                                      \
                            textures,                                                              \
                            _pos)                                                                  \
    $__attribute__(($visibility("default"))) PLS $fragment NAME(                                   \
        PLS _inpls,                                                                                \
        $constant MeshUniforms& meshUniforms [[$buffer(IMAGE_MESH_UNIFORM_BUFFER_IDX)]],           \
        Varyings varyings [[$stage_in]],                                                           \
        FragmentTextures textures)                                                                 \
    {                                                                                              \
        PLS _pls;

#define EMIT_PLS                                                                                   \
    }                                                                                              \
    return _pls;

$using $namespace $metal;

$template<int N> INLINE $vec<uint, N> floatBitsToUint($vec<float, N> x)
{
    return $as_type<$vec<uint, N>>(x);
}

$template<int N> INLINE $vec<int, N> floatBitsToInt($vec<float, N> x)
{
    return $as_type<$vec<int, N>>(x);
}

INLINE uint floatBitsToUint(float x) { return $as_type<uint>(x); }

INLINE int floatBitsToInt(float x) { return $as_type<int>(x); }

$template<int N> INLINE $vec<float, N> uintBitsToFloat($vec<uint, N> x)
{
    return $as_type<$vec<float, N>>(x);
}

INLINE float uintBitsToFloat(uint x) { return $as_type<float>(x); }
INLINE half2 unpackHalf2x16(uint x) { return $as_type<half2>(x); }
INLINE uint packHalf2x16(half2 x) { return $as_type<uint>(x); }

INLINE float2x2 inverse(float2x2 m)
{
    float2x2 m_ = float2x2(m[1][1], -m[0][1], -m[1][0], m[0][0]);
    float det = (m_[0][0] * m[0][0]) + (m_[0][1] * m[1][0]);
    return m_ * (1 / det);
}

INLINE half3 mix(half3 a, half3 b, bool3 c)
{
    half3 result;
    for (int i = 0; i < 3; ++i)
        result[i] = c[i] ? b[i] : a[i];
    return result;
}

INLINE half3x4 make_half3x4(half3 a, half b, half3 c, half d, half3 e, half f)
{
    return half3x4(a.x, a.y, a.z, b, c.x, c.y, c.z, d, e.x, e.y, e.z, f);
}
