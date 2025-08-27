/*
 * Copyright 2023 Rive
 */

// This header provides GLSL-specific #defines and declarations that enable our
// shaders to be compiled on MSL and GLSL both.

#define GLSL

#ifndef @GLSL_VERSION
// In "#version 320 es", Qualcomm incorrectly substitutes __VERSION__ to 300.
// @GLSL_VERSION is a workaround for this.
#define @GLSL_VERSION __VERSION__
#endif

#define float2 vec2
#define float3 vec3
#define packed_float3 vec3
#define float4 vec4

#define half mediump float
#define half2 mediump vec2
#define half3 mediump vec3
#define half4 mediump vec4
#define half3x3 mediump mat3x3
#define half2x3 mediump mat2x3

#define int2 ivec2
#define int3 ivec3
#define int4 ivec4

#define short mediump int
#define short2 mediump ivec2
#define short3 mediump ivec3
#define short4 mediump ivec4

#define uint2 uvec2
#define uint3 uvec3
#define uint4 uvec4

#define ushort mediump uint
#define ushort2 mediump uvec2
#define ushort3 mediump uvec3
#define ushort4 mediump uvec4

#define bool2 bvec2
#define bool3 bvec3
#define bool4 bvec4

#define float2x2 mat2

#define INLINE
#define OUT(ARG_TYPE) out ARG_TYPE
#define INOUT(ARG_TYPE) inout ARG_TYPE

#ifdef GL_ANGLE_base_vertex_base_instance_shader_builtin
#extension GL_ANGLE_base_vertex_base_instance_shader_builtin : require
#endif

#ifdef @ENABLE_KHR_BLEND
#extension GL_KHR_blend_equation_advanced : require
#endif

// clang-format off
#if defined(@RENDER_MODE_MSAA) && defined(@ENABLE_CLIP_RECT) && defined(GL_ES)
// clang-format on
#ifdef GL_EXT_clip_cull_distance
#extension GL_EXT_clip_cull_distance : require
#elif defined(GL_ANGLE_clip_cull_distance)
#extension GL_ANGLE_clip_cull_distance : require
#endif
#endif // RENDER_MODE_MSAA && ENABLE_CLIP_RECT

