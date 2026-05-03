/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/ore/ore_context.hpp"
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_texture.hpp"
#include "rive/renderer/ore/ore_sampler.hpp"
#include "rive/renderer/ore/ore_shader_module.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/renderer/ore/ore_render_pass.hpp"
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/webgpu/render_context_webgpu_impl.hpp"
#include "rive/rive_types.hpp"
#include "ore_wgpu_layout.hpp"

#include <array>
#include <cstring>
#include <string>

namespace rive::ore
{

// ============================================================================
// Enum → WebGPU conversion helpers
// ============================================================================

static wgpu::TextureFormat oreFormatToWGPU(TextureFormat fmt)
{
    switch (fmt)
    {
        case TextureFormat::r8unorm:
            return wgpu::TextureFormat::R8Unorm;
        case TextureFormat::rg8unorm:
            return wgpu::TextureFormat::RG8Unorm;
        case TextureFormat::rgba8unorm:
            return wgpu::TextureFormat::RGBA8Unorm;
        case TextureFormat::rgba8snorm:
            return wgpu::TextureFormat::RGBA8Snorm;
        case TextureFormat::bgra8unorm:
            return wgpu::TextureFormat::BGRA8Unorm;
        case TextureFormat::rgba16float:
            return wgpu::TextureFormat::RGBA16Float;
        case TextureFormat::rg16float:
            return wgpu::TextureFormat::RG16Float;
        case TextureFormat::r16float:
            return wgpu::TextureFormat::R16Float;
        case TextureFormat::rgba32float:
            return wgpu::TextureFormat::RGBA32Float;
        case TextureFormat::rg32float:
            return wgpu::TextureFormat::RG32Float;
        case TextureFormat::r32float:
            return wgpu::TextureFormat::R32Float;
        case TextureFormat::rgb10a2unorm:
            return wgpu::TextureFormat::RGB10A2Unorm;
        case TextureFormat::r11g11b10float:
            return wgpu::TextureFormat::RG11B10Ufloat;
        case TextureFormat::depth16unorm:
            return wgpu::TextureFormat::Depth16Unorm;
        case TextureFormat::depth24plusStencil8:
            return wgpu::TextureFormat::Depth24PlusStencil8;
        case TextureFormat::depth32float:
            return wgpu::TextureFormat::Depth32Float;
        case TextureFormat::depth32floatStencil8:
            return wgpu::TextureFormat::Depth32FloatStencil8;
        case TextureFormat::bc1unorm:
            return wgpu::TextureFormat::BC1RGBAUnorm;
        case TextureFormat::bc3unorm:
            return wgpu::TextureFormat::BC3RGBAUnorm;
        case TextureFormat::bc7unorm:
            return wgpu::TextureFormat::BC7RGBAUnorm;
        case TextureFormat::etc2rgb8:
            return wgpu::TextureFormat::ETC2RGB8Unorm;
        case TextureFormat::etc2rgba8:
            return wgpu::TextureFormat::ETC2RGBA8Unorm;
        case TextureFormat::astc4x4:
            return wgpu::TextureFormat::ASTC4x4Unorm;
        case TextureFormat::astc6x6:
            return wgpu::TextureFormat::ASTC6x6Unorm;
        case TextureFormat::astc8x8:
            return wgpu::TextureFormat::ASTC8x8Unorm;
    }
    RIVE_UNREACHABLE();
}

static wgpu::TextureDimension oreTypeToWGPUDimension(TextureType type)
{
    switch (type)
    {
        case TextureType::texture2D:
        case TextureType::cube:
        case TextureType::array2D:
            return wgpu::TextureDimension::e2D;
        case TextureType::texture3D:
            return wgpu::TextureDimension::e3D;
    }
    RIVE_UNREACHABLE();
}

static wgpu::TextureViewDimension oreViewDimToWGPU(TextureViewDimension dim)
{
    switch (dim)
    {
        case TextureViewDimension::texture2D:
            return wgpu::TextureViewDimension::e2D;
        case TextureViewDimension::cube:
            return wgpu::TextureViewDimension::Cube;
        case TextureViewDimension::texture3D:
            return wgpu::TextureViewDimension::e3D;
        case TextureViewDimension::array2D:
            return wgpu::TextureViewDimension::e2DArray;
        case TextureViewDimension::cubeArray:
            return wgpu::TextureViewDimension::CubeArray;
    }
    RIVE_UNREACHABLE();
}

static wgpu::TextureAspect oreAspectToWGPU(TextureAspect aspect)
{
    switch (aspect)
    {
        case TextureAspect::all:
            return wgpu::TextureAspect::All;
        case TextureAspect::depthOnly:
            return wgpu::TextureAspect::DepthOnly;
        case TextureAspect::stencilOnly:
            return wgpu::TextureAspect::StencilOnly;
    }
    RIVE_UNREACHABLE();
}

// Aspect-restricted views of combined depth/stencil textures: leave
// view.format Undefined so the impl auto-derives the per-aspect format
// (spec §6.2.1, "resolving GPUTextureViewDescriptor defaults"). Matches
// Dawn's e2e tests (DepthStencilSamplingTests,
// ReadOnlyDepthStencilAttachmentTests), which only set `aspect`. The
// explicit form (e.g. Depth24Plus view of D24S8) is also spec-valid and
// Dawn accepts it — Undefined is purely for following the most-exercised
// upstream call shape.
static wgpu::TextureFormat oreViewFormatForAspect(TextureFormat texFormat,
                                                  TextureAspect aspect)
{
    if (aspect == TextureAspect::depthOnly)
    {
        if (texFormat == TextureFormat::depth24plusStencil8 ||
            texFormat == TextureFormat::depth32floatStencil8)
            return wgpu::TextureFormat::Undefined;
    }
    else if (aspect == TextureAspect::stencilOnly)
    {
        return wgpu::TextureFormat::Undefined;
    }
    return oreFormatToWGPU(texFormat);
}

static wgpu::FilterMode oreFilterToWGPU(Filter f)
{
    return (f == Filter::linear) ? wgpu::FilterMode::Linear
                                 : wgpu::FilterMode::Nearest;
}

static wgpu::MipmapFilterMode oreMipmapFilterToWGPU(Filter f)
{
    return (f == Filter::linear) ? wgpu::MipmapFilterMode::Linear
                                 : wgpu::MipmapFilterMode::Nearest;
}

static wgpu::AddressMode oreWrapToWGPU(WrapMode mode)
{
    switch (mode)
    {
        case WrapMode::repeat:
            return wgpu::AddressMode::Repeat;
        case WrapMode::mirrorRepeat:
            return wgpu::AddressMode::MirrorRepeat;
        case WrapMode::clampToEdge:
            return wgpu::AddressMode::ClampToEdge;
    }
    RIVE_UNREACHABLE();
}

static wgpu::CompareFunction oreCompareFunctionToWGPU(CompareFunction fn)
{
    switch (fn)
    {
        case CompareFunction::none:
            return wgpu::CompareFunction::Undefined;
        case CompareFunction::never:
            return wgpu::CompareFunction::Never;
        case CompareFunction::less:
            return wgpu::CompareFunction::Less;
        case CompareFunction::equal:
            return wgpu::CompareFunction::Equal;
        case CompareFunction::lessEqual:
            return wgpu::CompareFunction::LessEqual;
        case CompareFunction::greater:
            return wgpu::CompareFunction::Greater;
        case CompareFunction::notEqual:
            return wgpu::CompareFunction::NotEqual;
        case CompareFunction::greaterEqual:
            return wgpu::CompareFunction::GreaterEqual;
        case CompareFunction::always:
            return wgpu::CompareFunction::Always;
    }
    RIVE_UNREACHABLE();
}

static wgpu::PrimitiveTopology oreTopologyToWGPU(PrimitiveTopology topo)
{
    switch (topo)
    {
        case PrimitiveTopology::pointList:
            return wgpu::PrimitiveTopology::PointList;
        case PrimitiveTopology::lineList:
            return wgpu::PrimitiveTopology::LineList;
        case PrimitiveTopology::lineStrip:
            return wgpu::PrimitiveTopology::LineStrip;
        case PrimitiveTopology::triangleList:
            return wgpu::PrimitiveTopology::TriangleList;
        case PrimitiveTopology::triangleStrip:
            return wgpu::PrimitiveTopology::TriangleStrip;
    }
    RIVE_UNREACHABLE();
}

static wgpu::IndexFormat oreIndexFormatToWGPU(IndexFormat fmt)
{
    switch (fmt)
    {
        case IndexFormat::none:
        case IndexFormat::uint16:
            return wgpu::IndexFormat::Uint16;
        case IndexFormat::uint32:
            return wgpu::IndexFormat::Uint32;
    }
    RIVE_UNREACHABLE();
}

static wgpu::CullMode oreCullModeToWGPU(CullMode mode)
{
    switch (mode)
    {
        case CullMode::none:
            return wgpu::CullMode::None;
        case CullMode::front:
            return wgpu::CullMode::Front;
        case CullMode::back:
            return wgpu::CullMode::Back;
    }
    RIVE_UNREACHABLE();
}

static wgpu::FrontFace oreWindingToWGPU(FaceWinding w)
{
    return (w == FaceWinding::counterClockwise) ? wgpu::FrontFace::CCW
                                                : wgpu::FrontFace::CW;
}

static wgpu::BlendFactor oreBlendFactorToWGPU(BlendFactor f)
{
    switch (f)
    {
        case BlendFactor::zero:
            return wgpu::BlendFactor::Zero;
        case BlendFactor::one:
            return wgpu::BlendFactor::One;
        case BlendFactor::srcColor:
            return wgpu::BlendFactor::Src;
        case BlendFactor::oneMinusSrcColor:
            return wgpu::BlendFactor::OneMinusSrc;
        case BlendFactor::srcAlpha:
            return wgpu::BlendFactor::SrcAlpha;
        case BlendFactor::oneMinusSrcAlpha:
            return wgpu::BlendFactor::OneMinusSrcAlpha;
        case BlendFactor::dstColor:
            return wgpu::BlendFactor::Dst;
        case BlendFactor::oneMinusDstColor:
            return wgpu::BlendFactor::OneMinusDst;
        case BlendFactor::dstAlpha:
            return wgpu::BlendFactor::DstAlpha;
        case BlendFactor::oneMinusDstAlpha:
            return wgpu::BlendFactor::OneMinusDstAlpha;
        case BlendFactor::srcAlphaSaturated:
            return wgpu::BlendFactor::SrcAlphaSaturated;
        case BlendFactor::blendColor:
            return wgpu::BlendFactor::Constant;
        case BlendFactor::oneMinusBlendColor:
            return wgpu::BlendFactor::OneMinusConstant;
    }
    RIVE_UNREACHABLE();
}

static wgpu::BlendOperation oreBlendOpToWGPU(BlendOp op)
{
    switch (op)
    {
        case BlendOp::add:
            return wgpu::BlendOperation::Add;
        case BlendOp::subtract:
            return wgpu::BlendOperation::Subtract;
        case BlendOp::reverseSubtract:
            return wgpu::BlendOperation::ReverseSubtract;
        case BlendOp::min:
            return wgpu::BlendOperation::Min;
        case BlendOp::max:
            return wgpu::BlendOperation::Max;
    }
    RIVE_UNREACHABLE();
}

static wgpu::StencilOperation oreStencilOpToWGPU(StencilOp op)
{
    switch (op)
    {
        case StencilOp::keep:
            return wgpu::StencilOperation::Keep;
        case StencilOp::zero:
            return wgpu::StencilOperation::Zero;
        case StencilOp::replace:
            return wgpu::StencilOperation::Replace;
        case StencilOp::incrementClamp:
            return wgpu::StencilOperation::IncrementClamp;
        case StencilOp::decrementClamp:
            return wgpu::StencilOperation::DecrementClamp;
        case StencilOp::invert:
            return wgpu::StencilOperation::Invert;
        case StencilOp::incrementWrap:
            return wgpu::StencilOperation::IncrementWrap;
        case StencilOp::decrementWrap:
            return wgpu::StencilOperation::DecrementWrap;
    }
    RIVE_UNREACHABLE();
}

static wgpu::ColorWriteMask oreColorWriteMaskToWGPU(ColorWriteMask mask)
{
    wgpu::ColorWriteMask result = wgpu::ColorWriteMask::None;
    if ((mask & ColorWriteMask::red) != ColorWriteMask::none)
        result |= wgpu::ColorWriteMask::Red;
    if ((mask & ColorWriteMask::green) != ColorWriteMask::none)
        result |= wgpu::ColorWriteMask::Green;
    if ((mask & ColorWriteMask::blue) != ColorWriteMask::none)
        result |= wgpu::ColorWriteMask::Blue;
    if ((mask & ColorWriteMask::alpha) != ColorWriteMask::none)
        result |= wgpu::ColorWriteMask::Alpha;
    return result;
}

static wgpu::VertexFormat oreVertexFormatToWGPU(VertexFormat fmt)
{
    switch (fmt)
    {
        case VertexFormat::float1:
            return wgpu::VertexFormat::Float32;
        case VertexFormat::float2:
            return wgpu::VertexFormat::Float32x2;
        case VertexFormat::float3:
            return wgpu::VertexFormat::Float32x3;
        case VertexFormat::float4:
            return wgpu::VertexFormat::Float32x4;
        case VertexFormat::uint8x4:
            return wgpu::VertexFormat::Uint8x4;
        case VertexFormat::sint8x4:
            return wgpu::VertexFormat::Sint8x4;
        case VertexFormat::unorm8x4:
            return wgpu::VertexFormat::Unorm8x4;
        case VertexFormat::snorm8x4:
            return wgpu::VertexFormat::Snorm8x4;
        case VertexFormat::uint16x2:
            return wgpu::VertexFormat::Uint16x2;
        case VertexFormat::sint16x2:
            return wgpu::VertexFormat::Sint16x2;
        case VertexFormat::unorm16x2:
            return wgpu::VertexFormat::Unorm16x2;
        case VertexFormat::snorm16x2:
            return wgpu::VertexFormat::Snorm16x2;
        case VertexFormat::uint16x4:
            return wgpu::VertexFormat::Uint16x4;
        case VertexFormat::sint16x4:
            return wgpu::VertexFormat::Sint16x4;
        case VertexFormat::float16x2:
            return wgpu::VertexFormat::Float16x2;
        case VertexFormat::float16x4:
            return wgpu::VertexFormat::Float16x4;
        case VertexFormat::uint32:
            return wgpu::VertexFormat::Uint32;
        case VertexFormat::uint32x2:
            return wgpu::VertexFormat::Uint32x2;
        case VertexFormat::uint32x3:
            return wgpu::VertexFormat::Uint32x3;
        case VertexFormat::uint32x4:
            return wgpu::VertexFormat::Uint32x4;
        case VertexFormat::sint32:
            return wgpu::VertexFormat::Sint32;
        case VertexFormat::sint32x2:
            return wgpu::VertexFormat::Sint32x2;
        case VertexFormat::sint32x3:
            return wgpu::VertexFormat::Sint32x3;
        case VertexFormat::sint32x4:
            return wgpu::VertexFormat::Sint32x4;
    }
    RIVE_UNREACHABLE();
}

static wgpu::VertexStepMode oreStepModeToWGPU(VertexStepMode mode)
{
    return (mode == VertexStepMode::instance) ? wgpu::VertexStepMode::Instance
                                              : wgpu::VertexStepMode::Vertex;
}

// ============================================================================
// Shader compilation helpers
// ============================================================================

#ifdef RIVE_DAWN
static wgpu::ShaderModule compileDawnWGSLShader(wgpu::Device device,
                                                const char* source,
                                                uint32_t codeSize)
{
    WGPUShaderSourceWGSL wgslDesc = WGPU_SHADER_SOURCE_WGSL_INIT;
    wgslDesc.code.data = source;
    wgslDesc.code.length = codeSize > 0 ? codeSize : WGPU_STRLEN;

    WGPUShaderModuleDescriptor descriptor = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
    descriptor.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wgslDesc);

