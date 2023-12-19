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
#define OUT(ARG_TYPE) out ARG_TYPE

#ifdef GL_ANGLE_base_vertex_base_instance_shader_builtin
#extension GL_ANGLE_base_vertex_base_instance_shader_builtin : require
#endif

#ifdef @ENABLE_BINDLESS_TEXTURES
#extension GL_ARB_bindless_texture : require
#endif

#if $__VERSION__ > 300
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
#if $__VERSION__ > 300
#define VARYING(IDX, TYPE, NAME) layout(location = IDX) out TYPE NAME
#else
#define VARYING(IDX, TYPE, NAME) out TYPE NAME
#endif
#else
#if $__VERSION__ > 300
#define VARYING(IDX, TYPE, NAME) layout(location = IDX) in TYPE NAME
#else
#define VARYING(IDX, TYPE, NAME) in TYPE NAME
#endif
#endif
#define FLAT flat
#define VARYING_BLOCK_BEGIN(NAME)
#define VARYING_BLOCK_END(_pos)

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
#define TEX32UIREF highp utexture2D
#elif $__VERSION__ > 300
#define TEXTURE_RGBA32UI(IDX, NAME) layout(binding = IDX) uniform highp usampler2D NAME
#define TEXTURE_RGBA32F(IDX, NAME) layout(binding = IDX) uniform highp sampler2D NAME
#define TEXTURE_RGBA8(IDX, NAME) layout(binding = IDX) uniform mediump sampler2D NAME
#define TEX32UIREF highp usampler2D
#else
#define TEXTURE_RGBA32UI(IDX, NAME) uniform highp usampler2D NAME
#define TEXTURE_RGBA32F(IDX, NAME) uniform highp sampler2D NAME
#define TEXTURE_RGBA8(IDX, NAME) uniform mediump sampler2D NAME
#define TEX32UIREF highp usampler2D
#endif
#define TEXTURE_DEREF(TEXTURE_BLOCK, NAME) NAME

#ifdef @TARGET_VULKAN
#define SAMPLER_LINEAR(TEXTURE_IDX, NAME)                                                          \
    layout(binding = TEXTURE_IDX, set = SAMPLER_BINDINGS_SET) uniform mediump sampler NAME;
#define SAMPLER_MIPMAP(TEXTURE_IDX, NAME)                                                          \
    layout(binding = TEXTURE_IDX, set = SAMPLER_BINDINGS_SET) uniform mediump sampler NAME;
#define TEXTURE_SAMPLE(NAME, SAMPLER_NAME, COORD) texture(sampler2D(NAME, SAMPLER_NAME), COORD)
#define TEXTURE_SAMPLE_LOD(NAME, SAMPLER_NAME, COORD, LOD)                                         \
    textureLod(sampler2D(NAME, SAMPLER_NAME), COORD, LOD)
#define TEXTURE_SAMPLE_GRAD(NAME, SAMPLER_NAME, COORD, DDX, DDY)                                   \
    textureGrad(sampler2D(NAME, SAMPLER_NAME), COORD, DDX, DDY)
#else
// SAMPLER_LINEAR and SAMPLER_MIPMAP are no-ops because in GL, sampling parameters are API-level
// state tied to the texture.
#define SAMPLER_LINEAR(TEXTURE_IDX, NAME)
#define SAMPLER_MIPMAP(TEXTURE_IDX, NAME)
#define TEXTURE_SAMPLE(NAME, SAMPLER_NAME, COORD) texture(NAME, COORD)
#define TEXTURE_SAMPLE_LOD(NAME, SAMPLER_NAME, COORD, LOD) textureLod(NAME, COORD, LOD)
#define TEXTURE_SAMPLE_GRAD(NAME, SAMPLER_NAME, COORD, DDX, DDY) textureGrad(NAME, COORD, DDX, DDY)
#endif

#define TEXEL_FETCH(NAME, COORD) texelFetch(NAME, COORD, 0)

// Define macros for implementing pixel local storage based on available extensions.
#ifdef @PLS_IMPL_WEBGL

#extension GL_ANGLE_shader_pixel_local_storage : require

#define PLS_BLOCK_BEGIN
#define PLS_DECL4F(IDX, NAME) layout(binding = IDX, rgba8) uniform lowp pixelLocalANGLE NAME
#define PLS_DECLUI(IDX, NAME) layout(binding = IDX, r32ui) uniform highp upixelLocalANGLE NAME
#define PLS_BLOCK_END

