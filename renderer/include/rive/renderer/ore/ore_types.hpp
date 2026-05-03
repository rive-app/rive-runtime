/*
 * Copyright 2025 Rive
 */

#pragma once

#include <cstdint>

namespace rive::ore
{

// ============================================================================
// Constants
// ============================================================================

// Maximum number of bind groups Ore supports per pipeline. WebGPU's
// `maxBindGroups` minimum is 4 (RFC §9.1 / §14.2), and Ore sits at that
// minimum — backends preallocate per-group structures using this
// constant (Vulkan DSLs, WebGPU BGLs, D3D12 root params, RenderPass
// `m_boundGroups`). Single source of truth across every backend's
// per-group array, the public `BindGroupDesc::groupIndex` validity
// range, and Lua-side validation in `gpubindgroup_construct` /
// `setBindGroup`.
constexpr uint32_t kMaxBindGroups = 4;

// ============================================================================
// Enums
// ============================================================================

enum class BufferUsage : uint8_t
{
    vertex,
    index,
    uniform,
};

enum class ShaderLanguage : uint8_t
{
    glsl, // GLSL via wagyu (GLSLRAW for GLES, GLSL/glslang for Vulkan).
    wgsl, // WGSL — works on both GLES and Vulkan wagyu paths, no pragma needed.
};

enum class ShaderStage : uint8_t
{
    autoDetect, // Infer from source content (legacy gl_Position heuristic).
    vertex,
    fragment,
};

enum class TextureFormat : uint8_t
{
    // 8-bit
    r8unorm,
    rg8unorm,
    rgba8unorm,
    rgba8snorm,
    bgra8unorm,

    // 16-bit float
    rgba16float,
    rg16float,
    r16float,

    // 32-bit float
    rgba32float,
    rg32float,
    r32float,

    // Packed
    rgb10a2unorm,
    r11g11b10float,

    // Depth/stencil
    depth16unorm,
    depth24plusStencil8,
    depth32float,
    depth32floatStencil8,