    return wgpu::ShaderModule::Acquire(
        wgpuDeviceCreateShaderModule(device.Get(), &descriptor));
}
#else
static wgpu::ShaderModule compileWagyuShader(wgpu::Device device,
                                             const char* source,
                                             uint32_t codeSize,
                                             WGPUWagyuShaderLanguage language)
{
    WGPUWagyuShaderModuleDescriptor wagyuDesc =
        WGPU_WAGYU_SHADER_MODULE_DESCRIPTOR_INIT;
    wagyuDesc.code = source;
    wagyuDesc.codeSize = codeSize > 0 ? codeSize : strlen(source);
    wagyuDesc.language = language;

    WGPUShaderModuleDescriptor descriptor = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
    descriptor.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wagyuDesc);

    return wgpu::ShaderModule::Acquire(
        wgpuDeviceCreateShaderModule(device.Get(), &descriptor));
}
#endif

// ============================================================================
// Context lifecycle
// ============================================================================

Context::Context() {}

Context::~Context() {}

Context::Context(Context&& other) noexcept :
    m_features(other.m_features),
    m_wgpuBackend(other.m_wgpuBackend),
    m_wgpuDevice(std::move(other.m_wgpuDevice)),
    m_wgpuQueue(std::move(other.m_wgpuQueue)),
    m_wgpuCommandEncoder(std::move(other.m_wgpuCommandEncoder)),
    m_wgpuExternalEncoder(other.m_wgpuExternalEncoder)
{
    other.m_wgpuExternalEncoder = false;
}