#define PLS_LOAD4F(P, _plsCoord) pixelLocalLoadANGLE(P)
#define PLS_LOADUI(P, _plsCoord) pixelLocalLoadANGLE(P).r
#define PLS_STORE4F(P, V, _plsCoord) pixelLocalStoreANGLE(P, V)
#define PLS_STOREUI(P, V, _plsCoord) pixelLocalStoreANGLE(P, uvec4(V))

#define PLS_PRESERVE_VALUE(P, _plsCoord)

#define PLS_INTERLOCK_BEGIN
#define PLS_INTERLOCK_END

#endif

#ifdef @PLS_IMPL_EXT_NATIVE

#extension GL_EXT_shader_pixel_local_storage : enable

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

#define PLS_LOAD4F(P, _plsCoord) P
#define PLS_LOADUI(P, _plsCoord) P
#define PLS_STORE4F(P, V, _plsCoord) P = (V)
#define PLS_STOREUI(P, V, _plsCoord) P = (V)

#define PLS_PRESERVE_VALUE(P, _plsCoord)

#define PLS_INTERLOCK_BEGIN
#define PLS_INTERLOCK_END

#endif

#ifdef @PLS_IMPL_FRAMEBUFFER_FETCH

#extension GL_EXT_shader_framebuffer_fetch : require

#define PLS_BLOCK_BEGIN
#define PLS_DECL4F(IDX, NAME) layout(location = IDX) inout lowp vec4 NAME
#define PLS_DECLUI(IDX, NAME) layout(location = IDX) inout highp uvec4 NAME
#define PLS_BLOCK_END

#define PLS_LOAD4F(P, _plsCoord) P
#define PLS_LOADUI(P, _plsCoord) P.r
#define PLS_STORE4F(P, V, _plsCoord) P = (V)
#define PLS_STOREUI(P, V, _plsCoord) P.r = (V)

// When using multiple color attachments, we have to write a value to every color attachment, every
// shader invocation, or else the contents become undefined.
#define PLS_PRESERVE_VALUE(P, _plsCoord) P = P

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
#else
#define PLS_INTERLOCK_BEGIN
#define PLS_INTERLOCK_END
#endif

#define PLS_BLOCK_BEGIN
#ifdef @TARGET_VULKAN
#define PLS_DECL4F(IDX, NAME)                                                                      \
    layout(set = PLS_TEXTURE_BINDINGS_SET, binding = IDX, rgba8) uniform lowp coherent image2D NAME
#define PLS_DECLUI(IDX, NAME)                                                                      \
    layout(set = PLS_TEXTURE_BINDINGS_SET, binding = IDX, r32ui)                                   \
        uniform highp coherent uimage2D NAME
#else
#define PLS_DECL4F(IDX, NAME) layout(binding = IDX, rgba8) uniform lowp coherent image2D NAME
#define PLS_DECLUI(IDX, NAME) layout(binding = IDX, r32ui) uniform highp coherent uimage2D NAME
#endif
#define PLS_BLOCK_END

#define PLS_LOAD4F(P, _plsCoord) imageLoad(P, _plsCoord)
#define PLS_LOADUI(P, _plsCoord) imageLoad(P, _plsCoord).r
#define PLS_STORE4F(P, V, _plsCoord) imageStore(P, _plsCoord, V)
#define PLS_STOREUI(P, V, _plsCoord) imageStore(P, _plsCoord, uvec4(V))

#define PLS_ATOMIC_MAX(PLANE, X, _plsCoord) imageAtomicMax(PLANE, _plsCoord, X)
#define PLS_ATOMIC_ADD(PLANE, X, _plsCoord) imageAtomicAdd(PLANE, _plsCoord, X)

#define PLS_PRESERVE_VALUE(P, _plsCoord)

#endif

#ifdef @PLS_IMPL_SUBPASS_LOAD

#define PLS_BLOCK_BEGIN
#define PLS_DECL4F(IDX, NAME)                                                                      \
    layout(input_attachment_index = IDX, binding = IDX, set = PLS_TEXTURE_BINDINGS_SET)            \
        uniform lowp subpassInput _in_##NAME;                                                      \
    layout(location = IDX) out lowp vec4 NAME
#define PLS_DECLUI(IDX, NAME)                                                                      \
    layout(input_attachment_index = IDX, binding = IDX, set = PLS_TEXTURE_BINDINGS_SET)            \
        uniform lowp usubpassInput _in_##NAME;                                                     \
    layout(location = IDX) out highp uvec4 NAME