    // Compressed (runtime support via Features query)
    bc1unorm,
    bc3unorm,
    bc7unorm,
    etc2rgb8,
    etc2rgba8,
    astc4x4,
    astc6x6,
    astc8x8,
};

// Returns bytes per texel for uncompressed formats, or 0 for block-compressed
// formats (which require block-based stride calculation).
inline uint32_t textureFormatBytesPerTexel(TextureFormat fmt)
{
    switch (fmt)
    {
        case TextureFormat::r8unorm:
            return 1;
        case TextureFormat::rg8unorm:
            return 2;
        case TextureFormat::rgba8unorm:
            return 4;
        case TextureFormat::rgba8snorm:
            return 4;
        case TextureFormat::bgra8unorm:
            return 4;
        case TextureFormat::rgba16float:
            return 8;
        case TextureFormat::rg16float:
            return 4;
        case TextureFormat::r16float:
            return 2;
        case TextureFormat::rgba32float:
            return 16;
        case TextureFormat::rg32float:
            return 8;
        case TextureFormat::r32float:
            return 4;
        case TextureFormat::rgb10a2unorm:
            return 4;
        case TextureFormat::r11g11b10float:
            return 4;
        case TextureFormat::depth16unorm:
            return 2;
        case TextureFormat::depth24plusStencil8:
            return 4;
        case TextureFormat::depth32float:
            return 4;
        case TextureFormat::depth32floatStencil8:
            return 8;
        default:
            return 0; // block-compressed
    }
}

enum class TextureType : uint8_t
{
    texture2D,
    cube,
    texture3D,
    array2D,
};

enum class TextureViewDimension : uint8_t
{
    texture2D,
    cube,
    texture3D,
    array2D,
    cubeArray,
};

enum class TextureAspect : uint8_t
{
    all,
    depthOnly,
    stencilOnly,
};

enum class Filter : uint8_t
{
    nearest,
    linear,
};

enum class WrapMode : uint8_t
{
    repeat,
    mirrorRepeat,
    clampToEdge,
};

enum class CompareFunction : uint8_t
{
    none, // Not a comparison (default for samplers = normal filtering).
    never,
    less,
    equal,
    lessEqual,
    greater,
    notEqual,
    greaterEqual,
    always,
};

enum class PrimitiveTopology : uint8_t
{
    pointList,
    lineList,
    lineStrip,
    triangleList,
    triangleStrip,
};

enum class IndexFormat : uint8_t
{
    none,
    uint16,
    uint32,
};

enum class VertexFormat : uint8_t
{
    float1,
    float2,
    float3,
    float4,
    uint8x4,
    sint8x4,
    unorm8x4,
    snorm8x4,
    uint16x2,
    sint16x2,
    unorm16x2,
    snorm16x2,
    uint16x4,
    sint16x4,
    float16x2,
    float16x4,
    uint32,
    uint32x2,
    uint32x3,
    uint32x4,
    sint32,
    sint32x2,
    sint32x3,
    sint32x4,
};

enum class VertexStepMode : uint8_t
{
    vertex,
    instance,
};

enum class CullMode : uint8_t
{
    none,
    front,
    back,
};

enum class FaceWinding : uint8_t
{
    clockwise,
    counterClockwise,
};

enum class BlendFactor : uint8_t
{
    zero,
    one,
    srcColor,
    oneMinusSrcColor,
    srcAlpha,
    oneMinusSrcAlpha,
    dstColor,
    oneMinusDstColor,
    dstAlpha,
    oneMinusDstAlpha,
    srcAlphaSaturated,
    blendColor,
    oneMinusBlendColor,
};

enum class BlendOp : uint8_t
{
    add,
    subtract,
    reverseSubtract,
    min,
    max,
};

enum class StencilOp : uint8_t
{
    keep,
    zero,
    replace,
    incrementClamp,
    decrementClamp,
    invert,
    incrementWrap,
    decrementWrap,
};

enum class LoadOp : uint8_t
{
    clear,
    load,
    dontCare,
};

enum class StoreOp : uint8_t
{
    store,
    discard,
};

enum class ColorWriteMask : uint8_t
{
    none = 0,
    red = 1 << 0,
    green = 1 << 1,
    blue = 1 << 2,
    alpha = 1 << 3,
    all = 0xF,
};

inline ColorWriteMask operator|(ColorWriteMask a, ColorWriteMask b)
{
    return static_cast<ColorWriteMask>(static_cast<uint8_t>(a) |
                                       static_cast<uint8_t>(b));
}

inline ColorWriteMask operator&(ColorWriteMask a, ColorWriteMask b)
{
    return static_cast<ColorWriteMask>(static_cast<uint8_t>(a) &
                                       static_cast<uint8_t>(b));
}

// ============================================================================
// Forward declarations
// ============================================================================

class Buffer;
class Texture;
class TextureView;
class Sampler;
class ShaderModule;
class Pipeline;
class BindGroupLayout;

// ============================================================================
// Descriptor Structs
// ============================================================================

struct BufferDesc
{
    BufferUsage usage;
    uint32_t size = 0;
    const void* data = nullptr;
    bool immutable = false; // GPU-only allocation; data must be non-null; no
                            // update() calls allowed after creation.
    const char* label = nullptr;
};

struct TextureDesc
{
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depthOrArrayLayers = 1;
    TextureFormat format = TextureFormat::rgba8unorm;
    TextureType type = TextureType::texture2D;
    bool renderTarget = false;
    uint32_t numMipmaps = 1;
    uint32_t sampleCount = 1;
    const char* label = nullptr;
};

struct TextureViewDesc
{
    Texture* texture = nullptr;
    TextureViewDimension dimension = TextureViewDimension::texture2D;
    TextureAspect aspect = TextureAspect::all;
    uint32_t baseMipLevel = 0;
    uint32_t mipCount = 1;
    uint32_t baseLayer = 0;
    uint32_t layerCount = 1;
};

struct TextureDataDesc
{
    const void* data = nullptr;
    uint32_t bytesPerRow = 0;
    uint32_t rowsPerImage = 0;
    uint32_t mipLevel = 0;
    uint32_t layer = 0;
    uint32_t x = 0, y = 0, z = 0;
    uint32_t width = 0, height = 0, depth = 1;
};

struct SamplerDesc
{
    Filter minFilter = Filter::nearest;
    Filter magFilter = Filter::nearest;
    Filter mipmapFilter = Filter::nearest;
    WrapMode wrapU = WrapMode::clampToEdge;
    WrapMode wrapV = WrapMode::clampToEdge;
    WrapMode wrapW = WrapMode::clampToEdge;
    CompareFunction compare = CompareFunction::none;
    float minLod = 0.0f;
    float maxLod = 32.0f;
    uint32_t maxAnisotropy = 1;
    const char* label = nullptr;
};

struct ShaderModuleDesc
{
    const void* code = nullptr;
    uint32_t codeSize = 0;
    ShaderLanguage language = ShaderLanguage::glsl;
    ShaderStage stage = ShaderStage::autoDetect;
    const char* label = nullptr;

