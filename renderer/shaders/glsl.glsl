/*
 * Copyright 2023 Rive
 */

// This header provides GLSL-specific #defines and declarations that enable our shaders to be
// compiled on MSL and GLSL both.

#define float2 vec2
#define float3 vec3
#define float4 vec4

#define half mediump float
#define half2 mediump vec2
#define half3 mediump vec3
#define half4 mediump vec4
#define make_half float
#define make_half2 vec2
#define make_half3 vec3
#define make_half4 vec4

#define int2 ivec2
#define int3 ivec3
#define int4 ivec4

#define short mediump int
#define short2 mediump ivec2
#define short3 mediump ivec3
#define short4 mediump ivec4
#define make_short ivec
#define make_short2 ivec2
#define make_short3 ivec3
#define make_short4 ivec4

#define uint2 uvec2
#define uint3 uvec3
#define uint4 uvec4

#define ushort mediump uint
#define ushort2 mediump uvec2
#define ushort3 mediump uvec3
#define ushort4 mediump uvec4
#define make_ushort uint
#define make_ushort2 uvec2
#define make_ushort3 uvec3
#define make_ushort4 uvec4

#define float2x2 mat2
#define float4x2 mat4x2
#define make_float2x2 mat2
#define make_float4x2 mat4x2
#define make_half3x4 mat3x4

#define INLINE
#define GLSL_FRONT_FACING gl_FrontFacing
#define GLSL_POSITION gl_Position
#define GLSL_VERTEX_ID gl_VertexID
#define GLSL_INSTANCE_ID gl_InstanceID

#define UNIFORM_BLOCK_BEGIN(N)                                                                     \
    layout(std140) uniform N                                                                       \
    {
// clang-format barrier... Otherwise it tries to merge this #define into the above macro...
#define UNIFORM_BLOCK_END(N)                                                                       \
    }                                                                                              \
    N;

#define ATTR_BLOCK_BEGIN(N)
#define ATTR(L) layout(location = L) in
#define ATTR_BLOCK_END
#define ATTR_LOAD(A, B, C, D)

#define VARYING_BLOCK_BEGIN(N)
#ifdef @VERTEX
#define VARYING out
#else
#define VARYING in
#endif
#define FLAT flat
#define VARYING_BLOCK_END

#ifdef GL_NV_shader_noperspective_interpolation
#extension GL_NV_shader_noperspective_interpolation : require
#define NO_PERSPECTIVE noperspective
#else
#define NO_PERSPECTIVE
#endif

#ifdef @VERTEX
#define VERTEX_TEXTURE_BLOCK_BEGIN(N)
#define VERTEX_TEXTURE_BLOCK_END
#endif

#ifdef @FRAGMENT
#define FRAG_TEXTURE_BLOCK_BEGIN(N)
#define FRAG_TEXTURE_BLOCK_END
#endif

#define TEXTURE_RGBA32UI(B) uniform highp usampler2D
#define TEXTURE_RGBA32F(B) uniform highp sampler2D
#define TEXTURE_RGBA8(B) uniform mediump sampler2D

#define TEXEL_FETCH(T, N, C, L) texelFetch(N, C, L)
#define TEXTURE_SAMPLE(T, N, C) texture(N, C)

// Define macros for implementing pixel local storage based on available extensions.
#ifdef @PLS_IMPL_WEBGL

#extension GL_ANGLE_shader_pixel_local_storage : require

#define PLS_BLOCK_BEGIN
#define PLS_DECL4F(B) layout(binding = B, rgba8) uniform lowp pixelLocalANGLE
#define PLS_DECL2F(B) layout(binding = B, r32ui) uniform highp upixelLocalANGLE
#define PLS_BLOCK_END

#define PLS_LOAD4F(P) pixelLocalLoadANGLE(P)
#define PLS_LOAD2F(P) unpackHalf2x16(pixelLocalLoadANGLE(P).x)
#define PLS_STORE4F(P, V) pixelLocalStoreANGLE(P, V)
#define PLS_STORE2F(P, X, Y) pixelLocalStoreANGLE(P, uvec4(packHalf2x16(vec2(X, Y))))

#define PLS_PRESERVE_VALUE(P)

#define PLS_INTERLOCK_BEGIN
#define PLS_INTERLOCK_END

