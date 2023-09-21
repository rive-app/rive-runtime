/*
 * Copyright 2023 Rive
 */

// This header provides GLSL-specific #defines and declarations that enable our shaders to be
// compiled on MSL and GLSL both.

#define GLSL

#define float2 vec2
#define float3 vec3
#define packed_float3 vec3
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
#define make_half3x4 mat3x4

#define INLINE

#if defined(GL_ANGLE_base_vertex_base_instance_shader_builtin)
#extension GL_ANGLE_base_vertex_base_instance_shader_builtin : require
#endif

#ifdef @TARGET_VULKAN
#define UNIFORM_BLOCK_BEGIN(IDX, NAME)                                                             \
    layout(binding = IDX, std140) uniform NAME                                                     \
    {
#else
#define UNIFORM_BLOCK_BEGIN(IDX, NAME)                                                             \
    layout(std140) uniform NAME                                                                    \
    {
#endif
// clang-format barrier... Otherwise it tries to merge this #define into the above macro...
#define UNIFORM_BLOCK_END(NAME)                                                                    \
    }                                                                                              \
    NAME;

#define ATTR_BLOCK_BEGIN(NAME)
#define ATTR(IDX, TYPE, NAME) layout(location = IDX) in TYPE NAME
#define ATTR_BLOCK_END
#define ATTR_LOAD(A, B, C, D)
#define ATTR_UNPACK(ID, attrs, NAME, TYPE)

#ifdef @VERTEX
#ifdef @TARGET_VULKAN
#define VARYING(IDX, TYPE, NAME) layout(location = IDX) out TYPE NAME
#else
#define VARYING(IDX, TYPE, NAME) out TYPE NAME
#endif
#else
#ifdef @TARGET_VULKAN
#define VARYING(IDX, TYPE, NAME) layout(location = IDX) in TYPE NAME
#else
#define VARYING(IDX, TYPE, NAME) in TYPE NAME
#endif
#endif
#define FLAT flat
#define VARYING_BLOCK_BEGIN(NAME)
#define VARYING_BLOCK_END(_pos)

#if defined(GL_NV_shader_noperspective_interpolation)
#extension GL_NV_shader_noperspective_interpolation : require
#define NO_PERSPECTIVE noperspective
#else
#define NO_PERSPECTIVE
#endif

#ifdef @VERTEX
#define VERTEX_TEXTURE_BLOCK_BEGIN(NAME)
#define VERTEX_TEXTURE_BLOCK_END
#endif

#ifdef @FRAGMENT
#define FRAG_TEXTURE_BLOCK_BEGIN(NAME)
#define FRAG_TEXTURE_BLOCK_END
#endif

#ifdef @TARGET_VULKAN
#define TEXTURE_RGBA32UI(IDX, NAME) layout(binding = IDX) uniform highp utexture2D NAME
#define TEXTURE_RGBA32F(IDX, NAME) layout(binding = IDX) uniform highp texture2D NAME
#define TEXTURE_RGBA8(IDX, NAME) layout(binding = IDX) uniform mediump texture2D NAME

#define SAMPLER_LINEAR(TEXTURE_IDX, NAME)                                                          \
    layout(binding = TEXTURE_IDX, set = SAMPLER_BINDINGS_SET) uniform mediump sampler NAME;
#define SAMPLER_MIPMAP(TEXTURE_IDX, NAME)                                                          \
    layout(binding = TEXTURE_IDX, set = SAMPLER_BINDINGS_SET) uniform mediump sampler NAME;

#define TEXTURE_SAMPLE_GRAD(TEXTURE_BLOCK, NAME, SAMPLER_NAME, COORD, DDX, DDY)                    \
    textureGrad(sampler2D(NAME, SAMPLER_NAME), COORD, DDX, DDY)
#else
#define TEXTURE_RGBA32UI(IDX, NAME) uniform highp usampler2D NAME
#define TEXTURE_RGBA32F(IDX, NAME) uniform highp sampler2D NAME
#define TEXTURE_RGBA8(IDX, NAME) uniform mediump sampler2D NAME

// SAMPLER_LINEAR and SAMPLER_MIPMAP are no-ops because in GL, sampling parameters are API-level
// state tied to the texture.
#define SAMPLER_LINEAR(TEXTURE_IDX, NAME)
#define SAMPLER_MIPMAP(TEXTURE_IDX, NAME)

#define TEXTURE_SAMPLE(TEXTURE_BLOCK, NAME, SAMPLER_NAME, COORD) texture(NAME, COORD)
#define TEXTURE_SAMPLE_GRAD(TEXTURE_BLOCK, NAME, SAMPLER_NAME, COORD, DDX, DDY)                    \
    textureGrad(NAME, COORD, DDX, DDY)
#endif

#define TEXEL_FETCH(TEXTURE_BLOCK, NAME, COORD) texelFetch(NAME, COORD, 0)

// Define macros for implementing pixel local storage based on available extensions.
#ifdef @PLS_IMPL_WEBGL

#extension GL_ANGLE_shader_pixel_local_storage : require

#define PLS_BLOCK_BEGIN
#define PLS_DECL4F(IDX, NAME) layout(binding = IDX, rgba8) uniform lowp pixelLocalANGLE NAME
#define PLS_DECLUI(IDX, NAME) layout(binding = IDX, r32ui) uniform highp upixelLocalANGLE NAME
#define PLS_BLOCK_END

#define PLS_LOAD4F(P) pixelLocalLoadANGLE(P)
#define PLS_LOADUI(P) pixelLocalLoadANGLE(P).r
#define PLS_STORE4F(P, V) pixelLocalStoreANGLE(P, V)
#define PLS_STOREUI(P, V) pixelLocalStoreANGLE(P, uvec4(V))

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
#define PLS_DECL4F(IDX, NAME) layout(rgba8) lowp vec4 NAME
#define PLS_DECLUI(IDX, NAME) layout(r32ui) highp uint NAME
#define PLS_BLOCK_END                                                                              \
    }                                                                                              \
    ;