    // D3D11 only: HLSL source for runtime compilation via D3DCompile.
    // When set, the shader is compiled at first use (ensureD3DShaders)
    // rather than from pre-compiled DXBC. This is required on AMD because
    // D3DCompile produces different DXBC per process context.
    const char* hlslSource = nullptr;
    uint32_t hlslSourceSize = 0;
    const char* hlslEntryPoint = nullptr;

    // Required binding-map sidecar bytes from the RSTB (target IDs 10-13,
    // 16 — one per source backend). `makeShaderModule` parses them via
    // `ore::BindingMap::fromBlob` into `ShaderModule::m_bindingMap`. The
    // sidecar is mandatory (RFC §14.4): a missing or unparseable blob is a
    // programming error, not a fallback condition.
    const uint8_t* bindingMapBytes = nullptr;
    uint32_t bindingMapSize = 0;

    // GL program-link fixup blob from the RSTB (target IDs 14/15, one per
    // GLSL stage). Consumed by `oreGLFixupProgramBindings` at
    // `glLinkProgram` time to call `glUniformBlockBinding` / `glUniform1i`
    // by uniform name — no runtime string parsing. Required when the
    // module's source target is GLSL (target 1); null for other targets.
    const uint8_t* glFixupBytes = nullptr;
    uint32_t glFixupSize = 0;
};

struct VertexAttribute
{
    VertexFormat format = VertexFormat::float4;
    uint32_t offset = 0;
    uint32_t shaderSlot = 0;
};

struct VertexBufferLayout
{
    uint32_t stride = 0;
    VertexStepMode stepMode = VertexStepMode::vertex;
    const VertexAttribute* attributes = nullptr;
    uint32_t attributeCount = 0;
};

struct BlendState
{
    BlendFactor srcColor = BlendFactor::one;
    BlendFactor dstColor = BlendFactor::zero;
    BlendOp colorOp = BlendOp::add;
    BlendFactor srcAlpha = BlendFactor::one;
    BlendFactor dstAlpha = BlendFactor::zero;
    BlendOp alphaOp = BlendOp::add;
};

struct ColorTargetState
{
    TextureFormat format = TextureFormat::bgra8unorm;
    bool blendEnabled = false;
    BlendState blend;
    ColorWriteMask writeMask = ColorWriteMask::all;
};

struct StencilFaceState
{
    CompareFunction compare = CompareFunction::always;
    StencilOp failOp = StencilOp::keep;
    StencilOp depthFailOp = StencilOp::keep;
    StencilOp passOp = StencilOp::keep;
};

struct DepthStencilState
{
    // rgba8unorm is the sentinel for "no depth/stencil attachment" (checked by
    // the Vulkan backend in ore_pipeline_vulkan.cpp). Set this to an actual
    // depth format to attach a depth/stencil buffer to the pipeline.
    TextureFormat format = TextureFormat::rgba8unorm;
    CompareFunction depthCompare = CompareFunction::always;
    bool depthWriteEnabled = false;
    int32_t depthBias = 0;
    float depthBiasSlopeScale = 0.0f;
    float depthBiasClamp = 0.0f;
};

// ============================================================================
// BindGroupLayout Descriptor — explicit layout, Dawn-shaped (RFC §3.2 / §6).
//
// One layout per WGSL `@group(N)`. Created via `Context::makeBindGroupLayout`
// and consumed by both `PipelineDesc::bindGroupLayouts[]` and
// `BindGroupDesc::layout`. Replaces the previous "BindGroup is built against
// a Pipeline" coupling — the layout is the contract that pipelines and bind
// groups agree on, so a single BindGroup can be used with any pipeline that
// declares the same layout for its corresponding group.
// ============================================================================

// Resource kind a layout entry describes. Values intentionally distinct from
// `ore::ResourceKind` (in ore_binding_map.hpp) so the public layout API
// doesn't drag the binding-map header into every translation unit. The two
// are mapped 1:1 inside the layout-creation code.
enum class BindingKind : uint8_t
{
    uniformBuffer,
    storageBufferRO,
    storageBufferRW,
    sampledTexture,
    storageTexture,
    sampler,
    comparisonSampler,
};

// Stage-visibility bits. Bitwise-OR of `kStageVS` / `kStageFS` (compute is
// reserved for future use). Mirrors WebGPU's `GPUShaderStage` flagset.
struct StageVisibility
{
    static constexpr uint8_t kVertex = 1u << 0;
    static constexpr uint8_t kFragment = 1u << 1;
    static constexpr uint8_t kCompute = 1u << 2;