#endif

#ifdef @PLS_IMPL_EXT_NATIVE

#extension GL_EXT_shader_pixel_local_storage : require

// We need one of the framebuffer fetch extensions for the shader that loads the framebuffer.
#extension GL_ARM_shader_framebuffer_fetch : enable
#extension GL_EXT_shader_framebuffer_fetch : enable

#define PLS_BLOCK_BEGIN                                                                            \
    __pixel_localEXT PLS                                                                           \
    {
#define PLS_DECL4F(B) layout(rgba8) lowp vec4
#define PLS_DECL2F(B) layout(rg16f) mediump vec2
#define PLS_BLOCK_END                                                                              \
    }                                                                                              \
    ;

#define PLS_LOAD4F(P) P
#define PLS_LOAD2F(P) P
#define PLS_STORE4F(P, V) P = (V)
#define PLS_STORE2F(P, X, Y) P = vec2(X, Y)

#define PLS_PRESERVE_VALUE(P)

#define PLS_INTERLOCK_BEGIN
#define PLS_INTERLOCK_END

#endif

#ifdef @PLS_IMPL_FRAMEBUFFER_FETCH

#extension GL_EXT_shader_framebuffer_fetch : require

#define PLS_BLOCK_BEGIN
#define PLS_DECL4F(B) layout(location = B) inout lowp vec4
#define PLS_DECL2F(B) layout(location = B) inout highp uvec4
#define PLS_BLOCK_END

#define PLS_LOAD4F(P) P
#define PLS_LOAD2F(P) unpackHalf2x16(P.x)
#define PLS_STORE4F(P, V) P = (V)
#define PLS_STORE2F(P, X, Y) P.x = packHalf2x16(vec2(X, Y))

// When using multiple color attachments, we have to write a value to every color attachment, every
// shader invocation, or else the contents become undefined.
#define PLS_PRESERVE_VALUE(P) P = P

#define PLS_INTERLOCK_BEGIN
#define PLS_INTERLOCK_END

#endif

#ifdef @PLS_IMPL_RW_TEXTURE

#extension GL_ARB_fragment_shader_interlock : enable
#extension GL_INTEL_fragment_shader_ordering : enable

#if defined(GL_ARB_fragment_shader_interlock)
#define PLS_INTERLOCK_BEGIN                                                                        \
    highp ivec2 plsCoord = ivec2(floor(gl_FragCoord.xy));                                          \
    beginInvocationInterlockARB()
#define PLS_INTERLOCK_END endInvocationInterlockARB()
#elif defined(GL_INTEL_fragment_shader_ordering)
#define PLS_INTERLOCK_BEGIN                                                                        \
    highp ivec2 plsCoord = ivec2(floor(gl_FragCoord.xy));                                          \
    beginFragmentShaderOrderingINTEL()
#define PLS_INTERLOCK_END
#endif

#define PLS_BLOCK_BEGIN
#define PLS_DECL4F(B) layout(binding = B, rgba8) uniform lowp coherent image2D
#define PLS_DECL2F(B) layout(binding = B, r32ui) uniform highp coherent uimage2D
#define PLS_BLOCK_END

#define PLS_LOAD4F(P) imageLoad(P, plsCoord)
#define PLS_LOAD2F(P) unpackHalf2x16(imageLoad(P, plsCoord).x)
#define PLS_STORE4F(P, V) imageStore(P, plsCoord, V)
#define PLS_STORE2F(P, X, Y) imageStore(P, plsCoord, uvec4(packHalf2x16(vec2(X, Y))))

#define PLS_PRESERVE_VALUE(P)

#endif

#define VERTEX_MAIN void main
#define EMIT_VERTEX(v)
#define EMIT_OFFSCREEN_VERTEX(v)

#define FRAG_DATA_TYPE(T) out T _fd;
#define FRAG_DATA_MAIN void main
#define EMIT_FRAG_DATA(f) _fd = f

#define PLS_MAIN void main
#define EMIT_PLS

// "FLD" for "field".
// In Metal, inputs and outputs are accessed from structs.
// In GLSL 300 es, they have to be global.
#define FLD(BLOCK, MEMBER) MEMBER

precision highp float;
precision highp int;