#define PLS_LOAD4F(P) P
#define PLS_LOADUI(P) P
#define PLS_STORE4F(P, V) P = (V)
#define PLS_STOREUI(P, V) P = (V)

#define PLS_PRESERVE_VALUE(P)

#define PLS_INTERLOCK_BEGIN
#define PLS_INTERLOCK_END

#endif

#ifdef @PLS_IMPL_FRAMEBUFFER_FETCH

#extension GL_EXT_shader_framebuffer_fetch : require

#define PLS_BLOCK_BEGIN
#define PLS_DECL4F(IDX, NAME) layout(location = IDX) inout lowp vec4 NAME
#define PLS_DECLUI(IDX, NAME) layout(location = IDX) inout highp uvec4 NAME
#define PLS_BLOCK_END

#define PLS_LOAD4F(P) P
#define PLS_LOADUI(P) P.r
#define PLS_STORE4F(P, V) P = (V)
#define PLS_STOREUI(P, V) P.r = (V)

// When using multiple color attachments, we have to write a value to every color attachment, every
// shader invocation, or else the contents become undefined.
#define PLS_PRESERVE_VALUE(P) P = P

#define PLS_INTERLOCK_BEGIN
#define PLS_INTERLOCK_END

#endif

#ifdef @PLS_IMPL_RW_TEXTURE

#if defined(GL_ARB_fragment_shader_interlock)
#extension GL_ARB_fragment_shader_interlock : require
#define PLS_INTERLOCK_BEGIN beginInvocationInterlockARB()
#define PLS_INTERLOCK_END endInvocationInterlockARB()
#elif defined(GL_INTEL_fragment_shader_ordering)
#extension GL_INTEL_fragment_shader_ordering : require
#define PLS_INTERLOCK_BEGIN beginFragmentShaderOrderingINTEL()
#define PLS_INTERLOCK_END
#endif

#define PLS_BLOCK_BEGIN
#define PLS_DECL4F(IDX, NAME) layout(binding = IDX, rgba8) uniform lowp coherent image2D NAME
#define PLS_DECLUI(IDX, NAME) layout(binding = IDX, r32ui) uniform highp coherent uimage2D NAME
#define PLS_BLOCK_END

#define PLS_LOAD4F(P) imageLoad(P, plsCoord)
#define PLS_LOADUI(P) imageLoad(P, plsCoord).r
#define PLS_STORE4F(P, V) imageStore(P, plsCoord, V)
#define PLS_STOREUI(P, V) imageStore(P, plsCoord, uvec4(V))

#define PLS_PRESERVE_VALUE(P)

#endif

#ifdef @TARGET_VULKAN