    uint8_t mask = kVertex | kFragment;
};

struct BindGroupLayoutEntry
{
    // WGSL `@binding(N)` value this entry describes. The (group, binding)
    // pair must agree with the shader's reflected binding map; mismatch
    // is rejected at `makePipeline` time with a clear error.
    uint32_t binding = 0;

    BindingKind kind = BindingKind::uniformBuffer;

    // Stage-visibility mask. Default vertex+fragment matches the most
    // common case; visibility narrower than the shader's reflected
    // stageMask is rejected at makePipeline (broader is fine).
    StageVisibility visibility;

    // UBO-only: declares the binding will receive a dynamic offset at
    // `setBindGroup` time. Vulkan DSL picks
    // `VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC` vs `UNIFORM_BUFFER`;
    // D3D12 uses a root CBV instead of an in-table CBV; Metal / GL /
    // D3D11 cache the flag for per-draw `setVertexBuffer` offset apply.
    bool hasDynamicOffset = false;

    // Texture-only: dimension + sample type + multisampled. Drives the
    // WebGPU BGL entry's `texture.{viewDimension, sampleType, multisampled}`.
    // Ignored for non-texture kinds.
    TextureViewDimension textureViewDim = TextureViewDimension::texture2D;
    // sampleType is one of "float", "unfilterable-float", "depth", "sint",
    // "uint" in WebGPU. Encoded compactly here as a small enum that
    // backends map to the Dawn enum.
    enum class SampleType : uint8_t
    {
        floatFilterable,
        floatUnfilterable,
        depth,
        sint,
        uint,
    };
    SampleType textureSampleType = SampleType::floatFilterable;
    bool textureMultisampled = false;

    // UBO-only: smallest valid bind size for this entry. 0 = no minimum
    // (use the full buffer range). Matches WebGPU's
    // `BindGroupLayoutEntry::buffer.minBindingSize`. Currently advisory —
    // backends don't yet enforce.
    uint32_t minBindingSize = 0;

    // Pre-resolved native slots, per-stage. Populated by the caller from
    // the shader's binding map (typically via the GM helper
    // `makeLayoutFromShader(ctx, shader, group)`). Used by backends with
    // no native layout object (Metal: buffer index; D3D11: per-stage
    // register; GL: global slot). Vulkan and WebGPU ignore — those
    // backends use `binding` directly (per-set namespace).
    //
    // 0xFFFFFFFF = `kAbsent` (binding not visible to that stage). Default
    // is "absent in all stages" so a layout-without-shader-resolution
    // works on Vulkan/WebGPU but fails loudly on Metal/D3D11/GL.
    static constexpr uint32_t kNativeSlotAbsent = 0xFFFFFFFFu;
    uint32_t nativeSlotVS = kNativeSlotAbsent;
    uint32_t nativeSlotFS = kNativeSlotAbsent;
    uint32_t nativeSlotCS = kNativeSlotAbsent;
};

struct BindGroupLayoutDesc
{
    // WGSL `@group(N)` this layout describes. Valid range [0, kMaxBindGroups).
    uint32_t groupIndex = 0;

    const BindGroupLayoutEntry* entries = nullptr;
    uint32_t entryCount = 0;

    const char* label = nullptr;
};

struct PipelineDesc
{
    ShaderModule* vertexModule = nullptr;
    const char* vertexEntryPoint = "vs_main";
    ShaderModule* fragmentModule = nullptr;
    const char* fragmentEntryPoint = "fs_main";

    const VertexBufferLayout* vertexBuffers = nullptr;
    uint32_t vertexBufferCount = 0;