#define PLS_BLOCK_END

#define PLS_LOAD4F(P, _plsCoord) subpassLoad(_in_##P)
#define PLS_LOADUI(P, _plsCoord) subpassLoad(_in_##P).r
#define PLS_STORE4F(P, V, _plsCoord) P = (V)
#define PLS_STOREUI(P, V, _plsCoord) P.r = (V)

#define PLS_PRESERVE_VALUE(P, _plsCoord) P = subpassLoad(_in_##P)
#define PLS_INTERLOCK_BEGIN
#define PLS_INTERLOCK_END

#endif

#ifdef @PLS_IMPL_NONE

#define PLS_BLOCK_BEGIN
#define PLS_DECL4F(IDX, NAME) layout(location = IDX) out lowp vec4 NAME
#define PLS_DECLUI(IDX, NAME) layout(location = IDX) out highp uvec4 NAME
#define PLS_BLOCK_END

#define PLS_LOAD4F(P, _plsCoord) vec4(0)
#define PLS_LOADUI(P, _plsCoord) 0u
#define PLS_STORE4F(P, V, _plsCoord) P = (V)
#define PLS_STOREUI(P, V, _plsCoord) P.r = (V)

#define PLS_PRESERVE_VALUE(P, _plsCoord)
#define PLS_INTERLOCK_BEGIN
#define PLS_INTERLOCK_END

#endif

#ifdef @TARGET_VULKAN
#define gl_VertexID gl_VertexIndex
#endif

// clang-format off
#ifdef @ENABLE_INSTANCE_INDEX
#  ifdef @TARGET_VULKAN
#    define INSTANCE_INDEX gl_InstanceIndex
#  else
#    ifdef @ENABLE_SPIRV_CROSS_BASE_INSTANCE
       // This uniform is specifically named "SPIRV_Cross_BaseInstance" for compatibility with
       // SPIRV-Cross sytems that search for it by name.
       uniform int $SPIRV_Cross_BaseInstance;
#      define INSTANCE_INDEX (gl_InstanceID + $SPIRV_Cross_BaseInstance)
#    else
#        define INSTANCE_INDEX (gl_InstanceID + gl_BaseInstance)
#    endif
#  endif
#else
#  define INSTANCE_INDEX 0
#endif
// clang-format on

// The Qualcomm compiler doesn't like how clang-format handles these lines.
// clang-format off
#define VERTEX_MAIN(NAME, Uniforms, uniforms, Attrs, attrs, Varyings, varyings, VertexTextures, textures, _vertexID, _instanceID, _pos) \
    void main()                                                                                    \
    {                                                                                              \
        int _vertexID = gl_VertexID;                                                               \
        int _instanceID = INSTANCE_INDEX;                                                          \
        vec4 _pos;
// clang-format on

// The Qualcomm compiler doesn't like how clang-format handles these lines.
// clang-format off
#define VERTEX_MAIN(NAME, Uniforms, uniforms, Attrs, attrs, Varyings, varyings, VertexTextures, textures, _vertexID, _instanceID, _pos) \
    void main()                                                                                    \
    {                                                                                              \
        int _vertexID = gl_VertexID;                                                               \
        int _instanceID = INSTANCE_INDEX;                                                          \
        vec4 _pos;

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
#define PLS_MAIN(NAME, Varyings, varyings, FragmentTextures, textures, _pos, _plsCoord)            \
    void main()                                                                                    \
    {                                                                                              \
        float2 _pos = gl_FragCoord.xy;                                                             \
        int2 _plsCoord = ivec2(floor(_pos));
#else
#define PLS_MAIN(NAME, Varyings, varyings, FragmentTextures, textures, _pos, _plsCoord)            \
    void main()                                                                                    \
    {
#endif

// clang-format off
#define IMAGE_DRAW_PLS_MAIN(NAME, MeshUniforms, meshUniforms, Varyings, varyings, FragmentTextures, textures, _pos, _plsCoord) \
    PLS_MAIN(NAME, Varyings, varyings, FragmentTextures, textures, _pos, _plsCoord)
// clang-format on

#define EMIT_PLS }

#define MUL(A, B) ((A) * (B))

#define STORAGE_BUFFER(IDX, STRUCT_NAME, TYPE, NAME)                                               \
    layout(std430, binding = IDX) readonly buffer STRUCT_NAME { TYPE values[]; }                   \
    NAME

#define STORAGE_BUFFER_AT(NAME, I) NAME.values[I]

precision highp float;
precision highp int;