Context& Context::operator=(Context&& other) noexcept
{
    if (this != &other)
    {
        m_features = other.m_features;
        m_wgpuBackend = other.m_wgpuBackend;
        m_wgpuDevice = std::move(other.m_wgpuDevice);
        m_wgpuQueue = std::move(other.m_wgpuQueue);
        m_wgpuCommandEncoder = std::move(other.m_wgpuCommandEncoder);
        m_wgpuExternalEncoder = other.m_wgpuExternalEncoder;
        other.m_wgpuExternalEncoder = false;
    }
    return *this;
}

Context Context::createWGPU(wgpu::Device device,
                            wgpu::Queue queue,
                            wgpu::BackendType backendType)
{
    Context ctx;
    ctx.m_wgpuDevice = std::move(device);
    ctx.m_wgpuQueue = std::move(queue);
    ctx.m_wgpuBackend = (backendType == wgpu::BackendType::OpenGLES)
                            ? WGPUBackend::OpenGLES
                            : WGPUBackend::Vulkan;

    // Populate features with WebGPU-level capabilities.
    Features& f = ctx.m_features;
    f.colorBufferFloat = true;
    f.perTargetBlend = true;
    f.perTargetWriteMask = true;
    f.textureViewSampling = true;
    f.drawBaseInstance = true;
    f.depthBiasClamp = true;
    f.anisotropicFiltering =
        false; // Requires wgpu::FeatureName::TextureCompressionBC
    f.texture3D = true;
    f.textureArrays = true;
    f.computeShaders = true;
    f.storageBuffers = true;
    f.bc = false;
    f.etc2 = true;
    f.astc = false;
    f.maxColorAttachments = 4;
    f.maxTextureSize2D = 8192;
    f.maxTextureSizeCube = 8192;
    f.maxTextureSize3D = 2048;
    f.maxUniformBufferSize = 65536;
    f.maxVertexAttributes = 16;
    f.maxSamplers = 16;

    return ctx;
}