    PrimitiveTopology topology = PrimitiveTopology::triangleList;
    IndexFormat indexFormat = IndexFormat::none;
    CullMode cullMode = CullMode::none;
    FaceWinding winding = FaceWinding::counterClockwise;

    ColorTargetState colorTargets[4] = {};
    uint32_t colorCount = 1;

    DepthStencilState depthStencil;

    StencilFaceState stencilFront;
    StencilFaceState stencilBack;
    uint8_t stencilReadMask = 0xFF;
    uint8_t stencilWriteMask = 0xFF;

    uint32_t sampleCount = 1;

    // Explicit bind-group layouts — one entry per `@group(N)` the shader
    // declares bindings for. `bindGroupLayouts[g]` is the layout used when
    // a BindGroup is bound at group index `g` via
    // `RenderPass::setBindGroup(g, ...)`. NULL entries are allowed for
    // groups the shader doesn't use.
    //
    // Required (no auto-derive). Mismatch with the shader's reflected
    // binding map causes `makePipeline` to set lastError + return null.
    BindGroupLayout* const* bindGroupLayouts = nullptr;
    uint32_t bindGroupLayoutCount = 0;

    const char* label = nullptr;
};

struct ClearColor
{
    float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;
};

struct ColorAttachment
{
    TextureView* view = nullptr;
    TextureView* resolveTarget = nullptr;
    LoadOp loadOp = LoadOp::clear;
    StoreOp storeOp = StoreOp::store;
    ClearColor clearColor;
};

struct DepthStencilAttachment
{
    TextureView* view = nullptr;
    LoadOp depthLoadOp = LoadOp::clear;
    StoreOp depthStoreOp = StoreOp::store;
    float depthClearValue = 1.0f;
    LoadOp stencilLoadOp = LoadOp::clear;
    StoreOp stencilStoreOp = StoreOp::discard;
    uint32_t stencilClearValue = 0;
};

struct RenderPassDesc
{
    ColorAttachment colorAttachments[4] = {};
    uint32_t colorCount = 1;
    DepthStencilAttachment depthStencil;
    const char* label = nullptr;
};

// ============================================================================
// BindGroup Descriptor
// ============================================================================

struct BindGroupDesc
{
    // The layout the BindGroup conforms to. The layout's `groupIndex`
    // determines which `setBindGroup(g, ...)` slot this BindGroup is
    // bindable at. Resource kinds and per-slot expectations come from the
    // layout's entries.
    BindGroupLayout* layout = nullptr;

    struct UBOEntry
    {
        uint32_t slot = 0; // WGSL @binding within the layout's group.
        Buffer* buffer = nullptr;
        uint32_t offset = 0;
        uint32_t size = 0;
    };
    struct TexEntry
    {
        uint32_t slot = 0;
        TextureView* view = nullptr;
    };
    struct SampEntry
    {
        uint32_t slot = 0;
        Sampler* sampler = nullptr;
    };

    const UBOEntry* ubos = nullptr;
    uint32_t uboCount = 0;
    const TexEntry* textures = nullptr;
    uint32_t textureCount = 0;
    const SampEntry* samplers = nullptr;
    uint32_t samplerCount = 0;
    const char* label = nullptr;
};

// ============================================================================
// Features — runtime capability query
// ============================================================================

struct Features
{
    bool colorBufferFloat = false;
    bool perTargetBlend = false;
    bool perTargetWriteMask = false;
    bool textureViewSampling = false;
    bool drawBaseInstance = false;
    bool depthBiasClamp = false;
    bool anisotropicFiltering = false;
    bool texture3D = false;
    bool textureArrays = false;
    bool computeShaders = false;
    bool storageBuffers = false;

    bool bc = false;
    bool etc2 = false;
    bool astc = false;

    uint32_t maxColorAttachments = 4;
    uint32_t maxTextureSize2D = 4096;
    uint32_t maxTextureSizeCube = 4096;
    uint32_t maxTextureSize3D = 256;
    uint32_t maxUniformBufferSize = 16384;
    uint32_t maxVertexAttributes = 16;
    uint32_t maxSamplers = 16;
    // Maximum MSAA sample count supported for color render targets.
    // Scripts should query this before creating MSAA textures; values are
    // always a power-of-two (1, 2, 4, 8). Conservative default: 4.
    uint32_t maxSamples = 4;
};

} // namespace rive::ore
