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
#include "rive/renderer/metal/render_context_metal_impl.h"
#include "rive/rive_types.hpp"

#include <string>

#import <Metal/Metal.h>

namespace rive::ore
{

// ============================================================================
// Enum → Metal conversion helpers
// ============================================================================

static MTLPixelFormat oreFormatToMTL(TextureFormat format)
{
    switch (format)
    {
        case TextureFormat::r8unorm:
            return MTLPixelFormatR8Unorm;
        case TextureFormat::rg8unorm:
            return MTLPixelFormatRG8Unorm;
        case TextureFormat::rgba8unorm:
            return MTLPixelFormatRGBA8Unorm;
        case TextureFormat::rgba8snorm:
            return MTLPixelFormatRGBA8Snorm;
        case TextureFormat::bgra8unorm:
            return MTLPixelFormatBGRA8Unorm;
        case TextureFormat::rgba16float:
            return MTLPixelFormatRGBA16Float;
        case TextureFormat::rg16float:
            return MTLPixelFormatRG16Float;
        case TextureFormat::r16float:
            return MTLPixelFormatR16Float;
        case TextureFormat::rgba32float:
            return MTLPixelFormatRGBA32Float;
        case TextureFormat::rg32float:
            return MTLPixelFormatRG32Float;
        case TextureFormat::r32float:
            return MTLPixelFormatR32Float;
        case TextureFormat::rgb10a2unorm:
            return MTLPixelFormatRGB10A2Unorm;
        case TextureFormat::r11g11b10float:
            return MTLPixelFormatRG11B10Float;
        case TextureFormat::depth16unorm:
            return MTLPixelFormatDepth16Unorm;
        case TextureFormat::depth24plusStencil8:
#if defined(RIVE_IOS) || defined(RIVE_IOS_SIMULATOR) || TARGET_CPU_ARM64
            // iOS and Apple Silicon (ARM64) don't support Depth24Unorm.
            return MTLPixelFormatDepth32Float_Stencil8;
#else
            return MTLPixelFormatDepth24Unorm_Stencil8;
#endif
        case TextureFormat::depth32float:
            return MTLPixelFormatDepth32Float;
        case TextureFormat::depth32floatStencil8:
            return MTLPixelFormatDepth32Float_Stencil8;
        case TextureFormat::bc1unorm:
#if TARGET_OS_OSX || (__IPHONE_OS_VERSION_MAX_ALLOWED >= 160400)
            if (@available(iOS 16.4, *))
                return MTLPixelFormatBC1_RGBA;
#endif
            RIVE_UNREACHABLE();
        case TextureFormat::bc3unorm:
#if TARGET_OS_OSX || (__IPHONE_OS_VERSION_MAX_ALLOWED >= 160400)
            if (@available(iOS 16.4, *))
                return MTLPixelFormatBC3_RGBA;
#endif
            RIVE_UNREACHABLE();
        case TextureFormat::bc7unorm:
#if TARGET_OS_OSX || (__IPHONE_OS_VERSION_MAX_ALLOWED >= 160400)
            if (@available(iOS 16.4, *))
                return MTLPixelFormatBC7_RGBAUnorm;
#endif
            RIVE_UNREACHABLE();
        case TextureFormat::etc2rgb8:
            return MTLPixelFormatETC2_RGB8;
        case TextureFormat::etc2rgba8:
            return MTLPixelFormatEAC_RGBA8;
        case TextureFormat::astc4x4:
            return MTLPixelFormatASTC_4x4_LDR;
        case TextureFormat::astc6x6:
            return MTLPixelFormatASTC_6x6_LDR;
        case TextureFormat::astc8x8:
            return MTLPixelFormatASTC_8x8_LDR;
    }
    RIVE_UNREACHABLE();
}

static MTLTextureType oreTextureTypeToMTL(TextureType type)
{
    switch (type)
    {
        case TextureType::texture2D:
            return MTLTextureType2D;
        case TextureType::cube:
            return MTLTextureTypeCube;
        case TextureType::texture3D:
            return MTLTextureType3D;
        case TextureType::array2D:
            return MTLTextureType2DArray;
    }
    RIVE_UNREACHABLE();
}

static MTLSamplerMinMagFilter oreFilterToMTL(Filter filter)
{
    switch (filter)
    {
        case Filter::nearest:
            return MTLSamplerMinMagFilterNearest;
        case Filter::linear:
            return MTLSamplerMinMagFilterLinear;
    }
    RIVE_UNREACHABLE();
}

static MTLSamplerMipFilter oreMipFilterToMTL(Filter filter)
{
    switch (filter)
    {
        case Filter::nearest:
            return MTLSamplerMipFilterNearest;
        case Filter::linear:
            return MTLSamplerMipFilterLinear;
    }
    RIVE_UNREACHABLE();
}

static MTLSamplerAddressMode oreWrapToMTL(WrapMode mode)
{
    switch (mode)
    {
        case WrapMode::repeat:
            return MTLSamplerAddressModeRepeat;
        case WrapMode::mirrorRepeat:
            return MTLSamplerAddressModeMirrorRepeat;
        case WrapMode::clampToEdge:
            return MTLSamplerAddressModeClampToEdge;
    }
    RIVE_UNREACHABLE();
}

static MTLCompareFunction oreCompareFunctionToMTL(CompareFunction fn)
{
    switch (fn)
    {
        case CompareFunction::none:
            return MTLCompareFunctionNever; // Should not be called for none.
        case CompareFunction::never:
            return MTLCompareFunctionNever;
        case CompareFunction::less:
            return MTLCompareFunctionLess;
        case CompareFunction::equal:
            return MTLCompareFunctionEqual;
        case CompareFunction::lessEqual:
            return MTLCompareFunctionLessEqual;
        case CompareFunction::greater:
            return MTLCompareFunctionGreater;
        case CompareFunction::notEqual:
            return MTLCompareFunctionNotEqual;
        case CompareFunction::greaterEqual:
            return MTLCompareFunctionGreaterEqual;
        case CompareFunction::always:
            return MTLCompareFunctionAlways;
    }
    RIVE_UNREACHABLE();
}

static MTLLoadAction oreLoadOpToMTL(LoadOp op)
{
    switch (op)
    {
        case LoadOp::clear:
            return MTLLoadActionClear;
        case LoadOp::load:
            return MTLLoadActionLoad;
        case LoadOp::dontCare:
            return MTLLoadActionDontCare;
    }
    RIVE_UNREACHABLE();
}

static MTLStoreAction oreStoreOpToMTL(StoreOp op)
{
    switch (op)
    {
        case StoreOp::store:
            return MTLStoreActionStore;
        case StoreOp::discard:
            return MTLStoreActionDontCare;
    }
    RIVE_UNREACHABLE();
}

static MTLBlendFactor oreBlendFactorToMTL(BlendFactor f)
{
    switch (f)
    {
        case BlendFactor::zero:
            return MTLBlendFactorZero;
        case BlendFactor::one:
            return MTLBlendFactorOne;
        case BlendFactor::srcColor:
            return MTLBlendFactorSourceColor;
        case BlendFactor::oneMinusSrcColor:
            return MTLBlendFactorOneMinusSourceColor;
        case BlendFactor::srcAlpha:
            return MTLBlendFactorSourceAlpha;
        case BlendFactor::oneMinusSrcAlpha:
            return MTLBlendFactorOneMinusSourceAlpha;
        case BlendFactor::dstColor:
            return MTLBlendFactorDestinationColor;
        case BlendFactor::oneMinusDstColor:
            return MTLBlendFactorOneMinusDestinationColor;
        case BlendFactor::dstAlpha:
            return MTLBlendFactorDestinationAlpha;
        case BlendFactor::oneMinusDstAlpha:
            return MTLBlendFactorOneMinusDestinationAlpha;
        case BlendFactor::srcAlphaSaturated:
            return MTLBlendFactorSourceAlphaSaturated;
        case BlendFactor::blendColor:
            return MTLBlendFactorBlendColor;
        case BlendFactor::oneMinusBlendColor:
            return MTLBlendFactorOneMinusBlendColor;
    }
    RIVE_UNREACHABLE();
}

static MTLBlendOperation oreBlendOpToMTL(BlendOp op)
{
    switch (op)
    {
        case BlendOp::add:
            return MTLBlendOperationAdd;
        case BlendOp::subtract:
            return MTLBlendOperationSubtract;
        case BlendOp::reverseSubtract:
            return MTLBlendOperationReverseSubtract;
        case BlendOp::min:
            return MTLBlendOperationMin;
        case BlendOp::max:
            return MTLBlendOperationMax;
    }
    RIVE_UNREACHABLE();
}

static MTLStencilOperation oreStencilOpToMTL(StencilOp op)
{
    switch (op)
    {
        case StencilOp::keep:
            return MTLStencilOperationKeep;
        case StencilOp::zero:
            return MTLStencilOperationZero;
        case StencilOp::replace:
            return MTLStencilOperationReplace;
        case StencilOp::incrementClamp:
            return MTLStencilOperationIncrementClamp;
        case StencilOp::decrementClamp:
            return MTLStencilOperationDecrementClamp;
        case StencilOp::invert:
            return MTLStencilOperationInvert;
        case StencilOp::incrementWrap:
            return MTLStencilOperationIncrementWrap;
        case StencilOp::decrementWrap:
            return MTLStencilOperationDecrementWrap;
    }
    RIVE_UNREACHABLE();
}

static MTLPrimitiveType orePrimitiveTopologyToMTL(PrimitiveTopology topo)
{
    switch (topo)
    {
        case PrimitiveTopology::pointList:
            return MTLPrimitiveTypePoint;
        case PrimitiveTopology::lineList:
            return MTLPrimitiveTypeLine;
        case PrimitiveTopology::lineStrip:
            return MTLPrimitiveTypeLineStrip;
        case PrimitiveTopology::triangleList:
            return MTLPrimitiveTypeTriangle;
        case PrimitiveTopology::triangleStrip:
            return MTLPrimitiveTypeTriangleStrip;
    }
    RIVE_UNREACHABLE();
}

static MTLVertexFormat oreVertexFormatToMTL(VertexFormat fmt)
{
    switch (fmt)
    {
        case VertexFormat::float1:
            return MTLVertexFormatFloat;
        case VertexFormat::float2:
            return MTLVertexFormatFloat2;
        case VertexFormat::float3:
            return MTLVertexFormatFloat3;
        case VertexFormat::float4:
            return MTLVertexFormatFloat4;
        case VertexFormat::uint8x4:
            return MTLVertexFormatUChar4;
        case VertexFormat::sint8x4:
            return MTLVertexFormatChar4;
        case VertexFormat::unorm8x4:
            return MTLVertexFormatUChar4Normalized;
        case VertexFormat::snorm8x4:
            return MTLVertexFormatChar4Normalized;
        case VertexFormat::uint16x2:
            return MTLVertexFormatUShort2;
        case VertexFormat::sint16x2:
            return MTLVertexFormatShort2;
        case VertexFormat::unorm16x2:
            return MTLVertexFormatUShort2Normalized;
        case VertexFormat::snorm16x2:
            return MTLVertexFormatShort2Normalized;
        case VertexFormat::uint16x4:
            return MTLVertexFormatUShort4;
        case VertexFormat::sint16x4:
            return MTLVertexFormatShort4;
        case VertexFormat::float16x2:
            return MTLVertexFormatHalf2;
        case VertexFormat::float16x4:
            return MTLVertexFormatHalf4;
        case VertexFormat::uint32:
            return MTLVertexFormatUInt;
        case VertexFormat::uint32x2:
            return MTLVertexFormatUInt2;
        case VertexFormat::uint32x3:
            return MTLVertexFormatUInt3;
        case VertexFormat::uint32x4:
            return MTLVertexFormatUInt4;
        case VertexFormat::sint32:
            return MTLVertexFormatInt;
        case VertexFormat::sint32x2:
            return MTLVertexFormatInt2;
        case VertexFormat::sint32x3:
            return MTLVertexFormatInt3;
        case VertexFormat::sint32x4:
            return MTLVertexFormatInt4;
    }
    RIVE_UNREACHABLE();
}

static MTLCullMode oreCullModeToMTL(CullMode mode)
{
    switch (mode)
    {
        case CullMode::none:
            return MTLCullModeNone;
        case CullMode::front:
            return MTLCullModeFront;
        case CullMode::back:
            return MTLCullModeBack;
    }
    RIVE_UNREACHABLE();
}

static MTLWinding oreWindingToMTL(FaceWinding winding)
{
    switch (winding)
    {
        case FaceWinding::clockwise:
            return MTLWindingClockwise;
        case FaceWinding::counterClockwise:
            return MTLWindingCounterClockwise;
    }
    RIVE_UNREACHABLE();
}

static MTLColorWriteMask oreColorWriteMaskToMTL(ColorWriteMask mask)
{
    MTLColorWriteMask mtl = MTLColorWriteMaskNone;
    if ((mask & ColorWriteMask::red) != ColorWriteMask::none)
        mtl |= MTLColorWriteMaskRed;
    if ((mask & ColorWriteMask::green) != ColorWriteMask::none)
        mtl |= MTLColorWriteMaskGreen;
    if ((mask & ColorWriteMask::blue) != ColorWriteMask::none)
        mtl |= MTLColorWriteMaskBlue;
    if ((mask & ColorWriteMask::alpha) != ColorWriteMask::none)
        mtl |= MTLColorWriteMaskAlpha;
    return mtl;
}

static MTLIndexType oreIndexFormatToMTL(IndexFormat format)
{
    switch (format)
    {
        case IndexFormat::uint16:
            return MTLIndexTypeUInt16;
        case IndexFormat::uint32:
            return MTLIndexTypeUInt32;
        case IndexFormat::none:
            break;
    }
    RIVE_UNREACHABLE();
}

// Metal uses a single [[buffer(n)]] namespace for both vertex buffers and
// uniform/constant buffers. WGSL→MSL compilation assigns uniform buffers
// starting at buffer index 0. To avoid collisions we reserve the first N
// slots for uniforms and map vertex buffers to slots
// [kMetalVertexBufferBase, ...).
static constexpr uint32_t kMetalVertexBufferBase = 16;

static TextureFormat mtlFormatToOre(MTLPixelFormat fmt)
{
    switch (fmt)
    {
        case MTLPixelFormatRGBA8Unorm:
            return TextureFormat::rgba8unorm;
        case MTLPixelFormatBGRA8Unorm:
            return TextureFormat::bgra8unorm;
        case MTLPixelFormatRGBA16Float:
            return TextureFormat::rgba16float;
        case MTLPixelFormatRGB10A2Unorm:
            return TextureFormat::rgb10a2unorm;
        default:
            return TextureFormat::rgba8unorm;
    }
}

// ============================================================================
// Metal implementation helpers (inline)
// Shared between metal-only and metal+gl builds.
// ============================================================================

inline void Context::mtlPopulateFeatures(id<MTLDevice> device)
{
    Features& f = m_features;
    f.colorBufferFloat = true;
    f.perTargetBlend = true;
    f.perTargetWriteMask = true;
    f.textureViewSampling = true;
    f.drawBaseInstance = true;
    f.depthBiasClamp = true;
    f.anisotropicFiltering = true;
    f.texture3D = true;
    f.textureArrays = true;
    f.computeShaders = true;
    f.storageBuffers = true;

#if defined(RIVE_IOS) || defined(RIVE_IOS_SIMULATOR)
    f.bc = false;
    f.etc2 = true;
    f.astc = true;
#else
    f.bc = true;
    f.etc2 = false;
    f.astc = false;
#endif

    f.maxColorAttachments = 8;
    f.maxTextureSize2D = 16384;
    f.maxTextureSizeCube = 16384;
    f.maxTextureSize3D = 2048;
    f.maxUniformBufferSize = 256 * 1024;
    f.maxVertexAttributes = 31;
    f.maxSamplers = 16;

    // Query the actual MSAA sample count limit from the device.
    f.maxSamples = 1;
    for (uint32_t sc : {8u, 4u, 2u})
    {
        if ([device supportsTextureSampleCount:sc])
        {
            f.maxSamples = sc;
            break;
        }
    }
}

inline rcp<Buffer> Context::mtlMakeBuffer(const BufferDesc& desc)
{
    auto buffer = rcp<Buffer>(new Buffer(desc.size, desc.usage));
    if (desc.data)
    {
        buffer->m_mtlBuffer =
            [m_mtlDevice newBufferWithBytes:desc.data
                                     length:desc.size
                                    options:MTLResourceStorageModeShared];
    }
    else
    {
        buffer->m_mtlBuffer =
            [m_mtlDevice newBufferWithLength:desc.size
                                     options:MTLResourceStorageModeShared];
    }
    if (desc.label)
    {
        buffer->m_mtlBuffer.label = @(desc.label);
    }
    return buffer;
}

inline rcp<Texture> Context::mtlMakeTexture(const TextureDesc& desc)
{
    MTLTextureDescriptor* td = [[MTLTextureDescriptor alloc] init];

    bool isMSAA = desc.sampleCount > 1 && desc.type == TextureType::texture2D;
    td.textureType =
        isMSAA ? MTLTextureType2DMultisample : oreTextureTypeToMTL(desc.type);
    td.pixelFormat = oreFormatToMTL(desc.format);
    td.width = desc.width;
    td.height = desc.height;
    td.mipmapLevelCount = isMSAA ? 1 : desc.numMipmaps;
    td.sampleCount = desc.sampleCount;
    // Private storage is GPU-only — replaceRegion (CPU upload) is undefined.
    // Use Shared for uploadable textures so upload() works on all Metal
    // devices. Render-target-only textures stay Private.
    td.storageMode =
        desc.renderTarget ? MTLStorageModePrivate : MTLStorageModeShared;
    td.usage = MTLTextureUsageShaderRead;

    if (desc.type == TextureType::texture3D)
    {
        td.depth = desc.depthOrArrayLayers;
        td.arrayLength = 1;
    }
    else if (desc.type == TextureType::array2D)
    {
        td.depth = 1;
        td.arrayLength = desc.depthOrArrayLayers;
    }
    else if (desc.type == TextureType::cube)
    {
        td.depth = 1;
        td.arrayLength = 1;
    }
    else
    {
        td.depth = 1;
        td.arrayLength = 1;
    }

    if (desc.renderTarget)
    {
        td.usage |= MTLTextureUsageRenderTarget;
    }

    TextureFormat fmt = desc.format;
    if (fmt == TextureFormat::depth24plusStencil8 ||
        fmt == TextureFormat::depth32floatStencil8)
    {
        td.usage |= MTLTextureUsagePixelFormatView;
    }

    auto texture = rcp<Texture>(new Texture(desc));
#if RIVE_OBJC_EXCEPTIONS
    @try
    {
        texture->m_mtlTexture = [m_mtlDevice newTextureWithDescriptor:td];
    }
    @catch (NSException* e)
    {
        NSLog(@"RIVE ORE: makeTexture exception: %@", e.reason ?: @"unknown");
        return nullptr;
    }
#else
    texture->m_mtlTexture = [m_mtlDevice newTextureWithDescriptor:td];
#endif
    if (texture->m_mtlTexture == nil)
    {
        NSLog(@"RIVE ORE: makeTexture: newTextureWithDescriptor returned nil");
        return nullptr;
    }
    if (desc.label)
    {
        texture->m_mtlTexture.label = @(desc.label);
    }
    return texture;
}

inline rcp<TextureView> Context::mtlMakeTextureView(const TextureViewDesc& desc)
{
    Texture* tex = desc.texture;
    assert(tex != nullptr);

    auto view = rcp<TextureView>(new TextureView(ref_rcp(tex), desc));

    if (tex->m_mtlTexture.textureType == MTLTextureType2DMultisample)
        return view;

    MTLTextureType viewType;
    switch (desc.dimension)
    {
        case TextureViewDimension::texture2D:
            viewType = MTLTextureType2D;
            break;
        case TextureViewDimension::cube:
            viewType = MTLTextureTypeCube;
            break;
        case TextureViewDimension::texture3D:
            viewType = MTLTextureType3D;
            break;
        case TextureViewDimension::array2D:
            viewType = MTLTextureType2DArray;
            break;
        case TextureViewDimension::cubeArray:
            viewType = MTLTextureTypeCubeArray;
            break;
    }

    NSRange mipRange = NSMakeRange(desc.baseMipLevel, desc.mipCount);
    NSRange sliceRange = NSMakeRange(desc.baseLayer, desc.layerCount);

    view->m_mtlTextureView = [tex->m_mtlTexture
        newTextureViewWithPixelFormat:tex->m_mtlTexture.pixelFormat
                          textureType:viewType
                               levels:mipRange
                               slices:sliceRange];
    return view;
}

inline rcp<Sampler> Context::mtlMakeSampler(const SamplerDesc& desc)
{
    MTLSamplerDescriptor* sd = [[MTLSamplerDescriptor alloc] init];
    sd.minFilter = oreFilterToMTL(desc.minFilter);
    sd.magFilter = oreFilterToMTL(desc.magFilter);
    sd.mipFilter = oreMipFilterToMTL(desc.mipmapFilter);
    sd.sAddressMode = oreWrapToMTL(desc.wrapU);
    sd.tAddressMode = oreWrapToMTL(desc.wrapV);
    sd.rAddressMode = oreWrapToMTL(desc.wrapW);
    sd.lodMinClamp = desc.minLod;
    sd.lodMaxClamp = desc.maxLod;
    sd.maxAnisotropy = desc.maxAnisotropy;

    if (desc.compare != CompareFunction::none)
    {
        sd.compareFunction = oreCompareFunctionToMTL(desc.compare);
    }

    if (desc.label)
    {
        sd.label = @(desc.label);
    }

    auto sampler = rcp<Sampler>(new Sampler());
    sampler->m_mtlSampler = [m_mtlDevice newSamplerStateWithDescriptor:sd];
    return sampler;
}

inline rcp<ShaderModule> Context::mtlMakeShaderModule(
    const ShaderModuleDesc& desc)
{
    auto module = rcp<ShaderModule>(new ShaderModule());

    NSError* err = nil;
    NSString* source = [[NSString alloc] initWithBytes:desc.code
                                                length:desc.codeSize
                                              encoding:NSUTF8StringEncoding];
    module->m_mtlLibrary =
        [m_mtlDevice newLibraryWithSource:source options:nil error: &err];
    if (err != nil || module->m_mtlLibrary == nil)
    {
        NSLog(@"RIVE ORE: makeShaderModule error: %@",
              err ? err.localizedDescription : @"<nil>");
        return nullptr;
    }

    module->applyBindingMapFromDesc(desc);
    return module;
}

inline rcp<Pipeline> Context::mtlMakePipeline(const PipelineDesc& desc,
                                              std::string* outError)
{
    auto pipeline = rcp<Pipeline>(new Pipeline(desc));

    // --- Validate user-supplied layouts against shader binding map ---
    {
        std::string err;
        if (!validateLayoutsAgainstBindingMap(pipeline->m_bindingMap,
                                              desc.bindGroupLayouts,
                                              desc.bindGroupLayoutCount,
                                              &err) ||
            !validateColorRequiresFragment(
                desc.colorCount, desc.fragmentModule != nullptr, &err))
        {
            if (outError)
                *outError = err;
            else
                setLastError("makePipeline: %s", err.c_str());
            return nullptr;
        }
    }

    // --- Render Pipeline State ---
    MTLRenderPipelineDescriptor* rpd =
        [[MTLRenderPipelineDescriptor alloc] init];

    // Vertex function.
    if (desc.vertexModule->m_mtlLibrary == nil)
    {
        if (outError)
            *outError = "vertex shader library is nil";
        return nullptr;
    }
    rpd.vertexFunction = [desc.vertexModule->m_mtlLibrary
        newFunctionWithName:@(desc.vertexEntryPoint)];
    if (rpd.vertexFunction == nil)
    {
        std::string msg = std::string("vertex entry point '") +
                          desc.vertexEntryPoint +
                          "' not found in shader library";
        NSLog(@"RIVE ORE: makePipeline: %s", msg.c_str());
        if (outError)
            *outError = msg;
        return nullptr;
    }
    // Fragment function. Depth-only pipelines leave it nil — Metal allows
    // a vertex-only pipeline that writes only depth.
    if (desc.fragmentModule != nullptr)
    {
        if (desc.fragmentModule->m_mtlLibrary == nil)
        {
            if (outError)
                *outError = "fragment shader library is nil";
            return nullptr;
        }
        rpd.fragmentFunction = [desc.fragmentModule->m_mtlLibrary
            newFunctionWithName:@(desc.fragmentEntryPoint)];
        if (rpd.fragmentFunction == nil)
        {
            std::string msg = std::string("fragment entry point '") +
                              desc.fragmentEntryPoint +
                              "' not found in shader library";
            NSLog(@"RIVE ORE: makePipeline: %s", msg.c_str());
            if (outError)
                *outError = msg;
            return nullptr;
        }
    }

    // Vertex descriptor.
    if (desc.vertexBufferCount > 0)
    {
        MTLVertexDescriptor* vd = [[MTLVertexDescriptor alloc] init];
        for (uint32_t bufIdx = 0; bufIdx < desc.vertexBufferCount; ++bufIdx)
        {
            const auto& layout = desc.vertexBuffers[bufIdx];
            uint32_t mtlBufIdx = bufIdx + kMetalVertexBufferBase;
            vd.layouts[mtlBufIdx].stride = layout.stride;
            vd.layouts[mtlBufIdx].stepFunction =
                layout.stepMode == VertexStepMode::instance
                    ? MTLVertexStepFunctionPerInstance
                    : MTLVertexStepFunctionPerVertex;
            vd.layouts[mtlBufIdx].stepRate = 1;

            for (uint32_t attrIdx = 0; attrIdx < layout.attributeCount;
                 ++attrIdx)
            {
                const auto& attr = layout.attributes[attrIdx];
                vd.attributes[attr.shaderSlot].format =
                    oreVertexFormatToMTL(attr.format);
                vd.attributes[attr.shaderSlot].offset = attr.offset;
                vd.attributes[attr.shaderSlot].bufferIndex = mtlBufIdx;
            }
        }
        rpd.vertexDescriptor = vd;
    }

    // Color attachments.
    for (uint32_t i = 0; i < desc.colorCount; ++i)
    {
        const auto& ct = desc.colorTargets[i];
        rpd.colorAttachments[i].pixelFormat = oreFormatToMTL(ct.format);
        rpd.colorAttachments[i].writeMask =
            oreColorWriteMaskToMTL(ct.writeMask);

        if (ct.blendEnabled)
        {
            rpd.colorAttachments[i].blendingEnabled = YES;
            rpd.colorAttachments[i].sourceRGBBlendFactor =
                oreBlendFactorToMTL(ct.blend.srcColor);
            rpd.colorAttachments[i].destinationRGBBlendFactor =
                oreBlendFactorToMTL(ct.blend.dstColor);
            rpd.colorAttachments[i].rgbBlendOperation =
                oreBlendOpToMTL(ct.blend.colorOp);
            rpd.colorAttachments[i].sourceAlphaBlendFactor =
                oreBlendFactorToMTL(ct.blend.srcAlpha);
            rpd.colorAttachments[i].destinationAlphaBlendFactor =
                oreBlendFactorToMTL(ct.blend.dstAlpha);
            rpd.colorAttachments[i].alphaBlendOperation =
                oreBlendOpToMTL(ct.blend.alphaOp);
        }
    }

    // Depth/stencil attachment format. The format sentinel
    // (`rgba8unorm` = "no depth-stencil", per `ore_types.hpp`) is the
    // single source of truth — the depthCompare/depthWrite/stencil flags
    // configure how the attachment is *used*, not whether it exists.
    // Pre-fix this gated on the ops, so a stencil prepass with
    // `compare=always passOp=replace` (writes stencil but doesn't depth-
    // test or stencil-test) was treated as "no depth-stencil" and the
    // pipeline got `MTLPixelFormatInvalid`, mismatching the framebuffer
    // attachment at draw time.
    const bool hasDepthStencil =
        desc.depthStencil.format != TextureFormat::rgba8unorm;
    if (hasDepthStencil)
    {
        rpd.depthAttachmentPixelFormat =
            oreFormatToMTL(desc.depthStencil.format);
        if (desc.depthStencil.format == TextureFormat::depth24plusStencil8 ||
            desc.depthStencil.format == TextureFormat::depth32floatStencil8)
        {
            rpd.stencilAttachmentPixelFormat =
                oreFormatToMTL(desc.depthStencil.format);
        }
    }

    rpd.rasterSampleCount = desc.sampleCount;

    if (desc.label)
    {
        rpd.label = @(desc.label);
    }

    NSError* pipelineErr = nil;
#if RIVE_OBJC_EXCEPTIONS
    @try
    {
        pipeline->m_mtlPipeline =
            [m_mtlDevice newRenderPipelineStateWithDescriptor:rpd
                                                        error:&pipelineErr];
    }
    @catch (NSException* e)
    {
        NSString* msg = e.reason ?: @"unknown Metal exception";
        NSLog(@"RIVE ORE: makePipeline exception: %@", msg);
        if (outError)
            *outError = msg.UTF8String;
        return nullptr;
    }
#else
    pipeline->m_mtlPipeline =
        [m_mtlDevice newRenderPipelineStateWithDescriptor:rpd
                                                    error:&pipelineErr];
#endif
    if (pipelineErr != nil || pipeline->m_mtlPipeline == nil)
    {
        NSString* msg = pipelineErr ? pipelineErr.localizedDescription
                                    : @"nil pipeline, no error details";
        NSLog(@"RIVE ORE: makePipeline error: %@", msg);
        if (outError)
            *outError = msg.UTF8String;
        return nullptr;
    }

    // --- Depth/Stencil State ---
    MTLDepthStencilDescriptor* dsd = [[MTLDepthStencilDescriptor alloc] init];
    dsd.depthCompareFunction =
        oreCompareFunctionToMTL(desc.depthStencil.depthCompare);
    dsd.depthWriteEnabled = desc.depthStencil.depthWriteEnabled;

    // Stencil front.
    MTLStencilDescriptor* frontStencil = [[MTLStencilDescriptor alloc] init];
    frontStencil.stencilCompareFunction =
        oreCompareFunctionToMTL(desc.stencilFront.compare);
    frontStencil.stencilFailureOperation =
        oreStencilOpToMTL(desc.stencilFront.failOp);
    frontStencil.depthFailureOperation =
        oreStencilOpToMTL(desc.stencilFront.depthFailOp);
    frontStencil.depthStencilPassOperation =
        oreStencilOpToMTL(desc.stencilFront.passOp);
    frontStencil.readMask = desc.stencilReadMask;
    frontStencil.writeMask = desc.stencilWriteMask;
    dsd.frontFaceStencil = frontStencil;

    // Stencil back.
    MTLStencilDescriptor* backStencil = [[MTLStencilDescriptor alloc] init];
    backStencil.stencilCompareFunction =
        oreCompareFunctionToMTL(desc.stencilBack.compare);
    backStencil.stencilFailureOperation =
        oreStencilOpToMTL(desc.stencilBack.failOp);
    backStencil.depthFailureOperation =
        oreStencilOpToMTL(desc.stencilBack.depthFailOp);
    backStencil.depthStencilPassOperation =
        oreStencilOpToMTL(desc.stencilBack.passOp);
    backStencil.readMask = desc.stencilReadMask;
    backStencil.writeMask = desc.stencilWriteMask;
    dsd.backFaceStencil = backStencil;

    pipeline->m_mtlDepthStencil =
        [m_mtlDevice newDepthStencilStateWithDescriptor:dsd];

    return pipeline;
}

inline rcp<BindGroup> Context::mtlMakeBindGroup(const BindGroupDesc& desc)
{
    if (desc.layout == nullptr)
    {
        setLastError("makeBindGroup: BindGroupDesc::layout is null");
        return nullptr;
    }
    BindGroupLayout* layout = desc.layout;
    const uint32_t groupIndex = layout->groupIndex();
    if (groupIndex >= kMaxBindGroups)
    {
        setLastError("makeBindGroup: layout->groupIndex %u out of range",
                     groupIndex);
        return nullptr;
    }

    auto bg = rcp<BindGroup>(new BindGroup());
    bg->m_context = this;
    bg->m_layoutRef = ref_rcp(layout);

    // Resolve per-stage Metal slots from the layout's pre-resolved
    // nativeSlotVS/FS fields. The layout was built via
    // `makeLayoutFromShader` (helper) which queried the shader's
    // BindingMap. Both stage slots may differ (Metal has independent
    // VS/FS argument tables); kNativeSlotAbsent skips the per-stage emit
    // at setBindGroup time.
    auto lookupStages = [&](uint32_t binding,
                            BindingKind expected,
                            uint16_t* outVS,
                            uint16_t* outFS) -> bool {
        const BindGroupLayoutEntry* le = layout->findEntry(binding);
        if (le == nullptr)
        {
            setLastError("makeBindGroup: (group=%u, binding=%u) not declared "
                         "in BindGroupLayout",
                         groupIndex,
                         binding);
            return false;
        }
        const bool kindOK = le->kind == expected ||
                            ((le->kind == BindingKind::sampler ||
                              le->kind == BindingKind::comparisonSampler) &&
                             (expected == BindingKind::sampler ||
                              expected == BindingKind::comparisonSampler));
        if (!kindOK)
        {
            setLastError("makeBindGroup: (group=%u, binding=%u) layout kind "
                         "mismatch",
                         groupIndex,
                         binding);
            return false;
        }
        *outVS = (le->nativeSlotVS == BindGroupLayoutEntry::kNativeSlotAbsent)
                     ? BindingMap::kAbsent
                     : static_cast<uint16_t>(le->nativeSlotVS);
        *outFS = (le->nativeSlotFS == BindGroupLayoutEntry::kNativeSlotAbsent)
                     ? BindingMap::kAbsent
                     : static_cast<uint16_t>(le->nativeSlotFS);
        if (*outVS == BindingMap::kAbsent && *outFS == BindingMap::kAbsent)
        {
            setLastError("makeBindGroup: (group=%u, binding=%u) layout has no "
                         "resolved native slot — call makeLayoutFromShader",
                         groupIndex,
                         binding);
            return false;
        }
        return true;
    };

    uint32_t nBufs = std::min(desc.uboCount, 8u);
    for (uint32_t i = 0; i < nBufs; i++)
    {
        const auto& u = desc.ubos[i];
        bg->m_retainedBuffers.push_back(ref_rcp(u.buffer));
        BindGroup::MTLBufferBinding binding;
        binding.buffer = u.buffer->m_mtlBuffer;
        binding.offset = u.offset;
        binding.binding = u.slot;
        if (!lookupStages(u.slot,
                          BindingKind::uniformBuffer,
                          &binding.vsSlot,
                          &binding.fsSlot))
            continue;
        binding.hasDynamicOffset = layout->hasDynamicOffset(u.slot);
        if (binding.hasDynamicOffset)
            bg->m_dynamicOffsetCount++;
        bg->m_mtlBuffers.push_back(binding);
    }
    // Sort UBOs by WGSL @binding so dynamicOffsets[] is consumed in
    // BindGroupLayout-entry order (WebGPU contract).
    std::sort(bg->m_mtlBuffers.begin(),
              bg->m_mtlBuffers.end(),
              [](const BindGroup::MTLBufferBinding& a,
                 const BindGroup::MTLBufferBinding& b) {
                  return a.binding < b.binding;
              });

    uint32_t nTexs = std::min(desc.textureCount, 8u);
    for (uint32_t i = 0; i < nTexs; i++)
    {
        const auto& t = desc.textures[i];
        bg->m_retainedViews.push_back(ref_rcp(t.view));
        BindGroup::MTLTextureBinding binding;
        binding.texture = t.view->mtlTexture();
        if (!lookupStages(t.slot,
                          BindingKind::sampledTexture,
                          &binding.vsSlot,
                          &binding.fsSlot))
            continue;
        bg->m_mtlTextures.push_back(binding);
    }

    uint32_t nSamps = std::min(desc.samplerCount, 8u);
    for (uint32_t i = 0; i < nSamps; i++)
    {
        const auto& s = desc.samplers[i];
        bg->m_retainedSamplers.push_back(ref_rcp(s.sampler));
        BindGroup::MTLSamplerBinding binding;
        binding.sampler = s.sampler->m_mtlSampler;
        if (!lookupStages(
                s.slot, BindingKind::sampler, &binding.vsSlot, &binding.fsSlot))
            continue;
        bg->m_mtlSamplers.push_back(binding);
    }

    return bg;
}

inline RenderPass Context::mtlBeginRenderPass(const RenderPassDesc& desc,
                                              std::string* outError)
{
    assert(m_mtlCommandBuffer != nil);

    MTLRenderPassDescriptor* rpd =
        [MTLRenderPassDescriptor renderPassDescriptor];

    for (uint32_t i = 0; i < desc.colorCount; ++i)
    {
        const auto& ca = desc.colorAttachments[i];
        id<MTLTexture> mtlTex = ca.view->mtlTexture();
        rpd.colorAttachments[i].texture = mtlTex;
        bool hasView = (mtlTex != ca.view->texture()->mtlTexture());
        rpd.colorAttachments[i].level = hasView ? 0 : ca.view->baseMipLevel();
        rpd.colorAttachments[i].slice = hasView ? 0 : ca.view->baseLayer();
        rpd.colorAttachments[i].loadAction = oreLoadOpToMTL(ca.loadOp);
        rpd.colorAttachments[i].storeAction = oreStoreOpToMTL(ca.storeOp);
        rpd.colorAttachments[i].clearColor = MTLClearColorMake(
            ca.clearColor.r, ca.clearColor.g, ca.clearColor.b, ca.clearColor.a);

        if (ca.resolveTarget)
        {
            id<MTLTexture> resolveTex = ca.resolveTarget->mtlTexture();
            rpd.colorAttachments[i].resolveTexture = resolveTex;
            bool resolveHasView =
                (resolveTex != ca.resolveTarget->texture()->mtlTexture());
            rpd.colorAttachments[i].resolveLevel =
                resolveHasView ? 0 : ca.resolveTarget->baseMipLevel();
            rpd.colorAttachments[i].resolveSlice =
                resolveHasView ? 0 : ca.resolveTarget->baseLayer();
            // Honor the user's storeOp on the MSAA buffer: when they
            // ask for `Store` AND a resolve target, use Metal's
            // combined `StoreAndMultisampleResolve` so the MSAA
            // attachment survives for a subsequent load+resolve
            // pattern. Pre-fix this unconditionally collapsed to
            // `MultisampleResolve`, discarding the multisample buffer
            // even when the script asked to keep it.
            rpd.colorAttachments[i].storeAction =
                (ca.storeOp == StoreOp::store)
                    ? MTLStoreActionStoreAndMultisampleResolve
                    : MTLStoreActionMultisampleResolve;
        }
    }

    if (desc.depthStencil.view)
    {
        id<MTLTexture> depthTex = desc.depthStencil.view->mtlTexture();
        bool depthHasView =
            (depthTex != desc.depthStencil.view->texture()->mtlTexture());
        rpd.depthAttachment.texture = depthTex;
        rpd.depthAttachment.level =
            depthHasView ? 0 : desc.depthStencil.view->baseMipLevel();
        rpd.depthAttachment.slice =
            depthHasView ? 0 : desc.depthStencil.view->baseLayer();
        rpd.depthAttachment.loadAction =
            oreLoadOpToMTL(desc.depthStencil.depthLoadOp);
        rpd.depthAttachment.storeAction =
            oreStoreOpToMTL(desc.depthStencil.depthStoreOp);
        rpd.depthAttachment.clearDepth = desc.depthStencil.depthClearValue;

        TextureFormat depthFmt = desc.depthStencil.view->texture()->format();
        if (depthFmt == TextureFormat::depth24plusStencil8 ||
            depthFmt == TextureFormat::depth32floatStencil8)
        {
            rpd.stencilAttachment.texture = depthTex;
            rpd.stencilAttachment.level =
                depthHasView ? 0 : desc.depthStencil.view->baseMipLevel();
            rpd.stencilAttachment.slice =
                depthHasView ? 0 : desc.depthStencil.view->baseLayer();
            rpd.stencilAttachment.loadAction =
                oreLoadOpToMTL(desc.depthStencil.stencilLoadOp);
            rpd.stencilAttachment.storeAction =
                oreStoreOpToMTL(desc.depthStencil.stencilStoreOp);
            rpd.stencilAttachment.clearStencil =
                desc.depthStencil.stencilClearValue;
        }
    }

    id<MTLRenderCommandEncoder> encoder =
        [m_mtlCommandBuffer renderCommandEncoderWithDescriptor:rpd];

    if (encoder == nil)
    {
        NSLog(@"RIVE ORE: beginRenderPass: renderCommandEncoderWithDescriptor "
              @"returned nil — render pass descriptor is invalid. "
              @"colorCount=%u, hasDepth=%s",
              desc.colorCount,
              desc.depthStencil.view ? "yes" : "no");
    }

    if (desc.label)
    {
        encoder.label = @(desc.label);
    }

    RenderPass pass;
    pass.m_context = this;
    pass.m_mtlEncoder = encoder;
    pass.m_mtlCommandBuffer = m_mtlCommandBuffer;
    pass.populateAttachmentMetadata(desc);
    return pass;
}

inline rcp<TextureView> Context::mtlWrapCanvasTexture(gpu::RenderCanvas* canvas)
{
    assert(canvas != nullptr);

    auto* metalTarget =
        static_cast<gpu::RenderTargetMetal*>(canvas->renderTarget());
    id<MTLTexture> mtlTexture = metalTarget->targetTexture();
    assert(mtlTexture != nil);

    uint32_t w = canvas->width();
    uint32_t h = canvas->height();

    TextureDesc texDesc{};
    texDesc.width = w;
    texDesc.height = h;
    texDesc.format = mtlFormatToOre(mtlTexture.pixelFormat);
    texDesc.type = TextureType::texture2D;
    texDesc.renderTarget = true;
    texDesc.numMipmaps = 1;
    texDesc.sampleCount = 1;

    auto texture = rcp<Texture>(new Texture(texDesc));
    texture->m_mtlTexture = mtlTexture;

    TextureViewDesc viewDesc{};
    viewDesc.texture = texture.get();
    viewDesc.dimension = TextureViewDimension::texture2D;
    viewDesc.baseMipLevel = 0;
    viewDesc.mipCount = 1;
    viewDesc.baseLayer = 0;
    viewDesc.layerCount = 1;

    auto view = rcp<TextureView>(new TextureView(std::move(texture), viewDesc));
    view->m_mtlTextureView = mtlTexture;
    return view;
}

// ============================================================================
// Context
// When both Metal and GL are compiled (macOS), ore_context_metal_gl.mm
// provides all Context method definitions with runtime dispatch instead.
// ============================================================================

#if !defined(ORE_BACKEND_GL)

Context::Context() {}

Context::~Context()
{
    m_mtlCommandBuffer = nil;
    m_mtlQueue = nil;
    m_mtlDevice = nil;
}

Context::Context(Context&& other) noexcept :
    m_features(other.m_features),
    m_mtlDevice(other.m_mtlDevice),
    m_mtlQueue(other.m_mtlQueue),
    m_mtlCommandBuffer(other.m_mtlCommandBuffer)
{
    other.m_mtlDevice = nil;
    other.m_mtlQueue = nil;
    other.m_mtlCommandBuffer = nil;
}

Context& Context::operator=(Context&& other) noexcept
{
    if (this != &other)
    {
        m_mtlCommandBuffer = nil;
        m_mtlQueue = nil;
        m_mtlDevice = nil;

        m_features = other.m_features;
        m_mtlDevice = other.m_mtlDevice;
        m_mtlQueue = other.m_mtlQueue;
        m_mtlCommandBuffer = other.m_mtlCommandBuffer;
        other.m_mtlDevice = nil;
        other.m_mtlQueue = nil;
        other.m_mtlCommandBuffer = nil;
    }
    return *this;
}

Context Context::createMetal(id<MTLDevice> device, id<MTLCommandQueue> queue)
{
    Context ctx;
    ctx.m_mtlDevice = device;
    ctx.m_mtlQueue = queue;
    ctx.mtlPopulateFeatures(device);
    return ctx;
}

void Context::beginFrame() { m_mtlCommandBuffer = [m_mtlQueue commandBuffer]; }

void Context::waitForGPU()
{
#if defined(ORE_BACKEND_METAL)
    if (m_mtlCommandBuffer)
        [m_mtlCommandBuffer waitUntilCompleted];
#endif
}

void Context::endFrame()
{
    if (m_mtlCommandBuffer)
    {
        // Capture deferred BindGroups in a `__block` vector that the
        // completion handler clears once the GPU is done with the
        // command buffer. Pre-fix the next `beginFrame()` cleared
        // `m_deferredBindGroups` immediately, which freed the
        // underlying `id<MTL...>` resources while the GPU was still
        // reading them — surfaces under iOS thermal-throttle delays
        // and Apple-Silicon shared-memory races. Refcount drops happen
        // on a Metal-internal thread; safe because each resource's
        // destructor is `delete this` against ARC-managed handles.
        if (!m_deferredBindGroups.empty())
        {
            __block std::vector<rcp<BindGroup>> deferred =
                std::move(m_deferredBindGroups);
            m_deferredBindGroups.clear();
            [m_mtlCommandBuffer addCompletedHandler:^(id<MTLCommandBuffer>) {
              deferred.clear();
            }];
        }
        [m_mtlCommandBuffer commit];
        m_mtlCommandBuffer = nil;
    }
}

// ============================================================================
// makeBuffer
// ============================================================================

rcp<Buffer> Context::makeBuffer(const BufferDesc& desc)
{
    return mtlMakeBuffer(desc);
}

// ============================================================================
// makeTexture
// ============================================================================

rcp<Texture> Context::makeTexture(const TextureDesc& desc)
{
    return mtlMakeTexture(desc);
}

// ============================================================================
// makeTextureView
// ============================================================================

rcp<TextureView> Context::makeTextureView(const TextureViewDesc& desc)
{
    return mtlMakeTextureView(desc);
}

// ============================================================================
// makeSampler
// ============================================================================

rcp<Sampler> Context::makeSampler(const SamplerDesc& desc)
{
    return mtlMakeSampler(desc);
}

// ============================================================================
// makeShaderModule
// ============================================================================

rcp<ShaderModule> Context::makeShaderModule(const ShaderModuleDesc& desc)
{
    return mtlMakeShaderModule(desc);
}

// ============================================================================
// makePipeline
// ============================================================================

rcp<Pipeline> Context::makePipeline(const PipelineDesc& desc,
                                    std::string* outError)
{
    return mtlMakePipeline(desc, outError);
}

// ============================================================================
// makeBindGroup
// ============================================================================

rcp<BindGroup> Context::makeBindGroup(const BindGroupDesc& desc)
{
    return mtlMakeBindGroup(desc);
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
    // Metal has no native layout object — entries-only suffices.
    return layout;
}

// ============================================================================
// beginRenderPass
// ============================================================================

RenderPass Context::beginRenderPass(const RenderPassDesc& desc,
                                    std::string* outError)
{
    finishActiveRenderPass();
    return mtlBeginRenderPass(desc, outError);
}

// ============================================================================
// wrapCanvasTexture
// ============================================================================

rcp<TextureView> Context::wrapCanvasTexture(gpu::RenderCanvas* canvas)
{
    return mtlWrapCanvasTexture(canvas);
}

rcp<TextureView> Context::wrapRiveTexture(gpu::Texture* gpuTex,
                                          uint32_t w,
                                          uint32_t h)
{
    if (!gpuTex)
        return nullptr;

    id<MTLTexture> mtlTex = (__bridge id<MTLTexture>)gpuTex->nativeHandle();
    if (!mtlTex)
        return nullptr;

    TextureDesc texDesc{};
    texDesc.width = w;
    texDesc.height = h;
    texDesc.format = mtlFormatToOre(mtlTex.pixelFormat);
    texDesc.type = TextureType::texture2D;
    texDesc.renderTarget = false;
    texDesc.numMipmaps = 1;
    texDesc.sampleCount = 1;

    auto texture = rcp<Texture>(new Texture(texDesc));
    texture->m_mtlTexture = mtlTex; // Borrow — caller owns via RenderImage.

    TextureViewDesc viewDesc{};
    viewDesc.texture = texture.get();
    viewDesc.dimension = TextureViewDimension::texture2D;
    viewDesc.baseMipLevel = 0;
    viewDesc.mipCount = 1;
    viewDesc.baseLayer = 0;
    viewDesc.layerCount = 1;

    auto view = rcp<TextureView>(new TextureView(std::move(texture), viewDesc));
    view->m_mtlTextureView = mtlTex;
    return view;
}

#endif // !ORE_BACKEND_GL

} // namespace rive::ore