void Context::beginFrame()
{
    // Release deferred BindGroups from last frame. By beginFrame() the
    // caller has waited for the previous frame's GPU work to complete.
    m_deferredBindGroups.clear();
    m_wgpuExternalEncoder = false;

    wgpu::CommandEncoderDescriptor encoderDesc{};
    m_wgpuCommandEncoder = m_wgpuDevice.CreateCommandEncoder(&encoderDesc);
}

void Context::beginFrame(wgpu::CommandEncoder externalEncoder)
{
    assert(externalEncoder != nullptr);
    // Same drain contract as owned-encoder mode: by the time we're called
    // again for a new frame the host must have submitted/awaited the prior
    // frame's work, so any BindGroups deferred at that point are GPU-idle.
    m_deferredBindGroups.clear();
    m_wgpuCommandEncoder = std::move(externalEncoder);
    m_wgpuExternalEncoder = true;
}

void Context::waitForGPU() {} // WGPU submit is synchronous for Dawn.

void Context::endFrame()
{
    assert(m_wgpuCommandEncoder != nullptr);
    if (m_wgpuExternalEncoder)
    {
        // Host owns Finish()/Submit() and the frame fence; just drop our
        // reference to the shared encoder.
        m_wgpuCommandEncoder = nullptr;
        m_wgpuExternalEncoder = false;
        return;
    }
    wgpu::CommandBuffer commands = m_wgpuCommandEncoder.Finish();
    m_wgpuQueue.Submit(1, &commands);
    m_wgpuCommandEncoder = nullptr;
}

// ============================================================================
// makeBuffer
// ============================================================================

rcp<Buffer> Context::makeBuffer(const BufferDesc& desc)
{
    auto buffer = rcp<Buffer>(new Buffer(desc.size, desc.usage));
    buffer->m_wgpuQueue = m_wgpuQueue; // addref'd copy for WriteBuffer

    wgpu::BufferUsage usage = wgpu::BufferUsage::CopyDst;
    switch (desc.usage)
    {
        case BufferUsage::vertex:
            usage |= wgpu::BufferUsage::Vertex;
            break;
        case BufferUsage::index:
            usage |= wgpu::BufferUsage::Index;
            break;
        case BufferUsage::uniform:
            usage |= wgpu::BufferUsage::Uniform;
            break;
    }

    wgpu::BufferDescriptor wDesc{};
    wDesc.label = desc.label;
    wDesc.size = desc.size;
    wDesc.usage = usage;
    wDesc.mappedAtCreation = false;

    buffer->m_wgpuBuffer = m_wgpuDevice.CreateBuffer(&wDesc);

    if (desc.data != nullptr)
    {
        m_wgpuQueue.WriteBuffer(buffer->m_wgpuBuffer, 0, desc.data, desc.size);
    }

    return buffer;
}

// ============================================================================
// makeTexture
// ============================================================================

rcp<Texture> Context::makeTexture(const TextureDesc& desc)
{
    auto texture = rcp<Texture>(new Texture(desc));
    texture->m_wgpuQueue = m_wgpuQueue;

    wgpu::TextureUsage usage =
        wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
    if (desc.renderTarget)
        usage |= wgpu::TextureUsage::RenderAttachment;

    wgpu::TextureDescriptor wDesc{};
    wDesc.label = desc.label;
    wDesc.usage = usage;
    wDesc.dimension = oreTypeToWGPUDimension(desc.type);
    wDesc.size = {desc.width, desc.height, desc.depthOrArrayLayers};
    wDesc.format = oreFormatToWGPU(desc.format);
    wDesc.mipLevelCount = desc.numMipmaps;
    wDesc.sampleCount = desc.sampleCount;

    texture->m_wgpuTexture = m_wgpuDevice.CreateTexture(&wDesc);

    return texture;
}

// ============================================================================
// makeTextureView
// ============================================================================

rcp<TextureView> Context::makeTextureView(const TextureViewDesc& desc)
{
    Texture* tex = desc.texture;
    if (!tex)
        return nullptr;

    auto view = rcp<TextureView>(new TextureView(ref_rcp(tex), desc));

    wgpu::TextureViewDescriptor wDesc{};
    wDesc.dimension = oreViewDimToWGPU(desc.dimension);
    wDesc.aspect = oreAspectToWGPU(desc.aspect);
    wDesc.baseMipLevel = desc.baseMipLevel;
    wDesc.mipLevelCount = desc.mipCount;
    wDesc.baseArrayLayer = desc.baseLayer;
    wDesc.arrayLayerCount = desc.layerCount;
    wDesc.format = oreViewFormatForAspect(tex->format(), desc.aspect);

    view->m_wgpuTextureView = tex->m_wgpuTexture.CreateView(&wDesc);

    return view;
}

// ============================================================================
// makeSampler
// ============================================================================

rcp<Sampler> Context::makeSampler(const SamplerDesc& desc)
{
    auto sampler = rcp<Sampler>(new Sampler());

    wgpu::SamplerDescriptor wDesc{};
    wDesc.label = desc.label;
    wDesc.addressModeU = oreWrapToWGPU(desc.wrapU);
    wDesc.addressModeV = oreWrapToWGPU(desc.wrapV);
    wDesc.addressModeW = oreWrapToWGPU(desc.wrapW);
    wDesc.magFilter = oreFilterToWGPU(desc.magFilter);
    wDesc.minFilter = oreFilterToWGPU(desc.minFilter);
    wDesc.mipmapFilter = oreMipmapFilterToWGPU(desc.mipmapFilter);
    wDesc.lodMinClamp = desc.minLod;
    wDesc.lodMaxClamp = desc.maxLod;
    wDesc.compare = oreCompareFunctionToWGPU(desc.compare);
    // Dawn rejects `maxAnisotropy = 0`. The Lua surface defaults to 1
    // (Phase 2B) but a C++ caller could pass through any default-init'd
    // SamplerDesc, so floor-clamp here as defense-in-depth.
    wDesc.maxAnisotropy =
        static_cast<uint16_t>(desc.maxAnisotropy < 1 ? 1 : desc.maxAnisotropy);

    sampler->m_wgpuSampler = m_wgpuDevice.CreateSampler(&wDesc);

    return sampler;
}