#define PLS_BLOCK_BEGIN
#define PLS_DECL4F(IDX, NAME) layout(location = IDX) out lowp vec4 NAME
// FIXME: WebGPU doesn't support subpassLoad. Use PLS once it's implemented.
// layout(input_attachment_index=IDX, binding=IDX, set=2) uniform lowp subpassInput i##NAME
#define PLS_DECLUI(IDX, NAME) layout(location = IDX) out highp uvec4 NAME
// FIXME: WebGPU doesn't support subpassLoad. Use PLS once it's implemented.
// layout(input_attachment_index=IDX, binding=IDX, set=2) uniform lowp usubpassInput i##NAME
#define PLS_BLOCK_END

// FIXME: WebGPU doesn't support subpassLoad. Use PLS once it's implemented.
// #define PLS_LOAD4F(P) subpassLoad(i##P)
// #define PLS_LOADUI(P) subpassLoad(i##P).r
#define PLS_LOAD4F(P) vec4(0)
#define PLS_LOADUI(P) 0u
#define PLS_STORE4F(P, V) P = (V)
#define PLS_STOREUI(P, V) P.r = (V)

#define PLS_PRESERVE_VALUE(P)
#define PLS_INTERLOCK_BEGIN
#define PLS_INTERLOCK_END

#endif

#ifdef @ENABLE_BASE_INSTANCE_POLYFILL
#define BASE_INSTANCE_POLYFILL_DECL(IDX, NAME) uniform int NAME
// The Qualcomm compiler doesn't like how clang-format handles these lines.
// clang-format off
#define VERTEX_MAIN(NAME, Uniforms, uniforms, Attrs, attrs, Varyings, varyings, VertexTextures, textures, _vertexID, _instanceID, _pos) \
    void main()                                                                                    \
    {                                                                                              \
        int _vertexID = gl_VertexID;                                                               \
        int _instanceID = gl_InstanceID;                                                           \
        vec4 _pos;
// clang-format on
#else
#ifdef GL_ANGLE_base_vertex_base_instance_shader_builtin
#extension GL_ANGLE_base_vertex_base_instance_shader_builtin : require
#endif
// The Qualcomm compiler doesn't like how clang-format handles these lines.
// clang-format off
#define VERTEX_MAIN(NAME, Uniforms, uniforms, Attrs, attrs, Varyings, varyings, VertexTextures, textures, _vertexID, _instanceID, _pos) \
    void main()                                                                                    \
    {                                                                                              \
        int _vertexID = gl_VertexID;                                                               \
        int _instanceID = gl_BaseInstance + gl_InstanceID;                                         \
        vec4 _pos;
// clang-format on
#endif

// clang-format off
#define IMAGE_MESH_VERTEX_MAIN(NAME, Uniforms, uniforms, MeshUniforms, meshUniforms, PositionAttr, position, UVAttr, uv, Varyings, varyings, _vertexID, _pos) \
    VERTEX_MAIN(NAME, Uniforms, uniforms, PositionAttr, position, Varyings, varyings, _, _, _vertexID, _instanceID, _pos)
// clang-format on

#define VARYING_INIT(varyings, NAME, TYPE)
#define VARYING_PACK(varyings, NAME)
#define VARYING_UNPACK(varyings, NAME, TYPE)

#define EMIT_VERTEX(varyings, _pos)                                                                \
    }                                                                                              \
    gl_Position = _pos;

#define FRAG_DATA_MAIN(DATA_TYPE, NAME, Varyings, varyings)                                        \
    layout(location = 0) out DATA_TYPE _fd;                                                        \
    void main()

#define EMIT_FRAG_DATA(VALUE) _fd = VALUE

#ifdef @PLS_IMPL_RW_TEXTURE
#define PLS_MAIN(NAME, Varyings, varyings, FragmentTextures, textures, _pos)                       \
    void main()                                                                                    \
    {                                                                                              \
        highp ivec2 plsCoord = ivec2(floor(gl_FragCoord.xy));
#else
#define PLS_MAIN(NAME, Varyings, varyings, FragmentTextures, textures, _pos)                       \
    void main()                                                                                    \
    {
#endif

// clang-format off
#define IMAGE_MESH_PLS_MAIN(NAME, MeshUniforms, meshUniforms, Varyings, varyings, FragmentTextures, textures, _pos) \
    PLS_MAIN(NAME, Varyings, varyings, FragmentTextures, textures, _pos)
// clang-format on

#define EMIT_PLS }

#define MUL(A, B) ((A) * (B))

precision highp float;
precision highp int;
