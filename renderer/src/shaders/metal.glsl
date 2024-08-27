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

#define INLINE $inline
#define OUT(ARG_TYPE) $thread ARG_TYPE&

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

#define ATTR_BLOCK_BEGIN(NAME)                                                                     \
    struct NAME                                                                                    \
    {
#define ATTR(IDX, TYPE, NAME) TYPE NAME
#define ATTR_BLOCK_END                                                                             \
    }                                                                                              \
    ;
#define ATTR_UNPACK(ID, attrs, NAME, TYPE) TYPE NAME = attrs[ID].NAME

#define VARYING_BLOCK_BEGIN                                                                        \
    struct Varyings                                                                                \
    {
#define VARYING(IDX, TYPE, NAME) TYPE NAME
#define FLAT [[flat]]
#define NO_PERSPECTIVE [[$center_no_perspective]]
#ifndef @OPTIONALLY_FLAT
// Don't use no-perspective interpolation for varyings that need to be flat. No-persective
// interpolation appears to break the guarantee that a varying == "x" when all barycentric values
// also == "x". Default (perspective-correct) interpolation does preserve this guarantee, and seems
// to be faster faster than flat on Apple Silicon.
#define @OPTIONALLY_FLAT
#endif
#define VARYING_BLOCK_END                                                                          \
    float4 _pos [[$position]] [[$invariant]];                                                      \
    }                                                                                              \
    ;

#define VARYING_INIT(NAME, TYPE) $thread TYPE& NAME = _varyings.NAME
#define VARYING_PACK(NAME)
#define VARYING_UNPACK(NAME, TYPE) TYPE NAME = _varyings.NAME

#define VERTEX_STORAGE_BUFFER_BLOCK_BEGIN                                                          \
    struct VertexStorageBuffers                                                                    \
    {
#define VERTEX_STORAGE_BUFFER_BLOCK_END                                                            \
    }                                                                                              \
    ;

#define FRAG_STORAGE_BUFFER_BLOCK_BEGIN                                                            \
    struct FragmentStorageBuffers                                                                  \
    {
#define FRAG_STORAGE_BUFFER_BLOCK_END                                                              \
    }                                                                                              \
    ;

#define STORAGE_BUFFER_U32x2(IDX, GLSL_STRUCT_NAME, NAME) $constant uint2* NAME [[$buffer(IDX)]]
#define STORAGE_BUFFER_U32x4(IDX, GLSL_STRUCT_NAME, NAME) $constant uint4* NAME [[$buffer(IDX)]]
#define STORAGE_BUFFER_F32x4(IDX, GLSL_STRUCT_NAME, NAME) $constant float4* NAME [[$buffer(IDX)]]
#define STORAGE_BUFFER_LOAD4(NAME, I) _buffers.NAME[I]
#define STORAGE_BUFFER_LOAD2(NAME, I) _buffers.NAME[I]

#define VERTEX_TEXTURE_BLOCK_BEGIN                                                                 \
    struct VertexTextures                                                                          \
    {
#define VERTEX_TEXTURE_BLOCK_END                                                                   \
    }                                                                                              \
    ;

#define FRAG_TEXTURE_BLOCK_BEGIN                                                                   \
    struct FragmentTextures                                                                        \
    {
#define FRAG_TEXTURE_BLOCK_END                                                                     \
    }                                                                                              \
    ;

#define TEXTURE_RGBA32UI(SET, IDX, NAME) [[$texture(IDX)]] $texture2d<uint> NAME
#define TEXTURE_RGBA32F(SET, IDX, NAME) [[$texture(IDX)]] $texture2d<float> NAME
#define TEXTURE_RGBA8(SET, IDX, NAME) [[$texture(IDX)]] $texture2d<half> NAME

#define SAMPLER_LINEAR(TEXTURE_IDX, NAME)                                                          \
    $constexpr $sampler NAME($filter::$linear, $mip_filter::$none);
#define SAMPLER_MIPMAP(TEXTURE_IDX, NAME)                                                          \
    $constexpr $sampler NAME($filter::$linear, $mip_filter::$linear);