// ============================================================================
// makeShaderModule
// ============================================================================

rcp<ShaderModule> Context::makeShaderModule(const ShaderModuleDesc& desc)
{
    auto module = rcp<ShaderModule>(new ShaderModule());

    const char* source = static_cast<const char*>(desc.code);
    uint32_t codeSize = desc.codeSize > 0
                            ? desc.codeSize
                            : static_cast<uint32_t>(strlen(source));

#ifdef RIVE_DAWN
    assert(desc.language == ShaderLanguage::wgsl &&
           "Dawn ore backend only supports WGSL shaders");
    module->m_wgpuShaderModule =
        compileDawnWGSLShader(m_wgpuDevice, source, codeSize);
    assert(module->m_wgpuShaderModule != nullptr &&
           "Ore Dawn WGSL shader compilation failed");
#else
    WGPUWagyuShaderLanguage language;
    if (desc.language == ShaderLanguage::wgsl)
    {
        // WGSL works on all wagyu backends without any pragma injection.
        language = WGPUWagyuShaderLanguage_WGSL;
    }
    else
    {
        // GLSL: GLES uses GLSLRAW (no glslang), Vulkan uses GLSL (glslang).
        language = (m_wgpuBackend == WGPUBackend::OpenGLES)
                       ? WGPUWagyuShaderLanguage_GLSLRAW
                       : WGPUWagyuShaderLanguage_GLSL;
    }

    module->m_wgpuShaderModule =
        compileWagyuShader(m_wgpuDevice, source, codeSize, language);

    assert(module->m_wgpuShaderModule != nullptr &&
           "Ore WGPU wagyu shader compilation failed");
#endif

    module->applyBindingMapFromDesc(desc);
    return module;
}

// ============================================================================
// makePipeline
// ============================================================================