#if @GLSL_VERSION >= 310
#define UNIFORM_BLOCK_BEGIN(IDX, NAME)                                         \
    layout(binding = IDX, std140) uniform NAME                                 \
    {
#else
#define UNIFORM_BLOCK_BEGIN(IDX, NAME)                                         \
    layout(std140) uniform NAME                                                \
    {
#endif
// clang-format barrier... Otherwise it tries to merge this #define into the
// above macro...
#define UNIFORM_BLOCK_END(NAME)                                                \
    }                                                                          \
    NAME;

#define ATTR_BLOCK_BEGIN(NAME)
#define ATTR(IDX, TYPE, NAME) layout(location = IDX) in TYPE NAME
#define ATTR_BLOCK_END
#define ATTR_LOAD(A, B, C, D)
#define ATTR_UNPACK(ID, attrs, NAME, TYPE)

#ifdef @VERTEX
#if @GLSL_VERSION >= 310
#define VARYING(IDX, TYPE, NAME) layout(location = IDX) out TYPE NAME
#else
#define VARYING(IDX, TYPE, NAME) out TYPE NAME
#endif
#else
#if @GLSL_VERSION >= 310
#define VARYING(IDX, TYPE, NAME) layout(location = IDX) in TYPE NAME
#else
#define VARYING(IDX, TYPE, NAME) in TYPE NAME
#endif
#endif
#define FLAT flat
#define VARYING_BLOCK_BEGIN
#define VARYING_BLOCK_END

// clang-format off
#ifdef @TARGET_VULKAN
   // Since Vulkan is compiled offline and not all platforms support noperspective, don't use it.
#  define NO_PERSPECTIVE
#else
#  ifdef GL_NV_shader_noperspective_interpolation
#    extension GL_NV_shader_noperspective_interpolation : require
#    define NO_PERSPECTIVE noperspective
#  else
#    define NO_PERSPECTIVE
#  endif
#endif
// clang-format on

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

#ifdef @TARGET_VULKAN
#define TEXTURE_RGBA32UI(SET, IDX, NAME)                                       \
    layout(set = SET, binding = IDX) uniform highp utexture2D NAME
#define TEXTURE_RGBA32F(SET, IDX, NAME)                                        \
    layout(set = SET, binding = IDX) uniform highp texture2D NAME
#define TEXTURE_RGBA8(SET, IDX, NAME)                                          \
    layout(set = SET, binding = IDX) uniform mediump texture2D NAME
#define TEXTURE_R16F(SET, IDX, NAME)                                           \
    layout(binding = IDX) uniform mediump texture2D NAME
#if defined(@FRAGMENT) && defined(@RENDER_MODE_MSAA)
#define DST_COLOR_TEXTURE(NAME)                                                \
    layout(input_attachment_index = 0,                                         \
           binding = COLOR_PLANE_IDX,                                          \
           set = PLS_TEXTURE_BINDINGS_SET) uniform lowp subpassInputMS NAME
#endif // @FRAGMENT && @RENDER_MODE_MSAA
#elif @GLSL_VERSION >= 310
#define TEXTURE_RGBA32UI(SET, IDX, NAME)                                       \
    layout(binding = IDX) uniform highp usampler2D NAME
#define TEXTURE_RGBA32F(SET, IDX, NAME)                                        \
    layout(binding = IDX) uniform highp sampler2D NAME
#define TEXTURE_RGBA8(SET, IDX, NAME)                                          \
    layout(binding = IDX) uniform mediump sampler2D NAME
#define TEXTURE_R16F(SET, IDX, NAME)                                           \
    layout(binding = IDX) uniform mediump sampler2D NAME
#define DST_COLOR_TEXTURE(NAME)                                                \
    TEXTURE_RGBA8(PER_FLUSH_BINDINGS_SET, DST_COLOR_TEXTURE_IDX, NAME)
#else
#define TEXTURE_RGBA32UI(SET, IDX, NAME) uniform highp usampler2D NAME
#define TEXTURE_RGBA32F(SET, IDX, NAME) uniform highp sampler2D NAME
#define TEXTURE_RGBA8(SET, IDX, NAME) uniform mediump sampler2D NAME
#define TEXTURE_R16F(SET, IDX, NAME) uniform mediump sampler2D NAME
#define DST_COLOR_TEXTURE(NAME)                                                \
    TEXTURE_RGBA8(PER_FLUSH_BINDINGS_SET, DST_COLOR_TEXTURE_IDX, NAME)
#endif

#ifdef @TARGET_VULKAN
#define SAMPLER_LINEAR(TEXTURE_IDX, NAME)                                      \
    layout(set = IMMUTABLE_SAMPLER_BINDINGS_SET, binding = TEXTURE_IDX)        \
        uniform mediump sampler NAME;
#define SAMPLER_MIPMAP(TEXTURE_IDX, NAME)                                      \
    layout(set = IMMUTABLE_SAMPLER_BINDINGS_SET, binding = TEXTURE_IDX)        \
        uniform mediump sampler NAME;
#define SAMPLER_DYNAMIC(SET, IDX, NAME)                                        \
    layout(set = SET, binding = IDX) uniform mediump sampler NAME;
#define TEXTURE_SAMPLE(NAME, SAMPLER_NAME, COORD)                              \
    texture(sampler2D(NAME, SAMPLER_NAME), COORD)
#define TEXTURE_SAMPLE_LOD(NAME, SAMPLER_NAME, COORD, LOD)                     \
    textureLod(sampler2D(NAME, SAMPLER_NAME), COORD, LOD)
#define TEXTURE_SAMPLE_GRAD(NAME, SAMPLER_NAME, COORD, DDX, DDY)               \
    textureGrad(sampler2D(NAME, SAMPLER_NAME), COORD, DDX, DDY)
#if defined(@FRAGMENT) && defined(@RENDER_MODE_MSAA)
#extension GL_OES_sample_variables : require
#define DST_COLOR_FETCH(NAME)                                                  \
    dst_color_fetch(mat4(subpassLoad(NAME, 0),                                 \
                         subpassLoad(NAME, 1),                                 \
                         subpassLoad(NAME, 2),                                 \
                         subpassLoad(NAME, 3)))
#endif // @FRAGMENT && @RENDER_MODE_MSAA
#else  // @TARGET_VULKAN -> !@TARGET_VULKAN
// SAMPLER_LINEAR and SAMPLER_MIPMAP are no-ops because in GL, sampling
// parameters are API-level state tied to the texture.
#define SAMPLER_LINEAR(TEXTURE_IDX, NAME)
#define SAMPLER_MIPMAP(TEXTURE_IDX, NAME)
#define SAMPLER_DYNAMIC(SET, IDX, NAME)
#define TEXTURE_SAMPLE(NAME, SAMPLER_NAME, COORD) texture(NAME, COORD)
#define TEXTURE_SAMPLE_LOD(NAME, SAMPLER_NAME, COORD, LOD)                     \
    textureLod(NAME, COORD, LOD)
#define TEXTURE_SAMPLE_GRAD(NAME, SAMPLER_NAME, COORD, DDX, DDY)               \
    textureGrad(NAME, COORD, DDX, DDY)
#define DST_COLOR_FETCH(NAME) texelFetch(NAME, ivec2(floor(_fragCoord.xy)), 0)
#endif // !@TARGET_VULKAN

#define TEXTURE_SAMPLE_DYNAMIC(TEXTURE, SAMPLER_NAME, COORD)                   \
    TEXTURE_SAMPLE(TEXTURE, SAMPLER_NAME, COORD)
#define TEXTURE_SAMPLE_DYNAMIC_LOD(TEXTURE, SAMPLER_NAME, COORD, LOD)          \
    TEXTURE_SAMPLE_LOD(TEXTURE, SAMPLER_NAME, COORD, LOD)

// Polyfill the feather texture as a sampler2D since ES doesn't support
// sampler1DArray. This is why the macro needs "ARRAY_INDEX_NORMALIZED": when
// polyfilled as a 2D texture, the "array index" needs to be a 0..1 normalized
// y coordinate instead of the literal array index.
#define TEXTURE_R16F_1D_ARRAY(SET, IDX, NAME) TEXTURE_R16F(SET, IDX, NAME)
// clang-format off
// Clang formatting on this line trips up the Qualcomm compiler.
#define TEXTURE_SAMPLE_LOD_1D_ARRAY(NAME, SAMPLER_NAME, X, ARRAY_INDEX, ARRAY_INDEX_NORMALIZED, LOD)                                       \
    TEXTURE_SAMPLE_LOD(NAME, SAMPLER_NAME, float2(X, ARRAY_INDEX_NORMALIZED), LOD)
// clang-format on

#define TEXTURE_RG32UI(SET, IDX, NAME) TEXTURE_RGBA32UI(SET, IDX, NAME)

#define TEXTURE_CONTEXT_DECL

#define TEXTURE_CONTEXT_FORWARD
#define TEXEL_FETCH(NAME, COORD) texelFetch(NAME, COORD, 0)

#ifdef @TARGET_VULKAN
#define TEXTURE_GATHER(NAME, SAMPLER_NAME, COORD, TEXTURE_INVERSE_SIZE)        \
    textureGather(sampler2D(NAME, SAMPLER_NAME),                               \
                  (COORD) * (TEXTURE_INVERSE_SIZE))
#elif @GLSL_VERSION >= 310
#define TEXTURE_GATHER(NAME, SAMPLER_NAME, COORD, TEXTURE_INVERSE_SIZE)        \
    textureGather(NAME, (COORD) * (TEXTURE_INVERSE_SIZE))
#else
#define TEXTURE_GATHER(NAME, SAMPLER_NAME, COORD, TEXTURE_INVERSE_SIZE)        \
    make_half4(TEXEL_FETCH(NAME, int2(COORD) + int2(-1, 0)).r,                 \
               TEXEL_FETCH(NAME, int2(COORD) + int2(0, 0)).r,                  \
               TEXEL_FETCH(NAME, int2(COORD) + int2(0, -1)).r,                 \
               TEXEL_FETCH(NAME, int2(COORD) + int2(-1, -1)).r)
#endif

#define VERTEX_STORAGE_BUFFER_BLOCK_BEGIN
#define VERTEX_STORAGE_BUFFER_BLOCK_END

#define FRAG_STORAGE_BUFFER_BLOCK_BEGIN
#define FRAG_STORAGE_BUFFER_BLOCK_END

#ifdef @DISABLE_SHADER_STORAGE_BUFFERS

#define STORAGE_BUFFER_U32x2(IDX, GLSL_STRUCT_NAME, NAME)                      \
    TEXTURE_RGBA32UI(PER_FLUSH_BINDINGS_SET, IDX, NAME)
#define STORAGE_BUFFER_U32x4(IDX, GLSL_STRUCT_NAME, NAME)                      \
    TEXTURE_RG32UI(PER_FLUSH_BINDINGS_SET, IDX, NAME)
#define STORAGE_BUFFER_F32x4(IDX, GLSL_STRUCT_NAME, NAME)                      \
    TEXTURE_RGBA32F(PER_FLUSH_BINDINGS_SET, IDX, NAME)
#define STORAGE_BUFFER_LOAD4(NAME, I)                                          \
    TEXEL_FETCH(                                                               \
        NAME,                                                                  \
        int2((I) & STORAGE_TEXTURE_MASK_X, (I) >> STORAGE_TEXTURE_SHIFT_Y))
#define STORAGE_BUFFER_LOAD2(NAME, I)                                          \
    TEXEL_FETCH(                                                               \
        NAME,                                                                  \
        int2((I) & STORAGE_TEXTURE_MASK_X, (I) >> STORAGE_TEXTURE_SHIFT_Y))    \
        .xy

#else

#ifdef GL_ARB_shader_storage_buffer_object
#extension GL_ARB_shader_storage_buffer_object : require
#endif
#define STORAGE_BUFFER_U32x2(IDX, GLSL_STRUCT_NAME, NAME)                      \
    layout(std430, binding = IDX) readonly buffer GLSL_STRUCT_NAME             \
    {                                                                          \
        uint2 _values[];                                                       \
    }                                                                          \
    NAME
#define STORAGE_BUFFER_U32x4(IDX, GLSL_STRUCT_NAME, NAME)                      \
    layout(std430, binding = IDX) readonly buffer GLSL_STRUCT_NAME             \
    {                                                                          \
        uint4 _values[];                                                       \
    }                                                                          \
    NAME
#define STORAGE_BUFFER_F32x4(IDX, GLSL_STRUCT_NAME, NAME)                      \
    layout(std430, binding = IDX) readonly buffer GLSL_STRUCT_NAME             \
    {                                                                          \
        float4 _values[];                                                      \
    }                                                                          \
    NAME
#define STORAGE_BUFFER_U32_ATOMIC(IDX, GLSL_STRUCT_NAME, NAME)                 \
    layout(std430, binding = IDX) buffer GLSL_STRUCT_NAME { uint _values[]; }  \
    NAME
#define STORAGE_BUFFER_LOAD4(NAME, I) NAME._values[I]
#define STORAGE_BUFFER_LOAD2(NAME, I) NAME._values[I]
#define STORAGE_BUFFER_LOAD(NAME, I) NAME._values[I]
#define STORAGE_BUFFER_ATOMIC_MAX(NAME, I, X) atomicMax(NAME._values[I], X)
#define STORAGE_BUFFER_ATOMIC_ADD(NAME, I, X) atomicAdd(NAME._values[I], X)

#endif // DISABLE_SHADER_STORAGE_BUFFERS

// Define macros for implementing pixel local storage based on available
// extensions.
#ifdef @PLS_IMPL_ANGLE

#extension GL_ANGLE_shader_pixel_local_storage : require

#define PLS_BLOCK_BEGIN
#define PLS_DECL4F(IDX, NAME)                                                  \
    layout(binding = IDX, rgba8) uniform lowp pixelLocalANGLE NAME
#define PLS_DECLUI(IDX, NAME)                                                  \
    layout(binding = IDX, r32ui) uniform highp upixelLocalANGLE NAME
#define PLS_BLOCK_END

#define PLS_LOAD4F(PLANE) pixelLocalLoadANGLE(PLANE)
#define PLS_LOADUI(PLANE) pixelLocalLoadANGLE(PLANE).r
#define PLS_STORE4F(PLANE, VALUE) pixelLocalStoreANGLE(PLANE, VALUE)
#define PLS_STOREUI(PLANE, VALUE) pixelLocalStoreANGLE(PLANE, uvec4(VALUE))

#define PLS_PRESERVE_4F(PLANE)
#define PLS_PRESERVE_UI(PLANE)

#define PLS_INTERLOCK_BEGIN
#define PLS_INTERLOCK_END

#endif // PLS_IMPL_ANGLE

#ifdef @PLS_IMPL_EXT_NATIVE

#extension GL_EXT_shader_pixel_local_storage : enable

#define PLS_BLOCK_BEGIN                                                        \
    __pixel_localEXT PLS                                                       \
    {
#define PLS_DECL4F(IDX, NAME) layout(rgba8) lowp vec4 NAME
#define PLS_DECLUI(IDX, NAME) layout(r32ui) highp uint NAME
#define PLS_BLOCK_END                                                          \
    }                                                                          \
    ;

#define PLS_LOAD4F(PLANE) PLANE
#define PLS_LOADUI(PLANE) PLANE
#define PLS_STORE4F(PLANE, VALUE) PLANE = (VALUE)
#define PLS_STOREUI(PLANE, VALUE) PLANE = (VALUE)

#define PLS_PRESERVE_4F(PLANE) PLANE = PLANE
#define PLS_PRESERVE_UI(PLANE) PLANE = PLANE

#define PLS_INTERLOCK_BEGIN
#define PLS_INTERLOCK_END

#endif

#ifdef @PLS_IMPL_FRAMEBUFFER_FETCH

#extension GL_EXT_shader_framebuffer_fetch : require

#define PLS_BLOCK_BEGIN
#define PLS_DECL4F(IDX, NAME) layout(location = IDX) inout lowp vec4 NAME
#define PLS_DECLUI(IDX, NAME) layout(location = IDX) inout highp uvec4 NAME
#define PLS_BLOCK_END

#define PLS_LOAD4F(PLANE) PLANE
#define PLS_LOADUI(PLANE) PLANE.r
#define PLS_STORE4F(PLANE, VALUE) PLANE = (VALUE)
#define PLS_STOREUI(PLANE, VALUE) PLANE.r = (VALUE)

// When using multiple color attachments, we have to write a value to every
// color attachment, every shader invocation, or else the contents become
// undefined.
#define PLS_PRESERVE_4F(PLANE) PLS_STORE4F(PLANE, PLS_LOAD4F(PLANE))
#define PLS_PRESERVE_UI(PLANE) PLS_STOREUI(PLANE, PLS_LOADUI(PLANE))

#define PLS_INTERLOCK_BEGIN
#define PLS_INTERLOCK_END

#endif // PLS_IMPL_FRAMEBUFFER_FETCH

#ifdef @PLS_IMPL_STORAGE_TEXTURE

#ifdef GL_ARB_shader_image_load_store
#extension GL_ARB_shader_image_load_store : require
#endif
#if defined(GL_ARB_fragment_shader_interlock)
#extension GL_ARB_fragment_shader_interlock : require
#define PLS_INTERLOCK_BEGIN beginInvocationInterlockARB()
#define PLS_INTERLOCK_END endInvocationInterlockARB()
#elif defined(GL_INTEL_fragment_shader_ordering)
#extension GL_INTEL_fragment_shader_ordering : require
#define PLS_INTERLOCK_BEGIN beginFragmentShaderOrderingINTEL()
#define PLS_INTERLOCK_END
#else
#define PLS_INTERLOCK_BEGIN
#define PLS_INTERLOCK_END
#endif

#define PLS_BLOCK_BEGIN
#ifdef @TARGET_VULKAN
#define PLS_DECL4F(IDX, NAME)                                                  \
    layout(set = PLS_TEXTURE_BINDINGS_SET, binding = IDX, rgba8)               \
        uniform lowp coherent image2D NAME
#define PLS_DECLUI(IDX, NAME)                                                  \
    layout(set = PLS_TEXTURE_BINDINGS_SET, binding = IDX, r32ui)               \
        uniform highp coherent uimage2D NAME
#else
#define PLS_DECL4F(IDX, NAME)                                                  \
    layout(binding = IDX, rgba8) uniform lowp coherent image2D NAME
#define PLS_DECLUI(IDX, NAME)                                                  \
    layout(binding = IDX, r32ui) uniform highp coherent uimage2D NAME
#endif
#define PLS_BLOCK_END

#define PLS_LOAD4F(PLANE) imageLoad(PLANE, _plsCoord)
#define PLS_LOADUI(PLANE) imageLoad(PLANE, _plsCoord).r
#define PLS_STORE4F(PLANE, VALUE) imageStore(PLANE, _plsCoord, VALUE)
#define PLS_STOREUI(PLANE, VALUE) imageStore(PLANE, _plsCoord, uvec4(VALUE))

#define PLS_PRESERVE_4F(PLANE)
#define PLS_PRESERVE_UI(PLANE)

#ifndef @USING_PLS_STORAGE_TEXTURES
#define @USING_PLS_STORAGE_TEXTURES

#endif // PLS_IMPL_STORAGE_TEXTURE

#endif // PLS_IMPL_STORAGE_TEXTURE

#ifdef @PLS_IMPL_SUBPASS_LOAD

#define PLS_BLOCK_BEGIN
#define PLS_DECL4F_READONLY(IDX, NAME)                                         \
    layout(input_attachment_index = IDX,                                       \
           binding = IDX,                                                      \
           set = PLS_TEXTURE_BINDINGS_SET)                                     \
        uniform lowp subpassInput _in_##NAME;
#define PLS_DECL4F(IDX, NAME)                                                  \
    PLS_DECL4F_READONLY(IDX, NAME);                                            \
    layout(location = IDX) out lowp vec4 NAME
#define PLS_DECLUI(IDX, NAME)                                                  \
    layout(input_attachment_index = IDX,                                       \
           binding = IDX,                                                      \
           set = PLS_TEXTURE_BINDINGS_SET)                                     \
        uniform highp usubpassInput _in_##NAME;                                \
    layout(location = IDX) out highp uvec4 NAME
#define PLS_BLOCK_END

#define PLS_LOAD4F(PLANE) subpassLoad(_in_##PLANE)
#define PLS_LOADUI(PLANE) subpassLoad(_in_##PLANE).r
#define PLS_STORE4F(PLANE, VALUE) PLANE = (VALUE)
#define PLS_STOREUI(PLANE, VALUE) PLANE.r = (VALUE)

#define PLS_PRESERVE_4F(PLANE) PLS_STORE4F(PLANE, subpassLoad(_in_##PLANE))
#define PLS_PRESERVE_UI(PLANE) PLS_STOREUI(PLANE, subpassLoad(_in_##PLANE).r)

#define PLS_INTERLOCK_BEGIN
#define PLS_INTERLOCK_END

#endif

#ifdef @PLS_IMPL_NONE

#define PLS_BLOCK_BEGIN
#define PLS_DECL4F(IDX, NAME) layout(location = IDX) out lowp vec4 NAME
#define PLS_DECLUI(IDX, NAME) layout(location = IDX) out highp uvec4 NAME
#define PLS_BLOCK_END

#define PLS_LOAD4F(PLANE) vec4(0)
#define PLS_LOADUI(PLANE) 0u
#define PLS_STORE4F(PLANE, VALUE) PLANE = (VALUE)
#define PLS_STOREUI(PLANE, VALUE) PLANE.r = (VALUE)

#define PLS_PRESERVE_4F(PLANE) PLANE = vec4(0)
#define PLS_PRESERVE_UI(PLANE) PLANE.r = 0u

#define PLS_INTERLOCK_BEGIN
#define PLS_INTERLOCK_END

#endif

#ifndef PLS_DECL4F_READONLY
#define PLS_DECL4F_READONLY PLS_DECL4F
#endif

#ifdef @TARGET_VULKAN
#define gl_VertexID gl_VertexIndex
#endif

// clang-format off
#ifdef @ENABLE_INSTANCE_INDEX
#  ifdef @TARGET_VULKAN
#    define INSTANCE_INDEX gl_InstanceIndex
#  else
#    ifdef @BASE_INSTANCE_UNIFORM_NAME
       // gl_BaseInstance isn't supported on this platform. The rendering
       // backend will set this uniform for us instead.
       uniform highp int @BASE_INSTANCE_UNIFORM_NAME;
#      define INSTANCE_INDEX (gl_InstanceID + @BASE_INSTANCE_UNIFORM_NAME)
#    else
#        define INSTANCE_INDEX (gl_InstanceID + gl_BaseInstance)
#    endif
#  endif
#else
#  define INSTANCE_INDEX 0
#endif
// clang-format on

#define VERTEX_CONTEXT_DECL
#define VERTEX_CONTEXT_UNPACK

#define VERTEX_MAIN(NAME, Attrs, attrs, _vertexID, _instanceID)                \
    void main()                                                                \
    {                                                                          \
        int _vertexID = gl_VertexID;                                           \
        int _instanceID = INSTANCE_INDEX;

#define IMAGE_RECT_VERTEX_MAIN VERTEX_MAIN

// clang-format off
#define IMAGE_MESH_VERTEX_MAIN(NAME, PositionAttr, position, UVAttr, uv, _vertexID) \
    VERTEX_MAIN(NAME, PositionAttr, position, _vertexID, _instanceID)
// clang-format on

#define VARYING_INIT(NAME, TYPE)
#define VARYING_PACK(NAME)
#define VARYING_UNPACK(NAME, TYPE)

#define EMIT_VERTEX(_pos)                                                      \
    gl_Position = _pos;                                                        \
    }

#define FRAG_DATA_MAIN(DATA_TYPE, NAME)                                        \
    layout(location = 0) out DATA_TYPE _fd;                                    \
    void main()

#define EMIT_FRAG_DATA(VALUE) _fd = VALUE

#define _fragCoord gl_FragCoord.xy

#define FRAGMENT_CONTEXT_DECL
#define FRAGMENT_CONTEXT_UNPACK

#ifdef @USING_PLS_STORAGE_TEXTURES

#ifdef @TARGET_VULKAN
#define PLS_DECLUI_ATOMIC(IDX, NAME)                                           \
    layout(set = PLS_TEXTURE_BINDINGS_SET, binding = IDX, r32ui)               \
        uniform highp coherent uimage2D NAME
#else
#define PLS_DECLUI_ATOMIC(IDX, NAME)                                           \
    layout(binding = IDX, r32ui) uniform highp coherent uimage2D NAME
#endif
#define PLS_LOADUI_ATOMIC(PLANE) imageLoad(PLANE, _plsCoord).r
#define PLS_STOREUI_ATOMIC(PLANE, VALUE)                                       \
    imageStore(PLANE, _plsCoord, uvec4(VALUE))
#define PLS_ATOMIC_MAX(PLANE, X) imageAtomicMax(PLANE, _plsCoord, X)
#define PLS_ATOMIC_ADD(PLANE, X) imageAtomicAdd(PLANE, _plsCoord, X)

#define PLS_CONTEXT_DECL , int2 _plsCoord
#define PLS_CONTEXT_UNPACK , _plsCoord

#define PLS_MAIN(NAME)                                                         \
    void main()                                                                \
    {                                                                          \
        int2 _plsCoord = ivec2(floor(_fragCoord));

#define EMIT_PLS }

#else // !USING_PLS_STORAGE_TEXTURES

#define PLS_CONTEXT_DECL
#define PLS_CONTEXT_UNPACK

#define PLS_MAIN(NAME) void main()
#define EMIT_PLS

#endif // !USING_PLS_STORAGE_TEXTURES

#define PLS_MAIN_WITH_IMAGE_UNIFORMS(NAME) PLS_MAIN(NAME)

#define PLS_FRAG_COLOR_MAIN(NAME)                                              \
    layout(location = 0) out half4 _fragColor;                                 \
    PLS_MAIN(NAME)

#define PLS_FRAG_COLOR_MAIN_WITH_IMAGE_UNIFORMS(NAME)                          \
    layout(location = 0) out half4 _fragColor;                                 \
    PLS_MAIN(NAME)

#define EMIT_PLS_AND_FRAG_COLOR EMIT_PLS

#define MUL(A, B) ((A) * (B))

precision highp float;
precision highp int;

#if @GLSL_VERSION < 310
// Polyfill ES 3.1+ methods.
INLINE half4 unpackUnorm4x8(uint u)
{
    uint4 vals = uint4(u & 0xffu, (u >> 8) & 0xffu, (u >> 16) & 0xffu, u >> 24);
    return float4(vals) * (1. / 255.);
}
#endif

// The Qualcomm compiler can't handle line breaks in #ifs.
// clang-format off
#if @GLSL_VERSION >= 310 && defined(@VERTEX) && defined(@RENDER_MODE_MSAA) && defined(@ENABLE_CLIP_RECT)
// clang-format on
out gl_PerVertex
{
    // Redeclare gl_ClipDistance with exactly 4 clip planes.
    float gl_ClipDistance[4];
    float4 gl_Position;
};
#endif

// The Qualcomm compiler can't handle line breaks in #ifs.
// clang-format off
#if defined(@TARGET_VULKAN) && defined(@FRAGMENT) && defined(@RENDER_MODE_MSAA) && !defined(@FIXED_FUNCTION_COLOR_OUTPUT)
// clang-format on
half4 dst_color_fetch(mediump mat4 dstSamples)
{
    if (gl_SampleMaskIn[0] == 0xf)
    {
        // Average together all samples for this fragment.
        return (dstSamples[0] + dstSamples[1] + dstSamples[2] + dstSamples[3]) *
               .25;
    }
    else
    {
        // Average together only the samples that are inside the sample mask.
        half4 mask =
            vec4(notEqual(gl_SampleMaskIn[0] & ivec4(1, 2, 4, 8), ivec4(0)));
        half4 ret = dstSamples * mask;
        // Since the sample mask can only have 4 bits, counting them is faster
        // this way on Galaxy S24 than calling bitCount().
        int numSamples =
            (gl_SampleMaskIn[0] & 5) + ((gl_SampleMaskIn[0] >> 1) & 5);
        numSamples = (numSamples & 3) + (numSamples >> 2);
        ret *= 1. / float(numSamples);
        return ret;
    }
}
#endif // @TARGET_VULKAN && @FRAGMENT && @RENDER_MODE_MSAA &&
       // !@FIXED_FUNCTION_COLOR_OUTPUT
