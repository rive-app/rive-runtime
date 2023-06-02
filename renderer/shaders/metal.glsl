/*
 * Copyright 2023 Rive
 */

// This header provides Metal-specific #defines and declarations that enable our shaders to be
// compiled on MSL and GLSL both.

#define METAL

// Native metal types need a #define. They get rewritten because they are not GLSL keywords.
#define half half
#define half2 half2
#define half3 half3
#define half4 half4
#define short short
#define short2 short2
#define short3 short3
#define short4 short4
#define ushort ushort
#define ushort2 ushort2
#define ushort3 ushort3
#define ushort4 ushort4
#define float2 float2
#define float3 float3
#define float4 float4
#define bool2 bool2
#define bool3 bool3
#define bool4 bool4
#define uint2 uint2
#define uint3 uint3
#define uint4 uint4
#define int2 int2
#define int3 int3
#define int4 int4
#define float4x2 float4x2
#define ushort ushort
#define float2x2 float2x2
#define half3x4 half3x4

#define make_half4 half4
#define make_half3 half3
#define make_half2 half2
#define make_half half

#define make_ushort ushort

#define INLINE inline

#define notEqual(A, B) ((A) != (B))
#define lessThanEqual(A, B) ((A) <= (B))
#define greaterThanEqual(A, B) ((A) >= (B))
#define atan atan2
#define inversesqrt rsqrt

#define UNIFORM_BLOCK_BEGIN(N)                                                                     \
    struct N                                                                                       \
    {
#define UNIFORM_BLOCK_END(N)                                                                       \
    }                                                                                              \
    ;

#define ATTR_BLOCK_BEGIN(N)                                                                        \
    struct N                                                                                       \
    {
#define ATTR(LOCATION)
#define ATTR_BLOCK_END                                                                             \
    }                                                                                              \
    ;
#define ATTR_LOAD(T, A, N, I) T N = A[I].N;

#define VARYING_BLOCK_BEGIN(N)                                                                     \
    struct N                                                                                       \
    {
#define VARYING
#define FLAT [[flat]]
#define NO_PERSPECTIVE [[center_no_perspective]]
// No-persective interpolation appears to break the guarantee that a varying == "x" when all
// barycentric values also == "x". Using default (perspective-correct) interpolation is also faster
// than flat on M1.
#define @OPTIONALLY_FLAT
#define VARYING_BLOCK_END                                                                          \
    float4 _pos [[position]];                                                                      \
    }                                                                                              \
    ;

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

#define TEXTURE_RGBA32UI(B) [[texture(B)]] texture2d<uint>
#define TEXTURE_RGBA32F(B) [[texture(B)]] texture2d<float>
#define TEXTURE_RGBA8(B) [[texture(B)]] texture2d<half>

#define TEXEL_FETCH(T, N, C, L) T.N.read(uint2(C))
#define TEXTURE_SAMPLE(T, N, C) T.N.sample(sampler(mag_filter::linear, min_filter::linear), C)

#define PLS_BLOCK_BEGIN                                                                            \
    struct PLS                                                                                     \
    {
#define PLS_DECL4F(B) [[color(B)]] half4
#define PLS_DECL2F(B) [[color(B)]] half2
#define PLS_BLOCK_END                                                                              \
    }                                                                                              \
    ;

#define PLS_LOAD4F(P) _inpls.P
#define PLS_LOAD2F(P) _inpls.P
#define PLS_STORE4F(P, V) _pls.P = (V)
#define PLS_STORE2F(P, X, Y) _pls.P = half2(X, Y)
#define PLS_PRESERVE_VALUE(P) _pls.P = _inpls.P

// A hack since we use GLSL_POSITION inside the following #defines...
#define GLSL_POSITION GLSL_POSITION

#define VERTEX_MAIN(F, V, v, ...)                                                                  \
    __attribute__((visibility("default"))) V vertex F(__VA_ARGS__)                                 \
    {                                                                                              \
        V v;                                                                                       \
        float4 GLSL_POSITION;

#define EMIT_VERTEX(v)                                                                             \
    v._pos = GLSL_POSITION;                                                                        \
    return v;                                                                                      \
    }

#define EMIT_OFFSCREEN_VERTEX(v)                                                                   \
    v._pos = GLSL_POSITION;                                                                        \
    v._pos.y = -v._pos.y;                                                                          \
    return v;                                                                                      \
    }

#define FRAG_DATA_TYPE(T) T
#define FRAG_DATA_MAIN(F, ...)                                                                     \
    __attribute__((visibility("default"))) fragment F(__VA_ARGS__)                                 \
    {
#define EMIT_FRAG_DATA(f)                                                                          \
    return f;                                                                                      \
    }

#define PLS_MAIN(F, ...)                                                                           \
    __attribute__((visibility("default"))) PLS fragment F(PLS _inpls, __VA_ARGS__)                 \
    {                                                                                              \
        PLS _pls;

#define EMIT_PLS                                                                                   \
    return _pls;                                                                                   \
    }

// "FLD" for "field".
// In Metal, inputs and outputs are accessed from structs.
// In GLSL 300 es, they have to be global.
#define FLD(BLOCK, MEMBER) BLOCK.MEMBER

using namespace metal;

template <int N> inline vec<uint, N> floatBitsToUint(vec<float, N> x)
{
    return as_type<vec<uint, N>>(x);
}

inline uint floatBitsToUint(float x) { return as_type<uint>(x); }

template <int N> inline vec<float, N> uintBitsToFloat(vec<uint, N> x)
{
    return as_type<vec<float, N>>(x);
}

inline float uintBitsToFloat(uint x) { return as_type<float>(x); }
inline half2 unpackHalf2x16(uint x) { return as_type<half2>(x); }
inline uint packHalf2x16(half2 x) { return as_type<uint>(x); }

inline float2x2 inverse(float2x2 m)
{
    float2x2 m_ = float2x2(m[1][1], -m[0][1], -m[1][0], m[0][0]);
    float det = (m_[0][0] * m[0][0]) + (m_[0][1] * m[1][0]);
    return m_ * (1 / det);
}

inline float2x2 make_float2x2(float4 f) { return float2x2(f.xy, f.zw); }
inline float4x2 make_float4x2(float4 a, float4 b) { return float4x2(a.xy, a.zw, b.xy, b.zw); }

inline half3 mix(half3 a, half3 b, bool3 c)
{
    half3 result;
    for (int i = 0; i < 3; ++i)
        result[i] = c[i] ? b[i] : a[i];
    return result;
}

inline half3x4 make_half3x4(half3 a, half b, half3 c, half d, half3 e, half f)
{
    return half3x4(a.x, a.y, a.z, b, c.x, c.y, c.z, d, e.x, e.y, e.z, f);
}