rcp<Pipeline> Context::makePipeline(const PipelineDesc& desc,
                                    std::string* outError)
{
    auto pipeline = rcp<Pipeline>(new Pipeline(desc));

    // --- Vertex state ---
    // Flatten attribute arrays per buffer slot.
    constexpr uint32_t kMaxBuffers = 8;
    constexpr uint32_t kMaxAttribs = 32;
    wgpu::VertexAttribute wAttribs[kMaxAttribs];
    wgpu::VertexBufferLayout wBuffers[kMaxBuffers];
    uint32_t attrIdx = 0;

    for (uint32_t b = 0; b < desc.vertexBufferCount; ++b)
    {
        const auto& layout = desc.vertexBuffers[b];
        wgpu::VertexBufferLayout& wLayout = wBuffers[b];
        wLayout.arrayStride = layout.stride;
        wLayout.stepMode = oreStepModeToWGPU(layout.stepMode);
        wLayout.attributeCount = layout.attributeCount;
        wLayout.attributes = &wAttribs[attrIdx];

        for (uint32_t a = 0; a < layout.attributeCount; ++a)
        {
            const auto& attr = layout.attributes[a];
            // Use explicit field assignment — wgpu::VertexAttribute has
            // nextInChain as its first field, so positional aggregate init
            // would map format→nextInChain (bad pointer) and corrupt the
            // vertex layout.
            wgpu::VertexAttribute& wa = wAttribs[attrIdx++];
            wa = {};
            wa.format = oreVertexFormatToWGPU(attr.format);
            wa.offset = attr.offset;
            wa.shaderLocation = attr.shaderSlot;
        }
    }

    wgpu::VertexState vertexState{};
    vertexState.module = desc.vertexModule->m_wgpuShaderModule;
    vertexState.entryPoint = desc.vertexEntryPoint;
    vertexState.bufferCount = desc.vertexBufferCount;
    vertexState.buffers = wBuffers;

    // --- Primitive state ---
    wgpu::PrimitiveState primitiveState{};
    primitiveState.topology = oreTopologyToWGPU(desc.topology);
    primitiveState.stripIndexFormat =
        (desc.topology == PrimitiveTopology::triangleStrip ||
         desc.topology == PrimitiveTopology::lineStrip)
            ? oreIndexFormatToWGPU(desc.indexFormat)
            : wgpu::IndexFormat::Undefined;
    primitiveState.frontFace = oreWindingToWGPU(desc.winding);
    primitiveState.cullMode = oreCullModeToWGPU(desc.cullMode);

    // --- Depth/stencil state ---
    //
    // The presence of a depth attachment is encoded purely by the
    // `depthStencil.format` sentinel: `rgba8unorm` means "no depth
    // attachment" (documented in `ore_types.hpp`), anything else means
    // there's a depth attachment to declare. Pre-fix this also gated on
    // `depthCompare != always || depthWriteEnabled`, which let a script
    // request a depth attachment for stencil-only operations and got
    // `pDepthStencil = nullptr` — Dawn then rejected the pipeline as
    // "depth-stencil format mismatch" against any pass with a depth
    // attachment. The format-only check is sufficient and matches Dawn's
    // own behavior: a pipeline with a depth-stencil-format declaration
    // must be paired with a pass that has the matching attachment, no
    // matter what compare / write the pipeline configures.
    wgpu::DepthStencilState depthStencilState{};
    const bool hasDepth =
        (desc.depthStencil.format != TextureFormat::rgba8unorm);

    wgpu::DepthStencilState* pDepthStencil = nullptr;
    if (hasDepth)
    {
        depthStencilState.format = oreFormatToWGPU(desc.depthStencil.format);
        depthStencilState.depthWriteEnabled =
            desc.depthStencil.depthWriteEnabled;
        depthStencilState.depthCompare =
            oreCompareFunctionToWGPU(desc.depthStencil.depthCompare);

        depthStencilState.stencilFront.compare =
            oreCompareFunctionToWGPU(desc.stencilFront.compare);
        depthStencilState.stencilFront.failOp =
            oreStencilOpToWGPU(desc.stencilFront.failOp);
        depthStencilState.stencilFront.depthFailOp =
            oreStencilOpToWGPU(desc.stencilFront.depthFailOp);
        depthStencilState.stencilFront.passOp =
            oreStencilOpToWGPU(desc.stencilFront.passOp);

        depthStencilState.stencilBack.compare =
            oreCompareFunctionToWGPU(desc.stencilBack.compare);
        depthStencilState.stencilBack.failOp =
            oreStencilOpToWGPU(desc.stencilBack.failOp);
        depthStencilState.stencilBack.depthFailOp =
            oreStencilOpToWGPU(desc.stencilBack.depthFailOp);
        depthStencilState.stencilBack.passOp =
            oreStencilOpToWGPU(desc.stencilBack.passOp);

        depthStencilState.stencilReadMask = desc.stencilReadMask;
        depthStencilState.stencilWriteMask = desc.stencilWriteMask;
        depthStencilState.depthBias = desc.depthStencil.depthBias;
        depthStencilState.depthBiasSlopeScale =
            desc.depthStencil.depthBiasSlopeScale;
        depthStencilState.depthBiasClamp = desc.depthStencil.depthBiasClamp;

        pDepthStencil = &depthStencilState;
    }

    // --- Multisample state ---
    wgpu::MultisampleState multisampleState{};
    multisampleState.count = desc.sampleCount;
    multisampleState.mask = 0xFFFFFFFF;

    // --- Fragment state + blend ---
    wgpu::BlendState blendStates[4];
    wgpu::ColorTargetState colorTargets[4];

    for (uint32_t i = 0; i < desc.colorCount; ++i)
    {
        const auto& ct = desc.colorTargets[i];
        wgpu::ColorTargetState& wct = colorTargets[i];
        wct.format = oreFormatToWGPU(ct.format);
        wct.writeMask = oreColorWriteMaskToWGPU(ct.writeMask);

        if (ct.blendEnabled)
        {
            blendStates[i] = {
                {oreBlendOpToWGPU(ct.blend.colorOp),
                 oreBlendFactorToWGPU(ct.blend.srcColor),
                 oreBlendFactorToWGPU(ct.blend.dstColor)},
                {oreBlendOpToWGPU(ct.blend.alphaOp),
                 oreBlendFactorToWGPU(ct.blend.srcAlpha),
                 oreBlendFactorToWGPU(ct.blend.dstAlpha)},
            };
            wct.blend = &blendStates[i];
        }
        else
        {
            wct.blend = nullptr;
        }
    }

    // Fragment state is omitted for depth-only pipelines.
    wgpu::FragmentState fragmentState{};
    if (desc.fragmentModule != nullptr)
    {
        fragmentState.module = desc.fragmentModule->m_wgpuShaderModule;
        fragmentState.entryPoint = desc.fragmentEntryPoint;
        fragmentState.targetCount = desc.colorCount;
        fragmentState.targets = colorTargets;
    }

    // --- Validate user-supplied layouts against shader binding map ---
    //
    // Phase E: explicit `bindGroupLayouts[]` is the source of truth. The
    // shader's reflected BindingMap drives the validation only — every
    // shader binding must be declared by the corresponding layout.
    {
        std::string err;
        if (!validateLayoutsAgainstBindingMap(pipeline->m_bindingMap,
                                              desc.bindGroupLayouts,
                                              desc.bindGroupLayoutCount,
                                              &err) ||
            !validateColorRequiresFragment(desc.colorCount,
                                           desc.fragmentModule != nullptr,
                                           &err))
        {
            if (outError)
                *outError = err;
            else
                setLastError("makePipeline: %s", err.c_str());
            return nullptr;
        }
    }

    // --- Assemble the pipeline layout from user-supplied BGLs ---
    wgpu::BindGroupLayout rawBGLs[kWGPUMaxGroups];
    for (uint32_t g = 0; g < desc.bindGroupLayoutCount && g < kWGPUMaxGroups;
         ++g)
    {
        if (desc.bindGroupLayouts[g] != nullptr)
            rawBGLs[g] = desc.bindGroupLayouts[g]->m_wgpuBGL;
    }
    wgpu::PipelineLayoutDescriptor plDesc{};
    plDesc.label = desc.label ? desc.label : "";
    plDesc.bindGroupLayoutCount = desc.bindGroupLayoutCount;
    plDesc.bindGroupLayouts = desc.bindGroupLayoutCount > 0 ? rawBGLs : nullptr;
    pipeline->m_wgpuPipelineLayout = m_wgpuDevice.CreatePipelineLayout(&plDesc);

    // --- Assemble the render pipeline ---
    wgpu::RenderPipelineDescriptor rpDesc{};
    rpDesc.label = desc.label;
    rpDesc.layout = pipeline->m_wgpuPipelineLayout;
    rpDesc.vertex = vertexState;
    rpDesc.primitive = primitiveState;
    rpDesc.depthStencil = pDepthStencil;
    rpDesc.multisample = multisampleState;
    rpDesc.fragment =
        (desc.fragmentModule != nullptr) ? &fragmentState : nullptr;

    pipeline->m_wgpuDevice = m_wgpuDevice;
    pipeline->m_wgpuPipeline = m_wgpuDevice.CreateRenderPipeline(&rpDesc);
    if (!pipeline->m_wgpuPipeline)
    {
        if (outError)
            *outError = "CreateRenderPipeline returned null";
        return nullptr;
    }

    return pipeline;
}

// ============================================================================
// makeBindGroupLayout
// ============================================================================

rcp<BindGroupLayout> Context::makeBindGroupLayout(
    const BindGroupLayoutDesc& desc)
{
    if (desc.groupIndex >= kMaxBindGroups)
    {
        setLastError("makeBindGroupLayout: groupIndex %u out of range [0, %u)",
                     desc.groupIndex,
                     kMaxBindGroups);
        return nullptr;
    }
    auto layout = rcp<BindGroupLayout>(new BindGroupLayout());
    layout->m_context = this;
    layout->m_groupIndex = desc.groupIndex;
    layout->m_entries.reserve(desc.entryCount);
    for (uint32_t i = 0; i < desc.entryCount; ++i)
        layout->m_entries.push_back(desc.entries[i]);

    layout->m_wgpuBGL = buildWGPUBindGroupLayoutFromDesc(m_wgpuDevice, desc);
    if (!layout->m_wgpuBGL)
    {
        setLastError("makeBindGroupLayout: CreateBindGroupLayout returned "
                     "null (group=%u)",
                     desc.groupIndex);
        return nullptr;
    }
    return layout;
}