#define TEXEL_FETCH(TEXTURE, COORD) _textures.TEXTURE.$read(uint2(COORD))
#define TEXTURE_SAMPLE(TEXTURE, SAMPLER_NAME, COORD) _textures.TEXTURE.$sample(SAMPLER_NAME, COORD)
#define TEXTURE_SAMPLE_LOD(TEXTURE, SAMPLER_NAME, COORD, LOD)                                      \
    _textures.TEXTURE.$sample(SAMPLER_NAME, COORD, $level(LOD))
#define TEXTURE_SAMPLE_GRAD(TEXTURE, SAMPLER_NAME, COORD, DDX, DDY)                                \
    _textures.TEXTURE.$sample(SAMPLER_NAME, COORD, $gradient2d(DDX, DDY))

#define VERTEX_CONTEXT_DECL , VertexTextures _textures, VertexStorageBuffers _buffers
#define VERTEX_CONTEXT_UNPACK , _textures, _buffers

#ifdef @ENABLE_INSTANCE_INDEX
#define VERTEX_MAIN(NAME, Attrs, attrs, _vertexID, _instanceID)                                    \
    $__attribute__(($visibility("default"))) Varyings $vertex NAME(                                \
        uint _vertexID [[$vertex_id]],                                                             \
        uint _instanceID [[$instance_id]],                                                         \
        $constant uint& _baseInstance [[$buffer(PATH_BASE_INSTANCE_UNIFORM_BUFFER_IDX)]],          \
        $constant @FlushUniforms& uniforms [[$buffer(FLUSH_UNIFORM_BUFFER_IDX)]],                  \
        $constant Attrs* attrs [[$buffer(0)]] VERTEX_CONTEXT_DECL)                                 \
    {                                                                                              \
        _instanceID += _baseInstance;                                                              \
        Varyings _varyings;
#else
#define VERTEX_MAIN(NAME, Attrs, attrs, _vertexID, _instanceID)                                    \
    $__attribute__(($visibility("default"))) Varyings $vertex NAME(                                \
        uint _vertexID [[$vertex_id]],                                                             \
        uint _instanceID [[$instance_id]],                                                         \
        $constant @FlushUniforms& uniforms [[$buffer(FLUSH_UNIFORM_BUFFER_IDX)]],                  \
        $constant Attrs* attrs [[$buffer(0)]] VERTEX_CONTEXT_DECL)                                 \
    {                                                                                              \
        Varyings _varyings;
#endif

#define IMAGE_RECT_VERTEX_MAIN(NAME, Attrs, attrs, _vertexID, _instanceID)                         \
    $__attribute__(($visibility("default"))) Varyings $vertex NAME(                                \
        uint _vertexID [[$vertex_id]],                                                             \
        $constant @FlushUniforms& uniforms [[$buffer(FLUSH_UNIFORM_BUFFER_IDX)]],                  \
        $constant @ImageDrawUniforms& imageDrawUniforms                                            \
        [[$buffer(IMAGE_DRAW_UNIFORM_BUFFER_IDX)]],                                                \
        $constant Attrs* attrs [[$buffer(0)]] VERTEX_CONTEXT_DECL)                                 \
    {                                                                                              \
        Varyings _varyings;

#define IMAGE_MESH_VERTEX_MAIN(NAME, PositionAttr, position, UVAttr, uv, _vertexID)                \
    $__attribute__(($visibility("default"))) Varyings $vertex NAME(                                \
        uint _vertexID [[$vertex_id]],                                                             \
        $constant @FlushUniforms& uniforms [[$buffer(FLUSH_UNIFORM_BUFFER_IDX)]],                  \
        $constant @ImageDrawUniforms& imageDrawUniforms                                            \
        [[$buffer(IMAGE_DRAW_UNIFORM_BUFFER_IDX)]],                                                \
        $constant PositionAttr* position [[$buffer(0)]],                                           \
        $constant UVAttr* uv [[$buffer(1)]])                                                       \
    {                                                                                              \
        Varyings _varyings;

#define EMIT_VERTEX(POSITION)                                                                      \
    _varyings._pos = POSITION;                                                                     \
    }                                                                                              \
    return _varyings;

#define FRAG_DATA_MAIN(DATA_TYPE, NAME)                                                            \
    DATA_TYPE $__attribute__(($visibility("default"))) $fragment NAME(Varyings _varyings           \
                                                                      [[$stage_in]])               \
    {

#define EMIT_FRAG_DATA(VALUE)                                                                      \
    return VALUE;                                                                                  \
    }

#define FRAGMENT_CONTEXT_DECL                                                                      \
    , float2 _fragCoord, FragmentTextures _textures, FragmentStorageBuffers _buffers
#define FRAGMENT_CONTEXT_UNPACK , _fragCoord, _textures, _buffers

#ifdef @PLS_IMPL_DEVICE_BUFFER

#define PLS_BLOCK_BEGIN                                                                            \
    struct PLS                                                                                     \
    {
#ifdef @PLS_IMPL_DEVICE_BUFFER_RASTER_ORDERED
// Apple Silicon doesn't support fragment-fragment memory barriers, so on this hardware we use
// raster order groups instead.
// Since the PLS plane indices collide with other buffer bindings, offset the binding indices of
// these buffers by DEFAULT_BINDINGS_SET_SIZE.
#define PLS_DECL4F(IDX, NAME)                                                                      \
    $device uint* NAME [[$buffer(IDX + DEFAULT_BINDINGS_SET_SIZE), $raster_order_group(0)]]
#define PLS_DECLUI(IDX, NAME)                                                                      \
    $device uint* NAME [[$buffer(IDX + DEFAULT_BINDINGS_SET_SIZE), $raster_order_group(0)]]
#define PLS_DECLUI_ATOMIC(IDX, NAME)                                                               \
    $device $atomic_uint* NAME [[$buffer(IDX + DEFAULT_BINDINGS_SET_SIZE), $raster_order_group(0)]]
#else
// Since the PLS plane indices collide with other buffer bindings, offset the binding indices of
// these buffers by DEFAULT_BINDINGS_SET_SIZE.
#define PLS_DECL4F(IDX, NAME) $device uint* NAME [[$buffer(IDX + DEFAULT_BINDINGS_SET_SIZE)]]
#define PLS_DECLUI(IDX, NAME) $device uint* NAME [[$buffer(IDX + DEFAULT_BINDINGS_SET_SIZE)]]
#define PLS_DECLUI_ATOMIC(IDX, NAME)                                                               \
    $device $atomic_uint* NAME [[$buffer(IDX + DEFAULT_BINDINGS_SET_SIZE)]]
#endif // @PLS_IMPL_DEVICE_BUFFER_RASTER_ORDERED
#define PLS_BLOCK_END                                                                              \
    }                                                                                              \
    ;
#define PLS_CONTEXT_DECL , PLS _pls, uint _plsIdx
#define PLS_CONTEXT_UNPACK , _pls, _plsIdx

#define PLS_LOAD4F(PLANE) unpackUnorm4x8(_pls.PLANE[_plsIdx])
#define PLS_LOADUI(PLANE) _pls.PLANE[_plsIdx]
#define PLS_LOADUI_ATOMIC(PLANE)                                                                   \
    $atomic_load_explicit(&_pls.PLANE[_plsIdx], $memory_order::$memory_order_relaxed)
#define PLS_STORE4F(PLANE, VALUE) _pls.PLANE[_plsIdx] = packUnorm4x8(VALUE)
#define PLS_STOREUI(PLANE, VALUE) _pls.PLANE[_plsIdx] = (VALUE)
#define PLS_STOREUI_ATOMIC(PLANE, VALUE)                                                           \
    $atomic_store_explicit(&_pls.PLANE[_plsIdx], VALUE, $memory_order::$memory_order_relaxed)
#define PLS_PRESERVE_4F(PLANE)
#define PLS_PRESERVE_UI(PLANE)

#define PLS_ATOMIC_MAX(PLANE, X)                                                                   \
    $atomic_fetch_max_explicit(&_pls.PLANE[_plsIdx], X, $memory_order::$memory_order_relaxed)

#define PLS_ATOMIC_ADD(PLANE, X)                                                                   \
    $atomic_fetch_add_explicit(&_pls.PLANE[_plsIdx], X, $memory_order::$memory_order_relaxed)

#define PLS_INTERLOCK_BEGIN
#define PLS_INTERLOCK_END

#define PLS_METAL_MAIN(NAME)                                                                       \
    $__attribute__(($visibility("default"))) $fragment NAME(PLS _pls,                              \
                                                            $constant @FlushUniforms& uniforms     \
                                                            [[$buffer(FLUSH_UNIFORM_BUFFER_IDX)]], \
                                                            Varyings _varyings [[$stage_in]],      \
                                                            FragmentTextures _textures,            \
                                                            FragmentStorageBuffers _buffers)       \
    {                                                                                              \
        float2 _fragCoord = _varyings._pos.xy;                                                     \
        uint2 _plsCoord = uint2($metal::floor(_fragCoord));                                        \
        uint _plsIdx = _plsCoord.y * uniforms.renderTargetWidth + _plsCoord.x;

#define PLS_METAL_MAIN_WITH_IMAGE_UNIFORMS(NAME)                                                   \
    $__attribute__(($visibility("default"))) $fragment NAME(                                       \
        PLS _pls,                                                                                  \
        $constant @FlushUniforms& uniforms [[$buffer(FLUSH_UNIFORM_BUFFER_IDX)]],                  \
        $constant @ImageDrawUniforms& imageDrawUniforms                                            \
        [[$buffer(IMAGE_DRAW_UNIFORM_BUFFER_IDX)]],                                                \
        Varyings _varyings [[$stage_in]],                                                          \
        FragmentTextures _textures,                                                                \
        FragmentStorageBuffers _buffers)                                                           \
    {                                                                                              \
        float2 _fragCoord = _varyings._pos.xy;                                                     \
        uint2 _plsCoord = uint2($metal::floor(_fragCoord));                                        \
        uint _plsIdx = _plsCoord.y * uniforms.renderTargetWidth + _plsCoord.x;

#define PLS_MAIN(NAME) void PLS_METAL_MAIN(NAME)
#define PLS_MAIN_WITH_IMAGE_UNIFORMS(NAME) void PLS_METAL_MAIN_WITH_IMAGE_UNIFORMS(NAME)
#define EMIT_PLS }

#define PLS_FRAG_COLOR_MAIN(NAME)                                                                  \
    half4 PLS_METAL_MAIN(NAME)                                                                     \
    {                                                                                              \
        half4 _fragColor;

#define PLS_FRAG_COLOR_MAIN_WITH_IMAGE_UNIFORMS(NAME)                                              \
    half4 PLS_METAL_MAIN_WITH_IMAGE_UNIFORMS(NAME)                                                 \
    {                                                                                              \
        half4 _fragColor;

#define EMIT_PLS_AND_FRAG_COLOR                                                                    \
    }                                                                                              \
    return _fragColor;                                                                             \
    EMIT_PLS

#else // Default implementation -- framebuffer reads.

#define PLS_BLOCK_BEGIN                                                                            \
    struct PLS                                                                                     \
    {
#define PLS_DECL4F(IDX, NAME) [[$color(IDX)]] half4 NAME
#define PLS_DECLUI(IDX, NAME) [[$color(IDX)]] uint NAME
#define PLS_DECLUI_ATOMIC PLS_DECLUI
#define PLS_BLOCK_END                                                                              \
    }                                                                                              \
    ;
#define PLS_CONTEXT_DECL , $thread PLS &_inpls, $thread PLS &_pls
#define PLS_CONTEXT_UNPACK , _inpls, _pls

#define PLS_LOAD4F(PLANE) _inpls.PLANE
#define PLS_LOADUI(PLANE) _inpls.PLANE
#define PLS_LOADUI_ATOMIC(PLANE) PLS_LOADUI
#define PLS_STORE4F(PLANE, VALUE) _pls.PLANE = (VALUE)
#define PLS_STOREUI(PLANE, VALUE) _pls.PLANE = (VALUE)
#define PLS_STOREUI_ATOMIC(PLANE) PLS_STOREUI
#define PLS_PRESERVE_4F(PLANE) _pls.PLANE = _inpls.PLANE
#define PLS_PRESERVE_UI(PLANE) _pls.PLANE = _inpls.PLANE

INLINE uint pls_atomic_max($thread uint& dst, uint x)
{
    uint originalValue = dst;
    dst = $metal::max(originalValue, x);
    return originalValue;
}

#define PLS_ATOMIC_MAX(PLANE, X) pls_atomic_max(_pls.PLANE, X)

INLINE uint pls_atomic_add($thread uint& dst, uint x)
{
    uint originalValue = dst;
    dst = originalValue + x;
    return originalValue;
}

#define PLS_ATOMIC_ADD(PLANE, X) pls_atomic_add(_pls.PLANE, X)

#define PLS_INTERLOCK_BEGIN
#define PLS_INTERLOCK_END

#define PLS_METAL_MAIN(NAME, ...)                                                                  \
    PLS $__attribute__(($visibility("default"))) $fragment NAME($__VA_ARGS__)                      \
    {                                                                                              \
        float2 _fragCoord [[$maybe_unused]] = _varyings._pos.xy;                                   \
        PLS _pls;

#define PLS_MAIN(NAME, ...)                                                                        \
    PLS_METAL_MAIN(NAME,                                                                           \
                   PLS _inpls,                                                                     \
                   Varyings _varyings [[$stage_in]],                                               \
                   FragmentTextures _textures,                                                     \
                   FragmentStorageBuffers _buffers)

#define PLS_MAIN_WITH_IMAGE_UNIFORMS(NAME)                                                         \
    PLS_METAL_MAIN(NAME,                                                                           \
                   PLS _inpls,                                                                     \
                   Varyings _varyings [[$stage_in]],                                               \
                   FragmentTextures _textures,                                                     \
                   FragmentStorageBuffers _buffers,                                                \
                   $constant @ImageDrawUniforms& imageDrawUniforms                                 \
                   [[$buffer(IMAGE_DRAW_UNIFORM_BUFFER_IDX)]])

#define EMIT_PLS                                                                                   \
    }                                                                                              \
    return _pls;

#define PLS_FRAG_COLOR_METAL_MAIN(NAME, ...)                                                       \
    struct FragmentOut                                                                             \
    {                                                                                              \
        half4 _color [[color(0)]];                                                                 \
        PLS _pls;                                                                                  \
    };                                                                                             \
    FragmentOut $__attribute__(($visibility("default"))) $fragment NAME($__VA_ARGS__)              \
    {                                                                                              \
        float2 _fragCoord [[$maybe_unused]] = _varyings._pos.xy;                                   \
        half4 _fragColor;                                                                          \
        PLS _pls;

#define PLS_FRAG_COLOR_MAIN(NAME)                                                                  \
    PLS_FRAG_COLOR_METAL_MAIN(NAME,                                                                \
                              PLS _inpls,                                                          \
                              Varyings _varyings [[$stage_in]],                                    \
                              FragmentTextures _textures,                                          \
                              FragmentStorageBuffers _buffers)

#define PLS_FRAG_COLOR_MAIN_WITH_IMAGE_UNIFORMS(NAME)                                              \
    PLS_FRAG_COLOR_METAL_MAIN(NAME,                                                                \
                              PLS _inpls,                                                          \
                              Varyings _varyings [[$stage_in]],                                    \
                              FragmentTextures _textures,                                          \
                              FragmentStorageBuffers _buffers,                                     \
                              $__VA_ARGS__ $constant @ImageDrawUniforms& imageDrawUniforms         \
                              [[$buffer(IMAGE_DRAW_UNIFORM_BUFFER_IDX)]])

#define EMIT_PLS_AND_FRAG_COLOR                                                                    \
    }                                                                                              \
    return {._color = _fragColor, ._pls = _pls};

#endif // PLS_IMPL_DEVICE_BUFFER

#define discard $discard_fragment()

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
INLINE half4 unpackUnorm4x8(uint x) { return $unpack_unorm4x8_to_half(x); }
INLINE uint packUnorm4x8(half4 x) { return $pack_half_to_unorm4x8(x); }

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