// ============================================================================
// makeBindGroup
// ============================================================================

rcp<BindGroup> Context::makeBindGroup(const BindGroupDesc& desc)
{
    if (desc.layout == nullptr)
    {
        setLastError("makeBindGroup: BindGroupDesc::layout is null");
        return nullptr;
    }
    BindGroupLayout* layout = desc.layout;
    if (layout->groupIndex() >= kMaxBindGroups)
    {
        setLastError("makeBindGroup: layout->groupIndex %u out of range [0, "
                     "%u)",
                     layout->groupIndex(),
                     kMaxBindGroups);
        return nullptr;
    }

    auto bg = rcp<BindGroup>(new BindGroup());
    bg->m_context = this;
    bg->m_layoutRef = ref_rcp(layout);

    // Count dynamic-offset UBOs declared by the layout. Authoritative —
    // dynamic-ness is a layout property (WebGPU/Vulkan model).
    uint32_t dynamicCount = 0;
    for (const auto& e : layout->entries())
    {
        if (e.kind == BindingKind::uniformBuffer && e.hasDynamicOffset)
            ++dynamicCount;
    }
    bg->m_dynamicOffsetCount = dynamicCount;

    // WebGPU is natively per-group-namespaced — `wgpu::BindGroupEntry::binding`
    // is the WGSL `@binding` value directly. Validate against the layout's
    // declared entries; missing entries set lastError and skip.
    auto checkLayout = [&](uint32_t binding, BindingKind expected) -> bool {
        const BindGroupLayoutEntry* le = layout->findEntry(binding);
        if (le == nullptr)
        {
            setLastError("makeBindGroup: (group=%u, binding=%u) not declared "
                         "in BindGroupLayout",
                         layout->groupIndex(),
                         binding);
            return false;
        }
        // Sampler/comparison-sampler are interchangeable on the bind side.
        if (le->kind != expected &&
            !((le->kind == BindingKind::sampler ||
               le->kind == BindingKind::comparisonSampler) &&
              (expected == BindingKind::sampler ||
               expected == BindingKind::comparisonSampler)))
        {
            setLastError("makeBindGroup: (group=%u, binding=%u) layout kind "
                         "mismatch",
                         layout->groupIndex(),
                         binding);
            return false;
        }
        return true;
    };

    uint32_t totalCapacity =
        desc.uboCount + desc.textureCount + desc.samplerCount;
    std::vector<wgpu::BindGroupEntry> entries(totalCapacity);
    uint32_t idx = 0;

    for (uint32_t i = 0; i < desc.uboCount; ++i)
    {
        const auto& ubo = desc.ubos[i];
        if (!checkLayout(ubo.slot, BindingKind::uniformBuffer))
            continue;
        wgpu::BindGroupEntry& e = entries[idx++];
        e = {};
        e.binding = ubo.slot;
        e.buffer = ubo.buffer->m_wgpuBuffer;
        e.offset = ubo.offset;
        e.size = (ubo.size > 0) ? ubo.size : ubo.buffer->size();
        bg->m_retainedBuffers.push_back(ref_rcp(ubo.buffer));
    }

    for (uint32_t i = 0; i < desc.textureCount; ++i)
    {
        const auto& tex = desc.textures[i];
        if (!checkLayout(tex.slot, BindingKind::sampledTexture))
            continue;
        wgpu::BindGroupEntry& e = entries[idx++];
        e = {};
        e.binding = tex.slot;
        e.textureView = tex.view->m_wgpuTextureView;
        bg->m_retainedViews.push_back(ref_rcp(tex.view));
    }

    for (uint32_t i = 0; i < desc.samplerCount; ++i)
    {
        const auto& samp = desc.samplers[i];
        if (!checkLayout(samp.slot, BindingKind::sampler))
            continue;
        wgpu::BindGroupEntry& e = entries[idx++];
        e = {};
        e.binding = samp.slot;
        e.sampler = samp.sampler->m_wgpuSampler;
        bg->m_retainedSamplers.push_back(ref_rcp(samp.sampler));
    }

    wgpu::BindGroupDescriptor bgDesc{};
    bgDesc.label = desc.label;
    bgDesc.layout = layout->m_wgpuBGL;
    bgDesc.entryCount = idx;
    bgDesc.entries = (idx > 0) ? entries.data() : nullptr;

    bg->m_wgpuBindGroup = m_wgpuDevice.CreateBindGroup(&bgDesc);
    if (bg->m_wgpuBindGroup == nullptr)
        return nullptr;

    return bg;
}

// ============================================================================
// beginRenderPass
// ============================================================================

RenderPass Context::beginRenderPass(const RenderPassDesc& desc,
                                    std::string* outError)
{
    finishActiveRenderPass();

    assert(m_wgpuCommandEncoder != nullptr &&
           "beginFrame must be called before beginRenderPass");

    RenderPass pass;
    pass.m_context = this;
    pass.m_wgpuContext = this;
    pass.populateAttachmentMetadata(desc);

    // Color attachments.
    wgpu::RenderPassColorAttachment colorAttachments[4];
    for (uint32_t i = 0; i < desc.colorCount; ++i)
    {
        const auto& ca = desc.colorAttachments[i];
        wgpu::RenderPassColorAttachment& wca = colorAttachments[i];
        wca.view = ca.view ? ca.view->m_wgpuTextureView : nullptr;
        wca.resolveTarget =
            ca.resolveTarget ? ca.resolveTarget->m_wgpuTextureView : nullptr;
        wca.loadOp = (ca.loadOp == LoadOp::clear) ? wgpu::LoadOp::Clear
                                                  : wgpu::LoadOp::Load;
        wca.storeOp = (ca.storeOp == StoreOp::store) ? wgpu::StoreOp::Store
                                                     : wgpu::StoreOp::Discard;
        wca.clearValue = {ca.clearColor.r,
                          ca.clearColor.g,
                          ca.clearColor.b,
                          ca.clearColor.a};
    }

    // Depth/stencil attachment.
    wgpu::RenderPassDepthStencilAttachment depthStencilAttachment{};
    wgpu::RenderPassDepthStencilAttachment* pDepthStencil = nullptr;
    if (desc.depthStencil.view)
    {
        const auto& ds = desc.depthStencil;
        depthStencilAttachment.view = ds.view->m_wgpuTextureView;
        depthStencilAttachment.depthLoadOp = (ds.depthLoadOp == LoadOp::clear)
                                                 ? wgpu::LoadOp::Clear
                                                 : wgpu::LoadOp::Load;
        depthStencilAttachment.depthStoreOp =
            (ds.depthStoreOp == StoreOp::store) ? wgpu::StoreOp::Store
                                                : wgpu::StoreOp::Discard;
        depthStencilAttachment.depthClearValue = ds.depthClearValue;
        // Only set stencil ops when the texture has a stencil aspect.
        // Dawn validates that stencil ops must not be set on depth-only
        // formats.
        TextureFormat fmt = ds.view->texture()->format();
        bool hasStencil = (fmt == TextureFormat::depth24plusStencil8 ||
                           fmt == TextureFormat::depth32floatStencil8);
        if (hasStencil)
        {
            depthStencilAttachment.stencilLoadOp =
                (ds.stencilLoadOp == LoadOp::clear) ? wgpu::LoadOp::Clear
                                                    : wgpu::LoadOp::Load;
            depthStencilAttachment.stencilStoreOp =
                (ds.stencilStoreOp == StoreOp::store) ? wgpu::StoreOp::Store
                                                      : wgpu::StoreOp::Discard;
            depthStencilAttachment.stencilClearValue = ds.stencilClearValue;
        }
        else
        {
            depthStencilAttachment.stencilReadOnly = true;
        }
        pDepthStencil = &depthStencilAttachment;
    }

    wgpu::RenderPassDescriptor passDesc{};
    passDesc.label = desc.label;
    passDesc.colorAttachmentCount = desc.colorCount;
    passDesc.colorAttachments = colorAttachments;
    passDesc.depthStencilAttachment = pDepthStencil;

    pass.m_wgpuPassEncoder = m_wgpuCommandEncoder.BeginRenderPass(&passDesc);

    return pass;
}

// ============================================================================
// wrapCanvasTexture
// ============================================================================

rcp<TextureView> Context::wrapCanvasTexture(gpu::RenderCanvas* canvas)
{
    assert(canvas != nullptr);

    auto* wgpuTarget =
        static_cast<gpu::RenderTargetWebGPU*>(canvas->renderTarget());

    // Derive the ore format from the actual WebGPU surface format so any MSAA
    // texture created from this descriptor matches the resolve target exactly.
    auto wgpuFmt = wgpuTarget->framebufferFormat();
    TextureFormat oreFormat;
    switch (wgpuFmt)
    {
        case wgpu::TextureFormat::BGRA8Unorm:
            oreFormat = TextureFormat::bgra8unorm;
            break;
        case wgpu::TextureFormat::RGBA16Float:
            oreFormat = TextureFormat::rgba16float;
            break;
        case wgpu::TextureFormat::RGB10A2Unorm:
            oreFormat = TextureFormat::rgb10a2unorm;
            break;
        default:
            oreFormat = TextureFormat::rgba8unorm;
            break;
    }

    TextureDesc texDesc{};
    texDesc.width = canvas->width();
    texDesc.height = canvas->height();
    texDesc.format = oreFormat;
    texDesc.type = TextureType::texture2D;
    texDesc.renderTarget = true;
    texDesc.numMipmaps = 1;
    texDesc.sampleCount = 1;

    // Borrow the WebGPU texture — the RenderCanvas owns it.
    auto texture = rcp<Texture>(new Texture(texDesc));
    texture->m_wgpuTexture = wgpuTarget->targetTexture();

    TextureViewDesc viewDesc{};
    viewDesc.texture = texture.get();
    viewDesc.dimension = TextureViewDimension::texture2D;
    viewDesc.baseMipLevel = 0;
    viewDesc.mipCount = 1;
    viewDesc.baseLayer = 0;
    viewDesc.layerCount = 1;

    auto view = rcp<TextureView>(new TextureView(std::move(texture), viewDesc));
    view->m_wgpuTextureView = wgpuTarget->targetTextureView();

    return view;
}

rcp<TextureView> Context::wrapRiveTexture(gpu::Texture* gpuTex,
                                          uint32_t w,
                                          uint32_t h)
{
    if (!gpuTex)
        return nullptr;

    // nativeHandle() returns the raw WGPUTexture handle.
    WGPUTexture rawTex = static_cast<WGPUTexture>(gpuTex->nativeHandle());
    if (!rawTex)
        return nullptr;

    // Acquire takes ownership (will release on destruction). Since we're
    // borrowing, AddRef/Reference first so the original owner's ref is
    // preserved. Use the same version guard as the rest of the codebase.
#if (defined(RIVE_WEBGPU) && RIVE_WEBGPU > 1) || defined(RIVE_DAWN)
    wgpuTextureAddRef(rawTex);
#else
    wgpuTextureReference(rawTex);
#endif
    wgpu::Texture wgpuTex = wgpu::Texture::Acquire(rawTex);

    TextureDesc texDesc{};
    texDesc.width = w;
    texDesc.height = h;
    texDesc.format = TextureFormat::rgba8unorm; // Images decode to RGBA8.
    texDesc.type = TextureType::texture2D;
    texDesc.renderTarget = false;
    texDesc.numMipmaps = 1;
    texDesc.sampleCount = 1;

    auto texture = rcp<Texture>(new Texture(texDesc));
    texture->m_wgpuTexture = wgpuTex; // Borrow.

    TextureViewDesc viewDesc{};
    viewDesc.texture = texture.get();
    viewDesc.dimension = TextureViewDimension::texture2D;
    viewDesc.baseMipLevel = 0;
    viewDesc.mipCount = 1;
    viewDesc.baseLayer = 0;
    viewDesc.layerCount = 1;

    auto view = rcp<TextureView>(new TextureView(std::move(texture), viewDesc));
    // Create a wgpu::TextureView for sampling.
    wgpu::TextureViewDescriptor tvd{};
    tvd.format = wgpu::TextureFormat::RGBA8Unorm;
    tvd.dimension = wgpu::TextureViewDimension::e2D;
    tvd.baseMipLevel = 0;
    tvd.mipLevelCount = 1;
    tvd.baseArrayLayer = 0;
    tvd.arrayLayerCount = 1;
    view->m_wgpuTextureView = wgpuTex.CreateView(&tvd);

    return view;
}

} // namespace rive::ore
