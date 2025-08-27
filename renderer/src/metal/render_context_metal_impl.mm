/*
 * Copyright 2023 Rive
 */

#include "rive/renderer/metal/render_context_metal_impl.h"

#include "background_shader_compiler.h"
#include "rive/renderer/buffer_ring.hpp"
#include "rive/renderer/texture.hpp"
#include "rive/renderer/rive_render_buffer.hpp"
#include "shaders/constants.glsl"
#include <sstream>

#include "generated/shaders/color_ramp.exports.h"
#include "generated/shaders/tessellate.exports.h"

#if defined(RIVE_IOS_SIMULATOR)
#import <mach-o/arch.h>
#endif

namespace rive::gpu
{
#if defined(RIVE_IOS)
#include "generated/shaders/rive_pls_ios.metallib.c"
#elif defined(RIVE_IOS_SIMULATOR)
#include "generated/shaders/rive_pls_ios_simulator.metallib.c"
#elif defined(RIVE_XROS)
#include "generated/shaders/rive_renderer_xros.metallib.c"
#elif defined(RIVE_XROS_SIMULATOR)
#include "generated/shaders/rive_renderer_xros_simulator.metallib.c"
#elif defined(RIVE_APPLETVOS)
#include "generated/shaders/rive_renderer_appletvos.metallib.c"
#elif defined(RIVE_APPLETVOS_SIMULATOR)
#include "generated/shaders/rive_renderer_appletvsimulator.metallib.c"
#else
#include "generated/shaders/rive_pls_macosx.metallib.c"
#endif

static id<MTLRenderPipelineState> make_pipeline_state(
    id<MTLDevice> gpu, MTLRenderPipelineDescriptor* desc)
{
    NSError* err = [NSError errorWithDomain:@"pipeline_create"
                                       code:201
                                   userInfo:nil];
    id<MTLRenderPipelineState> state =
        [gpu newRenderPipelineStateWithDescriptor:desc error:&err];
    if (!state)
    {
        fprintf(stderr, "%s\n", err.localizedDescription.UTF8String);
        abort();
    }
    return state;
}

static MTLSamplerAddressMode address_mode_for_image_wrap(ImageWrap wrap)
{
    switch (wrap)
    {
        case ImageWrap::clamp:
            return MTLSamplerAddressModeClampToEdge;
        case ImageWrap::repeat:
            return MTLSamplerAddressModeRepeat;
        case ImageWrap::mirror:
            return MTLSamplerAddressModeMirrorRepeat;
    }

    RIVE_UNREACHABLE();
}

static MTLSamplerMinMagFilter min_mag_filter_for_image_filter(
    ImageFilter option)
{
    switch (option)
    {
        case ImageFilter::trilinear:
            return MTLSamplerMinMagFilterLinear;
        case ImageFilter::nearest:
            return MTLSamplerMinMagFilterNearest;
    }

    RIVE_UNREACHABLE();
}

static MTLSamplerMipFilter mip_filter_for_image_filter(ImageFilter option)
{
    switch (option)
    {
        case ImageFilter::trilinear:
            return MTLSamplerMipFilterLinear;
        case ImageFilter::nearest:
            return MTLSamplerMipFilterNearest;
    }

    RIVE_UNREACHABLE();
}

// Renders color ramps to the gradient texture.
class RenderContextMetalImpl::ColorRampPipeline
{
public:
    ColorRampPipeline(id<MTLDevice> gpu, id<MTLLibrary> plsLibrary)
    {
        MTLRenderPipelineDescriptor* desc =
            [[MTLRenderPipelineDescriptor alloc] init];
        desc.vertexFunction =
            [plsLibrary newFunctionWithName:@GLSL_colorRampVertexMain];
        desc.fragmentFunction =
            [plsLibrary newFunctionWithName:@GLSL_colorRampFragmentMain];
        desc.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
        m_pipelineState = make_pipeline_state(gpu, desc);
    }

    id<MTLRenderPipelineState> pipelineState() const { return m_pipelineState; }

private:
    id<MTLRenderPipelineState> m_pipelineState;
};

// Renders tessellated vertices to the tessellation texture.
class RenderContextMetalImpl::TessellatePipeline
{
public:
    TessellatePipeline(id<MTLDevice> gpu, id<MTLLibrary> plsLibrary)
    {
        MTLRenderPipelineDescriptor* desc =
            [[MTLRenderPipelineDescriptor alloc] init];
        desc.vertexFunction =
            [plsLibrary newFunctionWithName:@GLSL_tessellateVertexMain];
        desc.fragmentFunction =
            [plsLibrary newFunctionWithName:@GLSL_tessellateFragmentMain];
        desc.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA32Uint;
        m_pipelineState = make_pipeline_state(gpu, desc);
    }

    id<MTLRenderPipelineState> pipelineState() const { return m_pipelineState; }

private:
    id<MTLRenderPipelineState> m_pipelineState;
};

// Renders feathered fills and strokes to the atlas.
class RenderContextMetalImpl::AtlasPipeline
{
public:
    AtlasPipeline(id<MTLDevice> gpu,
                  id<MTLLibrary> plsLibrary,
                  NSString* fragmentMain,
                  MTLBlendOperation blendOperation)
    {
        MTLRenderPipelineDescriptor* desc =
            [[MTLRenderPipelineDescriptor alloc] init];
        desc.vertexFunction =
            [plsLibrary newFunctionWithName:@GLSL_atlasVertexMain];
        desc.fragmentFunction = [plsLibrary newFunctionWithName:fragmentMain];
        desc.colorAttachments[0].pixelFormat = MTLPixelFormatR32Float;
        desc.colorAttachments[0].blendingEnabled = TRUE;
        desc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorOne;
        desc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOne;
        desc.colorAttachments[0].rgbBlendOperation = blendOperation;
        desc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
        desc.colorAttachments[0].destinationAlphaBlendFactor =
            MTLBlendFactorOne;
        desc.colorAttachments[0].alphaBlendOperation = blendOperation;
        desc.colorAttachments[0].writeMask = MTLColorWriteMaskAll;
        m_pipelineState = make_pipeline_state(gpu, desc);
    }

    id<MTLRenderPipelineState> pipelineState() const { return m_pipelineState; }

private:
    id<MTLRenderPipelineState> m_pipelineState;
};

// Renders paths to the main render target.
class RenderContextMetalImpl::DrawPipeline
{
public:
    // Precompiled functions are embedded in namespaces. Return the fully
    // qualified name of the desired function, including its namespace.
    static NSString* GetPrecompiledFunctionName(
        DrawType drawType,
        gpu::ShaderFeatures shaderFeatures,
        gpu::ShaderMiscFlags shaderMiscFlags,
        id<MTLLibrary> precompiledLibrary,
        const char* functionBaseName)
    {
        // Each feature corresponds to a specific index in the namespaceID.
        // These must stay in sync with generate_draw_combinations.py.
        char namespaceID[] = "000000000";
        static_assert(sizeof(namespaceID) ==
                      gpu::kShaderFeatureCount + 1 /*DRAW_INTERIOR_TRIANGLES*/ +
                          1 /*ATLAS_BLIT*/ + 1 /*null terminator*/);
        for (size_t i = 0; i < gpu::kShaderFeatureCount; ++i)
        {
            ShaderFeatures feature = static_cast<ShaderFeatures>(1 << i);
            if (shaderFeatures & feature)
            {
                namespaceID[i] = '1';
            }
            static_assert((int)ShaderFeatures::ENABLE_CLIPPING == 1 << 0);
            static_assert((int)ShaderFeatures::ENABLE_CLIP_RECT == 1 << 1);
            static_assert((int)ShaderFeatures::ENABLE_ADVANCED_BLEND == 1 << 2);
            static_assert((int)ShaderFeatures::ENABLE_FEATHER == 1 << 3);
            static_assert((int)ShaderFeatures::ENABLE_EVEN_ODD == 1 << 4);
            static_assert((int)ShaderFeatures::ENABLE_NESTED_CLIPPING ==
                          1 << 5);
            static_assert((int)ShaderFeatures::ENABLE_HSL_BLEND_MODES ==
                          1 << 6);
        }
        if (drawType == DrawType::interiorTriangulation)
        {
            namespaceID[gpu::kShaderFeatureCount] = '1';
        }
        else if (drawType == DrawType::atlasBlit)
        {
            namespaceID[gpu::kShaderFeatureCount] = '1';
            namespaceID[gpu::kShaderFeatureCount + 1] = '1';
        }

        char namespacePrefix;
        switch (drawType)
        {
            case DrawType::midpointFanPatches:
            case DrawType::midpointFanCenterAAPatches:
            case DrawType::outerCurvePatches:
            case DrawType::interiorTriangulation:
            case DrawType::atlasBlit:
                namespacePrefix =
                    (shaderMiscFlags & gpu::ShaderMiscFlags::clockwiseFill)
                        ? 'c'
                        : 'p';
                break;
            case DrawType::imageRect:
                RIVE_UNREACHABLE();
            case DrawType::imageMesh:
                namespacePrefix = 'm';
                break;
            case DrawType::atomicInitialize:
            case DrawType::atomicResolve:
            case DrawType::msaaStrokes:
            case DrawType::msaaMidpointFanBorrowedCoverage:
            case DrawType::msaaMidpointFans:
            case DrawType::msaaMidpointFanStencilReset:
            case DrawType::msaaMidpointFanPathsStencil:
            case DrawType::msaaMidpointFanPathsCover:
            case DrawType::msaaOuterCubics:
            case DrawType::msaaStencilClipReset:
                RIVE_UNREACHABLE();
        }

        return [NSString stringWithFormat:@"%c%s::%s",
                                          namespacePrefix,
                                          namespaceID,
                                          functionBaseName];
    }

    DrawPipeline(id<MTLDevice> gpu,
                 id<MTLLibrary> library,
                 NSString* vertexFunctionName,
                 NSString* fragmentFunctionName,
                 gpu::DrawType drawType,
                 gpu::InterlockMode interlockMode,
                 gpu::ShaderFeatures shaderFeatures,
                 gpu::ShaderMiscFlags shaderMiscFlags)
    {
        if (library == nil)
        {
            // This pipeline is being built from a shader that failed to
            // compile. Leave everything nil and let draws fail.
            return;
        }

        auto makePipelineState = [=](id<MTLFunction> vertexMain,
                                     id<MTLFunction> fragmentMain,
                                     MTLPixelFormat pixelFormat) {
            MTLRenderPipelineDescriptor* desc =
                [[MTLRenderPipelineDescriptor alloc] init];
            desc.vertexFunction = vertexMain;
            desc.fragmentFunction = fragmentMain;

            auto* framebuffer = desc.colorAttachments[COLOR_PLANE_IDX];
            framebuffer.pixelFormat = pixelFormat;

            switch (interlockMode)
            {
                case gpu::InterlockMode::rasterOrdering:
                    // In rasterOrdering mode, the PLS planes are accessed as
                    // color attachments.
                    desc.colorAttachments[CLIP_PLANE_IDX].pixelFormat =
                        MTLPixelFormatR32Uint;
                    desc.colorAttachments[SCRATCH_COLOR_PLANE_IDX].pixelFormat =
                        pixelFormat;
                    desc.colorAttachments[COVERAGE_PLANE_IDX].pixelFormat =
                        MTLPixelFormatR32Uint;
                    break;

                case gpu::InterlockMode::atomics:
                    // In atomic mode, the PLS planes are accessed as device
                    // buffers. We only use the "framebuffer" attachment
                    // configured above.
                    if (shaderMiscFlags &
                        gpu::ShaderMiscFlags::fixedFunctionColorOutput)
                    {
                        // The shader expectes a "src-over" blend function in
                        // order to to implement antialiasing and opacity.
                        framebuffer.blendingEnabled = TRUE;
                        framebuffer.sourceRGBBlendFactor = MTLBlendFactorOne;
                        framebuffer.destinationRGBBlendFactor =
                            MTLBlendFactorOneMinusSourceAlpha;
                        framebuffer.rgbBlendOperation = MTLBlendOperationAdd;
                        framebuffer.sourceAlphaBlendFactor = MTLBlendFactorOne;
                        framebuffer.destinationAlphaBlendFactor =
                            MTLBlendFactorOneMinusSourceAlpha;
                        framebuffer.alphaBlendOperation = MTLBlendOperationAdd;
                        framebuffer.writeMask = MTLColorWriteMaskAll;
                    }
                    else if (drawType == gpu::DrawType::atomicResolve)
                    {
                        // We're resolving from the offscreen color buffer to
                        // the framebuffer attachment. Write out the final color
                        // directly without any blend modes.
                        framebuffer.blendingEnabled = FALSE;
                        framebuffer.writeMask = MTLColorWriteMaskAll;
                    }
                    else
                    {
                        // This pipeline renders by storing to the offscreen
                        // color buffer; disable writes to the framebuffer
                        // attachment.
                        framebuffer.blendingEnabled = FALSE;
                        framebuffer.writeMask = MTLColorWriteMaskNone;
                    }
                    break;

                case gpu::InterlockMode::clockwiseAtomic:
                case gpu::InterlockMode::msaa:
                    RIVE_UNREACHABLE();
            }
            return make_pipeline_state(gpu, desc);
        };
        id<MTLFunction> vertexMain =
            [library newFunctionWithName:vertexFunctionName];
        id<MTLFunction> fragmentMain =
            [library newFunctionWithName:fragmentFunctionName];
        m_pipelineStateRGBA8 = makePipelineState(
            vertexMain, fragmentMain, MTLPixelFormatRGBA8Unorm);
        m_pipelineStateBGRA8 = makePipelineState(
            vertexMain, fragmentMain, MTLPixelFormatBGRA8Unorm);
    }

    bool valid() const
    {
        assert((m_pipelineStateRGBA8 != nil) == (m_pipelineStateBGRA8 != nil));
        return m_pipelineStateRGBA8 != nil;
    }

    id<MTLRenderPipelineState> pipelineState(MTLPixelFormat pixelFormat) const
    {
        assert(valid());
        assert(pixelFormat == MTLPixelFormatRGBA8Unorm ||
               pixelFormat == MTLPixelFormatRGBA16Float ||
               pixelFormat == MTLPixelFormatRGBA8Unorm_sRGB ||
               pixelFormat == MTLPixelFormatBGRA8Unorm ||
               pixelFormat == MTLPixelFormatBGRA8Unorm_sRGB);

        switch (pixelFormat)
        {
            case MTLPixelFormatRGBA8Unorm_sRGB:
            case MTLPixelFormatRGBA8Unorm:
            case MTLPixelFormatRGBA16Float:
                return m_pipelineStateRGBA8;
            default:
                return m_pipelineStateBGRA8;
        }
    }

private:
    id<MTLRenderPipelineState> m_pipelineStateRGBA8 = nil;
    id<MTLRenderPipelineState> m_pipelineStateBGRA8 = nil;
};

#if defined(RIVE_IOS) || defined(RIVE_XROS) || defined(RIVE_APPLETVOS)
static bool is_apple_silicon(id<MTLDevice> gpu)
{
    if (@available(iOS 13, tvOS 13, visionOS 1, *))
    {
        return [gpu supportsFamily:MTLGPUFamilyApple4];
    }
    return false;
}
#endif

class BufferRingMetalImpl : public BufferRing
{
public:
    static std::unique_ptr<BufferRingMetalImpl> Make(id<MTLDevice> gpu,
                                                     size_t capacityInBytes)
    {
        return capacityInBytes != 0
                   ? std::make_unique<BufferRingMetalImpl>(gpu, capacityInBytes)
                   : nullptr;
    }

    BufferRingMetalImpl(id<MTLDevice> gpu, size_t capacityInBytes) :
        BufferRing(capacityInBytes)
    {
        for (int i = 0; i < kBufferRingSize; ++i)
        {
            m_buffers[i] =
                [gpu newBufferWithLength:capacityInBytes
                                 options:MTLResourceStorageModeShared];
        }
    }

    id<MTLBuffer> submittedBuffer() const
    {
        return m_buffers[submittedBufferIdx()];
    }

protected:
    void* onMapBuffer(int bufferIdx, size_t mapSizeInBytes) override
    {
        return m_buffers[bufferIdx].contents;
    }

    void onUnmapAndSubmitBuffer(int bufferIdx, size_t mapSizeInBytes) override
    {}

private:
    id<MTLBuffer> m_buffers[kBufferRingSize];
};

std::unique_ptr<RenderContext> RenderContextMetalImpl::MakeContext(
    id<MTLDevice> gpu, const ContextOptions& contextOptions)
{
    auto renderContextImpl = std::unique_ptr<RenderContextMetalImpl>(
        new RenderContextMetalImpl(gpu, contextOptions));
    return std::make_unique<RenderContext>(std::move(renderContextImpl));
}

RenderContextMetalImpl::RenderContextMetalImpl(
    id<MTLDevice> gpu, const ContextOptions& contextOptions) :
    m_contextOptions(contextOptions), m_gpu(gpu)
{
    // It appears, so far, that we don't need to use flat interpolation for path
    // IDs on any Apple device, and it's faster not to.
    m_platformFeatures.avoidFlatVaryings = true;
    m_platformFeatures.clipSpaceBottomUp = true;
    m_platformFeatures.framebufferBottomUp = false;
    if ([m_gpu supportsFamily:MTLGPUFamilyApple2] ||
        [m_gpu supportsFamily:MTLGPUFamilyMac2])
    {
        m_platformFeatures.maxTextureSize = 16384;
    }
    else
    {
        m_platformFeatures.maxTextureSize = 8192;
    }
#if defined(RIVE_IOS) || defined(RIVE_XROS) || defined(RIVE_APPLETVOS)
    m_platformFeatures.supportsRasterOrdering = true;
    m_platformFeatures.supportsFragmentShaderAtomics = false;
    if (!is_apple_silicon(m_gpu))
    {
        // The PowerVR GPU, at least on A10, has fp16 precision issues. We can't
        // use the the bottom 3 bits of the path and clip IDs in order for our
        // equality testing to work.
        m_platformFeatures.pathIDGranularity = 8;
    }
#elif defined(RIVE_IOS_SIMULATOR) || defined(RIVE_XROS_SIMULATOR) ||           \
    defined(RIVE_APPLETVOS_SIMULATOR)
    // The simulator does not support framebuffer reads. Fall back on atomic
    // mode.
    m_platformFeatures.supportsRasterOrdering = false;
    m_platformFeatures.supportsFragmentShaderAtomics = true;
#else
    m_platformFeatures.supportsRasterOrdering =
        [m_gpu supportsFamily:MTLGPUFamilyApple1] &&
        !contextOptions.disableFramebufferReads;
    m_platformFeatures.supportsFragmentShaderAtomics = true;
#endif
    m_platformFeatures.atomicPLSMustBeInitializedAsDraw = true;

#if defined(RIVE_IOS) || defined(RIVE_XROS) || defined(RIVE_XROS_SIMULATOR) || \
    defined(RIVE_APPLETVOS) || defined(RIVE_APPLETVOS_SIMULATOR)
    // Atomic barriers are never used on iOS, but if we ever did need them, we
    // would use rasterOrderGroups.
    m_metalFeatures.atomicBarrierType = AtomicBarrierType::rasterOrderGroup;
#elif defined(RIVE_IOS_SIMULATOR)
    const NXArchInfo* hostArchitecture = NXGetLocalArchInfo();
    if (strncmp(hostArchitecture->name, "arm64", 5) == 0)
    {
        // The simulator doesn't advertise support for raster order groups, but
        // they appear to work anyway on an Apple-Silicon-hosted simulator. Use
        // rasterOrderGroup in this case because it's much faster than
        // renderPassBreak. (On Intel/AMD this doesn't matter anyway because
        // renderPassBreaks are cheap and actually faster than
        // rasterOrderGroups.)
        m_metalFeatures.atomicBarrierType = AtomicBarrierType::rasterOrderGroup;
    }
    else
    {
        m_metalFeatures.atomicBarrierType = AtomicBarrierType::renderPassBreak;
    }
#else
    // Use real memory barriers for atomic mode if they're availabile.
    // "GPU devices in Apple3 through Apple9 families don’t support memory
    // barriers that include the MTLRenderStages.fragment or .tile stages in the
    // after argument..."
    if (([m_gpu supportsFamily:MTLGPUFamilyCommon2] ||
         [m_gpu supportsFamily:MTLGPUFamilyMac2]) &&
        ![m_gpu supportsFamily:MTLGPUFamilyApple3])
    {
        m_metalFeatures.atomicBarrierType = AtomicBarrierType::memoryBarrier;
    }
    else if (m_gpu.rasterOrderGroupsSupported)
    {
        m_metalFeatures.atomicBarrierType = AtomicBarrierType::rasterOrderGroup;
    }
    else
    {
        m_metalFeatures.atomicBarrierType = AtomicBarrierType::renderPassBreak;
    }
#endif

    for (int i = 0; i < rive::ImageSampler::MAX_SAMPLER_PERMUTATIONS; ++i)
    {
        auto wrapX = ImageSampler::GetWrapXOptionFromKey(i);
        auto wrapY = ImageSampler::GetWrapYOptionFromKey(i);
        auto filter = ImageSampler::GetFilterOptionFromKey(i);

        MTLSamplerDescriptor* samplerDescriptor = [MTLSamplerDescriptor new];
        samplerDescriptor.minFilter = min_mag_filter_for_image_filter(filter);
        samplerDescriptor.magFilter = min_mag_filter_for_image_filter(filter);
        samplerDescriptor.mipFilter = mip_filter_for_image_filter(filter);
        samplerDescriptor.sAddressMode = address_mode_for_image_wrap(wrapX);
        samplerDescriptor.tAddressMode = address_mode_for_image_wrap(wrapY);

        m_imageSamplers[i] =
            [gpu newSamplerStateWithDescriptor:samplerDescriptor];
    }

    m_backgroundShaderCompiler =
        std::make_unique<BackgroundShaderCompiler>(m_gpu, m_metalFeatures);

    // Load the precompiled shaders.
    dispatch_data_t metallibData = dispatch_data_create(
#if defined(RIVE_IOS)
        rive_pls_ios_metallib,
        rive_pls_ios_metallib_len,
#elif defined(RIVE_IOS_SIMULATOR)
        rive_pls_ios_simulator_metallib,
        rive_pls_ios_simulator_metallib_len,
#elif defined(RIVE_XROS)
        rive_renderer_xros_metallib,
        rive_renderer_xros_metallib_len,
#elif defined(RIVE_XROS_SIMULATOR)
        rive_renderer_xros_simulator_metallib,
        rive_renderer_xros_simulator_metallib_len,
#elif defined(RIVE_APPLETVOS)
        rive_renderer_appletvos_metallib,
        rive_renderer_appletvos_metallib_len,
#elif defined(RIVE_APPLETVOS_SIMULATOR)
        rive_renderer_appletvsimulator_metallib,
        rive_renderer_appletvsimulator_metallib_len,
#else
        rive_pls_macosx_metallib,
        rive_pls_macosx_metallib_len,
#endif
        nil,
        nil);
    NSError* err = [NSError errorWithDomain:@"metallib_load"
                                       code:200
                                   userInfo:nil];
    m_plsPrecompiledLibrary = [m_gpu newLibraryWithData:metallibData
                                                  error:&err];
    if (m_plsPrecompiledLibrary == nil)
    {
        fprintf(stderr, "Failed to load pls metallib.\n");
        fprintf(stderr, "%s\n", err.localizedDescription.UTF8String);
        abort();
    }

    m_colorRampPipeline =
        std::make_unique<ColorRampPipeline>(m_gpu, m_plsPrecompiledLibrary);

    MTLTextureDescriptor* desc = [[MTLTextureDescriptor alloc] init];
    desc.pixelFormat = MTLPixelFormatR16Float;
    desc.textureType = MTLTextureType1DArray;
    desc.width = gpu::GAUSSIAN_TABLE_SIZE;
    desc.mipmapLevelCount = 1;
    desc.arrayLength = FEATHER_TEXTURE_1D_ARRAY_LENGTH;
    desc.usage = MTLTextureUsageShaderRead;
    m_featherTexture = [m_gpu newTextureWithDescriptor:desc];
    [m_featherTexture
        replaceRegion:MTLRegionMake2D(0, 0, gpu::GAUSSIAN_TABLE_SIZE, 1)
          mipmapLevel:0
                slice:FEATHER_FUNCTION_ARRAY_INDEX
            withBytes:gpu::g_gaussianIntegralTableF16
          bytesPerRow:sizeof(gpu::g_gaussianIntegralTableF16)
        bytesPerImage:sizeof(gpu::g_gaussianIntegralTableF16)];
    [m_featherTexture
        replaceRegion:MTLRegionMake2D(0, 0, gpu::GAUSSIAN_TABLE_SIZE, 1)
          mipmapLevel:0
                slice:FEATHER_INVERSE_FUNCTION_ARRAY_INDEX
            withBytes:gpu::g_inverseGaussianIntegralTableF16
          bytesPerRow:sizeof(gpu::g_gaussianIntegralTableF16)
        bytesPerImage:sizeof(gpu::g_gaussianIntegralTableF16)];

    m_tessPipeline =
        std::make_unique<TessellatePipeline>(m_gpu, m_plsPrecompiledLibrary);
    m_tessSpanIndexBuffer =
        [m_gpu newBufferWithBytes:gpu::kTessSpanIndices
                           length:sizeof(gpu::kTessSpanIndices)
                          options:MTLResourceStorageModeShared];

    // The precompiled static library has a fully-featured shader for each
    // drawType in "rasterOrdering" mode. We load these at initialization and
    // use them while waiting for the background compiler to generate more
    // specialized, higher performance shaders.
    if (m_platformFeatures.supportsRasterOrdering)
    {
        for (auto drawType : {DrawType::midpointFanPatches,
                              DrawType::interiorTriangulation,
                              DrawType::atlasBlit,
                              DrawType::imageMesh})
        {
            for (auto shaderMiscFlags : {gpu::ShaderMiscFlags::none,
                                         gpu::ShaderMiscFlags::clockwiseFill})
            {
                if (drawType == gpu::DrawType::atlasBlit &&
                    shaderMiscFlags != gpu::ShaderMiscFlags::none)
                {
                    continue;
                }
                gpu::ShaderFeatures allShaderFeatures =
                    gpu::ShaderFeaturesMaskFor(
                        drawType, gpu::InterlockMode::rasterOrdering);
                uint32_t pipelineKey =
                    ShaderUniqueKey(drawType,
                                    allShaderFeatures,
                                    gpu::InterlockMode::rasterOrdering,
                                    shaderMiscFlags);
                m_drawPipelines[pipelineKey] = std::make_unique<DrawPipeline>(
                    m_gpu,
                    m_plsPrecompiledLibrary,
                    DrawPipeline::GetPrecompiledFunctionName(
                        drawType,
                        allShaderFeatures & gpu::kVertexShaderFeaturesMask,
                        gpu::ShaderMiscFlags::none,
                        m_plsPrecompiledLibrary,
                        GLSL_drawVertexMain),
                    DrawPipeline::GetPrecompiledFunctionName(
                        drawType,
                        allShaderFeatures,
                        shaderMiscFlags,
                        m_plsPrecompiledLibrary,
                        GLSL_drawFragmentMain),
                    drawType,
                    gpu::InterlockMode::rasterOrdering,
                    allShaderFeatures,
                    shaderMiscFlags);
            }
        }
    }

    // Create vertex and index buffers for the different PLS patches.
    m_pathPatchVertexBuffer =
        [m_gpu newBufferWithLength:kPatchVertexBufferCount * sizeof(PatchVertex)
                           options:MTLResourceStorageModeShared];
    m_pathPatchIndexBuffer =
        [m_gpu newBufferWithLength:kPatchIndexBufferCount * sizeof(uint16_t)
                           options:MTLResourceStorageModeShared];
    GeneratePatchBufferData(
        reinterpret_cast<PatchVertex*>(m_pathPatchVertexBuffer.contents),
        reinterpret_cast<uint16_t*>(m_pathPatchIndexBuffer.contents));

    // Set up the imageRect rendering buffers. (gpu::InterlockMode::atomics
    // only.)
    m_imageRectVertexBuffer =
        [m_gpu newBufferWithBytes:gpu::kImageRectVertices
                           length:sizeof(gpu::kImageRectVertices)
                          options:MTLResourceStorageModeShared];
    m_imageRectIndexBuffer =
        [m_gpu newBufferWithBytes:gpu::kImageRectIndices
                           length:sizeof(gpu::kImageRectIndices)
                          options:MTLResourceStorageModeShared];
}

RenderContextMetalImpl::~RenderContextMetalImpl() {}

// If the GPU supports framebuffer reads (called "programmable blending" in the
// feature tables), PLS planes besides the main framebuffer can exist in
// ephemeral "memoryless" storage. This means their contents are never actually
// written to main memory, and they only exist in fast tiled memory.
static id<MTLTexture> make_pls_memoryless_texture(id<MTLDevice> gpu,
                                                  MTLPixelFormat pixelFormat,
                                                  uint32_t width,
                                                  uint32_t height)
{
    MTLTextureDescriptor* desc = [[MTLTextureDescriptor alloc] init];
    desc.pixelFormat = pixelFormat;
    desc.width = width;
    desc.height = height;
    desc.usage = MTLTextureUsageRenderTarget;
    desc.textureType = MTLTextureType2D;
    desc.mipmapLevelCount = 1;
    desc.storageMode = MTLStorageModeMemoryless;
    return [gpu newTextureWithDescriptor:desc];
}

RenderTargetMetal::RenderTargetMetal(id<MTLDevice> gpu,
                                     MTLPixelFormat pixelFormat,
                                     uint32_t width,
                                     uint32_t height,
                                     const PlatformFeatures& platformFeatures) :
    RenderTarget(width, height), m_gpu(gpu), m_pixelFormat(pixelFormat)
{
    m_targetTexture = nil; // Will be configured later by setTargetTexture().
    if (platformFeatures.supportsRasterOrdering)
    {
        m_coverageMemorylessTexture = make_pls_memoryless_texture(
            gpu, MTLPixelFormatR32Uint, width, height);
        m_clipMemorylessTexture = make_pls_memoryless_texture(
            gpu, MTLPixelFormatR32Uint, width, height);
        m_scratchColorMemorylessTexture =
            make_pls_memoryless_texture(gpu, m_pixelFormat, width, height);
    }
}

void RenderTargetMetal::setTargetTexture(id<MTLTexture> texture)
{
    assert(!texture || compatibleWith(texture));
    m_targetTexture = texture;
}

rcp<RenderTargetMetal> RenderContextMetalImpl::makeRenderTarget(
    MTLPixelFormat pixelFormat, uint32_t width, uint32_t height)
{
    return rcp(new RenderTargetMetal(
        m_gpu, pixelFormat, width, height, m_platformFeatures));
}

class RenderBufferMetalImpl
    : public LITE_RTTI_OVERRIDE(RiveRenderBuffer, RenderBufferMetalImpl)
{
public:
    RenderBufferMetalImpl(RenderBufferType renderBufferType,
                          RenderBufferFlags renderBufferFlags,
                          size_t sizeInBytes,
                          id<MTLDevice> gpu) :
        lite_rtti_override(renderBufferType, renderBufferFlags, sizeInBytes),
        m_gpu(gpu)
    {
        int bufferCount =
            flags() & RenderBufferFlags::mappedOnceAtInitialization
                ? 1
                : gpu::kBufferRingSize;
        for (int i = 0; i < bufferCount; ++i)
        {
            m_buffers[i] =
                [gpu newBufferWithLength:sizeInBytes
                                 options:MTLResourceStorageModeShared];
        }
    }

    id<MTLBuffer> submittedBuffer() { return m_buffers[frontBufferIdx()]; }

protected:
    void* onMap() override
    {
        assert(m_buffers[backBufferIdx()] != nil);
        return m_buffers[backBufferIdx()].contents;
    }

    void onUnmap() override {}

private:
    id<MTLDevice> m_gpu;
    id<MTLBuffer> m_buffers[gpu::kBufferRingSize];
    int m_submittedBufferIdx = -1;
};

rcp<RenderBuffer> RenderContextMetalImpl::makeRenderBuffer(
    RenderBufferType type, RenderBufferFlags flags, size_t sizeInBytes)
{
    return make_rcp<RenderBufferMetalImpl>(type, flags, sizeInBytes, m_gpu);
}

class TextureMetalImpl : public Texture
{
public:
    TextureMetalImpl(id<MTLDevice> gpu,
                     uint32_t width,
                     uint32_t height,
                     uint32_t mipLevelCount,
                     const uint8_t imageDataRGBAPremul[]) :
        Texture(width, height)
    {
        // Create the texture.
        MTLTextureDescriptor* desc = [[MTLTextureDescriptor alloc] init];
        desc.pixelFormat = MTLPixelFormatRGBA8Unorm;
        desc.width = width;
        desc.height = height;
        desc.mipmapLevelCount = mipLevelCount;
        desc.usage = MTLTextureUsageShaderRead;
        desc.textureType = MTLTextureType2D;
        m_texture = [gpu newTextureWithDescriptor:desc];

        // Specify the top-level image in the mipmap chain.
        MTLRegion region = MTLRegionMake2D(0, 0, width, height);
        [m_texture replaceRegion:region
                     mipmapLevel:0
                       withBytes:imageDataRGBAPremul
                     bytesPerRow:width * 4];
    }

    void ensureMipmaps(id<MTLCommandBuffer> commandBuffer) const
    {
        if (m_mipsDirty)
        {
            // Generate mipmaps.
            id<MTLBlitCommandEncoder> mipEncoder =
                [commandBuffer blitCommandEncoder];
            [mipEncoder generateMipmapsForTexture:m_texture];
            [mipEncoder endEncoding];
            m_mipsDirty = false;
        }
    }

    id<MTLTexture> texture() const { return m_texture; }

private:
    id<MTLTexture> m_texture;
    mutable bool m_mipsDirty = true;
};

rcp<Texture> RenderContextMetalImpl::makeImageTexture(
    uint32_t width,
    uint32_t height,
    uint32_t mipLevelCount,
    const uint8_t imageDataRGBAPremul[])
{
    return make_rcp<TextureMetalImpl>(
        m_gpu, width, height, mipLevelCount, imageDataRGBAPremul);
}

std::unique_ptr<BufferRing> RenderContextMetalImpl::makeUniformBufferRing(
    size_t capacityInBytes)
{
    return BufferRingMetalImpl::Make(m_gpu, capacityInBytes);
}

std::unique_ptr<BufferRing> RenderContextMetalImpl::makeStorageBufferRing(
    size_t capacityInBytes, gpu::StorageBufferStructure)
{
    return BufferRingMetalImpl::Make(m_gpu, capacityInBytes);
}

std::unique_ptr<BufferRing> RenderContextMetalImpl::makeVertexBufferRing(
    size_t capacityInBytes)
{
    return BufferRingMetalImpl::Make(m_gpu, capacityInBytes);
}

void RenderContextMetalImpl::resizeGradientTexture(uint32_t width,
                                                   uint32_t height)
{
    if (width == 0 || height == 0)
    {
        m_gradientTexture = nil;
        return;
    }
    MTLTextureDescriptor* desc = [[MTLTextureDescriptor alloc] init];
    desc.pixelFormat = MTLPixelFormatRGBA8Unorm;
    desc.width = width;
    desc.height = height;
    desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    desc.textureType = MTLTextureType2D;
    desc.mipmapLevelCount = 1;
    desc.storageMode = MTLStorageModePrivate;
    m_gradientTexture = [m_gpu newTextureWithDescriptor:desc];
}

void RenderContextMetalImpl::resizeTessellationTexture(uint32_t width,
                                                       uint32_t height)
{
    if (width == 0 || height == 0)
    {
        m_tessVertexTexture = nil;
        return;
    }
    MTLTextureDescriptor* desc = [[MTLTextureDescriptor alloc] init];
    desc.pixelFormat = MTLPixelFormatRGBA32Uint;
    desc.width = width;
    desc.height = height;
    desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    desc.textureType = MTLTextureType2D;
    desc.mipmapLevelCount = 1;
    desc.storageMode = MTLStorageModePrivate;
    m_tessVertexTexture = [m_gpu newTextureWithDescriptor:desc];
}

void RenderContextMetalImpl::resizeAtlasTexture(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
    {
        m_atlasTexture = nil;
        return;
    }

    MTLTextureDescriptor* desc = [[MTLTextureDescriptor alloc] init];
    desc.pixelFormat = MTLPixelFormatR32Float;
    desc.width = width;
    desc.height = height;
    desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    desc.textureType = MTLTextureType2D;
    desc.mipmapLevelCount = 1;
    desc.storageMode = MTLStorageModePrivate;
    m_atlasTexture = [m_gpu newTextureWithDescriptor:desc];

    // Don't build atlas pipelines until we get an indication that they will be
    // used.
    assert((m_atlasFillPipeline == nil) == (m_atlasStrokePipeline == nil));
    if (m_atlasFillPipeline == nil)
    {
        m_atlasFillPipeline =
            std::make_unique<AtlasPipeline>(m_gpu,
                                            m_plsPrecompiledLibrary,
                                            @GLSL_atlasFillFragmentMain,
                                            MTLBlendOperationAdd);
        m_atlasStrokePipeline =
            std::make_unique<AtlasPipeline>(m_gpu,
                                            m_plsPrecompiledLibrary,
                                            @GLSL_atlasStrokeFragmentMain,
                                            MTLBlendOperationMax);
    }
}

const RenderContextMetalImpl::DrawPipeline* RenderContextMetalImpl::
    findCompatibleDrawPipeline(gpu::DrawType drawType,
                               gpu::ShaderFeatures shaderFeatures,
                               const gpu::FlushDescriptor& desc,
                               gpu::ShaderMiscFlags shaderMiscFlags)
{
    // Find a fully-featured superset of features whose pipeline we can fall
    // back on while waiting for it to compile.
    ShaderFeatures fullyFeaturedPipelineFeatures =
        gpu::UbershaderFeaturesMaskFor(
            shaderFeatures, drawType, desc.interlockMode);

    if (m_contextOptions.shaderCompilationMode ==
        ShaderCompilationMode::onlyUbershaders)
    {
        // Force the shader features to be the full set if that's what was
        // requested.
        shaderFeatures = fullyFeaturedPipelineFeatures;
    }

    uint32_t pipelineKey = gpu::ShaderUniqueKey(
        drawType, shaderFeatures, desc.interlockMode, shaderMiscFlags);
    auto pipelineIter = m_drawPipelines.find(pipelineKey);
    if (pipelineIter == m_drawPipelines.end())
    {
        // The shader for this pipeline hasn't been scheduled for compiling yet.
        // Schedule it to compile in the background.
        m_backgroundShaderCompiler->pushJob({
            .drawType = drawType,
            .shaderFeatures = shaderFeatures,
            .interlockMode = desc.interlockMode,
            .shaderMiscFlags = shaderMiscFlags,
#ifdef WITH_RIVE_TOOLS
            .synthesizeCompilationFailure = desc.synthesizeCompilationFailures,
#endif
        });
        pipelineIter = m_drawPipelines.insert({pipelineKey, nullptr}).first;
    }

    if (pipelineIter->second == nullptr)
    {
        // The shader for this pipeline hasn't finished compiling yet.
        // Fully-featured "rasterOrdering" pipelines should have already been
        // pre-loaded from the static library.
        assert(shaderFeatures != fullyFeaturedPipelineFeatures ||
               desc.interlockMode != gpu::InterlockMode::rasterOrdering);

        // Poll to see if the shader is actually done compiling, but only wait
        // if it's a fully-feature pipeline. Otherwise, we can fall back on the
        // fully-featured pipeline while we wait for compilation.
        BackgroundCompileJob job;
        bool shouldWaitForBackgroundCompilation =
            shaderFeatures == fullyFeaturedPipelineFeatures ||
            m_contextOptions.shaderCompilationMode !=
                ShaderCompilationMode::allowAsynchronous;
        while (m_backgroundShaderCompiler->popFinishedJob(
            &job, shouldWaitForBackgroundCompilation))
        {
            uint32_t jobKey = gpu::ShaderUniqueKey(job.drawType,
                                                   job.shaderFeatures,
                                                   job.interlockMode,
                                                   job.shaderMiscFlags);
            m_drawPipelines[jobKey] =
                std::make_unique<DrawPipeline>(m_gpu,
                                               job.compiledLibrary,
                                               @GLSL_drawVertexMain,
                                               @GLSL_drawFragmentMain,
                                               job.drawType,
                                               job.interlockMode,
                                               job.shaderFeatures,
                                               job.shaderMiscFlags);
            if (jobKey == pipelineKey)
            {
                // The shader we wanted was actually done compiling and pending
                // being built into a pipeline.
                break;
            }
        }
    }

    if ((pipelineIter->second == nullptr || !pipelineIter->second->valid()) &&
        shaderFeatures != fullyFeaturedPipelineFeatures)
    {
        // The shader for this feature set hasn't finished compiling (or it
        // failed to compile). Use the uber-shader pipeline that has all
        // features enabled while we wait for it to finish.
        return findCompatibleDrawPipeline(
            drawType, fullyFeaturedPipelineFeatures, desc, shaderMiscFlags);
    }

    return pipelineIter->second.get();
}

void RenderContextMetalImpl::prepareToFlush(uint64_t nextFrameNumber,
                                            uint64_t safeFrameNumber)
{
    // Wait until the GPU finishes rendering flush "N + 1 - kBufferRingSize".
    // This ensures it is safe for the CPU to begin modifying the next buffers
    // in our rings.
    m_bufferRingIdx = (m_bufferRingIdx + 1) % kBufferRingSize;
    m_bufferRingLocks[m_bufferRingIdx].lock();
}

static id<MTLBuffer> mtl_buffer(const BufferRing* bufferRing)
{
    assert(bufferRing != nullptr);
    return static_cast<const BufferRingMetalImpl*>(bufferRing)
        ->submittedBuffer();
}

static MTLViewport make_viewport(uint32_t x,
                                 uint32_t y,
                                 uint32_t width,
                                 uint32_t height)
{
    return {
        static_cast<double>(x),
        static_cast<double>(y),
        static_cast<double>(width),
        static_cast<double>(height),
        0,
        1,
    };
}

static MTLScissorRect make_scissor(const TAABB<uint16_t>& scissor)
{
    return {
        static_cast<NSUInteger>(scissor.left),
        static_cast<NSUInteger>(scissor.top),
        static_cast<NSUInteger>(scissor.width()),
        static_cast<NSUInteger>(scissor.height()),
    };
}

id<MTLRenderCommandEncoder> RenderContextMetalImpl::makeRenderPassForDraws(
    const gpu::FlushDescriptor& flushDesc,
    MTLRenderPassDescriptor* passDesc,
    id<MTLCommandBuffer> commandBuffer,
    gpu::ShaderMiscFlags baselineShaderMiscFlags)
{
    auto* renderTarget =
        static_cast<RenderTargetMetal*>(flushDesc.renderTarget);

    id<MTLRenderCommandEncoder> encoder =
        [commandBuffer renderCommandEncoderWithDescriptor:passDesc];

    [encoder
        setViewport:make_viewport(
                        0, 0, renderTarget->width(), renderTarget->height())];
    [encoder setVertexBuffer:mtl_buffer(flushUniformBufferRing())
                      offset:flushDesc.flushUniformDataOffsetInBytes
                     atIndex:METAL_BUFFER_IDX(FLUSH_UNIFORM_BUFFER_IDX)];
    [encoder setFragmentBuffer:mtl_buffer(flushUniformBufferRing())
                        offset:flushDesc.flushUniformDataOffsetInBytes
                       atIndex:METAL_BUFFER_IDX(FLUSH_UNIFORM_BUFFER_IDX)];
    [encoder setVertexTexture:m_tessVertexTexture
                      atIndex:TESS_VERTEX_TEXTURE_IDX];
    [encoder setVertexTexture:m_featherTexture atIndex:FEATHER_TEXTURE_IDX];
    [encoder setFragmentTexture:m_gradientTexture atIndex:GRAD_TEXTURE_IDX];
    [encoder setFragmentTexture:m_featherTexture atIndex:FEATHER_TEXTURE_IDX];
    [encoder setFragmentTexture:m_atlasTexture atIndex:ATLAS_TEXTURE_IDX];
    if (flushDesc.pathCount > 0)
    {
        [encoder setVertexBuffer:mtl_buffer(pathBufferRing())
                          offset:flushDesc.firstPath * sizeof(gpu::PathData)
                         atIndex:METAL_BUFFER_IDX(PATH_BUFFER_IDX)];
        if (flushDesc.interlockMode == gpu::InterlockMode::atomics)
        {
            [encoder
                setFragmentBuffer:mtl_buffer(paintBufferRing())
                           offset:flushDesc.firstPaint * sizeof(gpu::PaintData)
                          atIndex:METAL_BUFFER_IDX(PAINT_BUFFER_IDX)];
            [encoder setFragmentBuffer:mtl_buffer(paintAuxBufferRing())
                                offset:flushDesc.firstPaintAux *
                                       sizeof(gpu::PaintAuxData)
                               atIndex:METAL_BUFFER_IDX(PAINT_AUX_BUFFER_IDX)];
        }
        else
        {
            [encoder
                setVertexBuffer:mtl_buffer(paintBufferRing())
                         offset:flushDesc.firstPaint * sizeof(gpu::PaintData)
                        atIndex:METAL_BUFFER_IDX(PAINT_BUFFER_IDX)];
            [encoder setVertexBuffer:mtl_buffer(paintAuxBufferRing())
                              offset:flushDesc.firstPaintAux *
                                     sizeof(gpu::PaintAuxData)
                             atIndex:METAL_BUFFER_IDX(PAINT_AUX_BUFFER_IDX)];
        }
    }
    if (flushDesc.contourCount > 0)
    {
        [encoder
            setVertexBuffer:mtl_buffer(contourBufferRing())
                     offset:flushDesc.firstContour * sizeof(gpu::ContourData)
                    atIndex:METAL_BUFFER_IDX(CONTOUR_BUFFER_IDX)];
    }
    if (flushDesc.interlockMode == gpu::InterlockMode::atomics)
    {
        // In atomic mode, the PLS planes are buffers that we need to bind
        // separately. Since the PLS plane indices collide with other buffer
        // bindings, offset the binding indices of these buffers by
        // DEFAULT_BINDINGS_SET_SIZE.
        if (!(baselineShaderMiscFlags &
              gpu::ShaderMiscFlags::fixedFunctionColorOutput))
        {
            [encoder
                setFragmentBuffer:renderTarget->colorAtomicBuffer()
                           offset:0
                          atIndex:METAL_BUFFER_IDX(COLOR_PLANE_IDX +
                                                   DEFAULT_BINDINGS_SET_SIZE)];
        }
        if (flushDesc.combinedShaderFeatures &
            gpu::ShaderFeatures::ENABLE_CLIPPING)
        {
            [encoder
                setFragmentBuffer:renderTarget->clipAtomicBuffer()
                           offset:0
                          atIndex:METAL_BUFFER_IDX(CLIP_PLANE_IDX +
                                                   DEFAULT_BINDINGS_SET_SIZE)];
        }
        [encoder setFragmentBuffer:renderTarget->coverageAtomicBuffer()
                            offset:0
                           atIndex:METAL_BUFFER_IDX(COVERAGE_PLANE_IDX +
                                                    DEFAULT_BINDINGS_SET_SIZE)];
    }
    if (flushDesc.wireframe)
    {
        [encoder setTriangleFillMode:MTLTriangleFillModeLines];
    }
    return encoder;
}

void RenderContextMetalImpl::flush(const FlushDescriptor& desc)
{
    assert(desc.interlockMode != gpu::InterlockMode::clockwiseAtomic);
    assert(desc.interlockMode != gpu::InterlockMode::msaa); // TODO: msaa.

    auto* renderTarget = static_cast<RenderTargetMetal*>(desc.renderTarget);
    id<MTLCommandBuffer> commandBuffer =
        (__bridge id<MTLCommandBuffer>)desc.externalCommandBuffer;

    // Render the color ramps to the gradient texture.
    if (desc.gradSpanCount > 0)
    {
        MTLRenderPassDescriptor* gradPass =
            [MTLRenderPassDescriptor renderPassDescriptor];
        gradPass.renderTargetWidth = kGradTextureWidth;
        gradPass.renderTargetHeight = desc.gradDataHeight;
        gradPass.colorAttachments[0].loadAction = MTLLoadActionDontCare;
        gradPass.colorAttachments[0].storeAction = MTLStoreActionStore;
        gradPass.colorAttachments[0].texture = m_gradientTexture;

        id<MTLRenderCommandEncoder> gradEncoder =
            [commandBuffer renderCommandEncoderWithDescriptor:gradPass];
        [gradEncoder
            setViewport:make_viewport(0,
                                      0,
                                      kGradTextureWidth,
                                      static_cast<float>(desc.gradDataHeight))];
        [gradEncoder
            setRenderPipelineState:m_colorRampPipeline->pipelineState()];
        [gradEncoder
            setVertexBuffer:mtl_buffer(flushUniformBufferRing())
                     offset:desc.flushUniformDataOffsetInBytes
                    atIndex:METAL_BUFFER_IDX(FLUSH_UNIFORM_BUFFER_IDX)];
        [gradEncoder
            setVertexBuffer:mtl_buffer(gradSpanBufferRing())
                     offset:desc.firstGradSpan * sizeof(gpu::GradientSpan)
                    atIndex:0];
        [gradEncoder setCullMode:MTLCullModeBack];
        [gradEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip
                        vertexStart:0
                        vertexCount:gpu::GRAD_SPAN_TRI_STRIP_VERTEX_COUNT
                      instanceCount:desc.gradSpanCount];
        [gradEncoder endEncoding];
    }

    // Tessellate all curves into vertices in the tessellation texture.
    if (desc.tessVertexSpanCount > 0)
    {
        MTLRenderPassDescriptor* tessPass =
            [MTLRenderPassDescriptor renderPassDescriptor];
        tessPass.renderTargetWidth = kTessTextureWidth;
        tessPass.renderTargetHeight = desc.tessDataHeight;
        tessPass.colorAttachments[0].loadAction = MTLLoadActionDontCare;
        tessPass.colorAttachments[0].storeAction = MTLStoreActionStore;
        tessPass.colorAttachments[0].texture = m_tessVertexTexture;

        id<MTLRenderCommandEncoder> tessEncoder =
            [commandBuffer renderCommandEncoderWithDescriptor:tessPass];
        [tessEncoder
            setViewport:make_viewport(
                            0, 0, kTessTextureWidth, desc.tessDataHeight)];
        [tessEncoder setRenderPipelineState:m_tessPipeline->pipelineState()];
        [tessEncoder setVertexTexture:m_featherTexture
                              atIndex:FEATHER_TEXTURE_IDX];
        [tessEncoder
            setVertexBuffer:mtl_buffer(flushUniformBufferRing())
                     offset:desc.flushUniformDataOffsetInBytes
                    atIndex:METAL_BUFFER_IDX(FLUSH_UNIFORM_BUFFER_IDX)];
        [tessEncoder setVertexBuffer:mtl_buffer(tessSpanBufferRing())
                              offset:desc.firstTessVertexSpan *
                                     sizeof(gpu::TessVertexSpan)
                             atIndex:0];
        assert(desc.pathCount > 0);
        [tessEncoder setVertexBuffer:mtl_buffer(pathBufferRing())
                              offset:desc.firstPath * sizeof(gpu::PathData)
                             atIndex:METAL_BUFFER_IDX(PATH_BUFFER_IDX)];
        assert(desc.contourCount > 0);
        [tessEncoder
            setVertexBuffer:mtl_buffer(contourBufferRing())
                     offset:desc.firstContour * sizeof(gpu::ContourData)
                    atIndex:METAL_BUFFER_IDX(CONTOUR_BUFFER_IDX)];
        [tessEncoder setCullMode:MTLCullModeBack];
        [tessEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                indexCount:std::size(gpu::kTessSpanIndices)
                                 indexType:MTLIndexTypeUInt16
                               indexBuffer:m_tessSpanIndexBuffer
                         indexBufferOffset:0
                             instanceCount:desc.tessVertexSpanCount];
        [tessEncoder endEncoding];
    }

    // Render the atlas if we have any offscreen feathers.
    if ((desc.atlasFillBatchCount | desc.atlasStrokeBatchCount) != 0)
    {
        MTLRenderPassDescriptor* atlasPass =
            [MTLRenderPassDescriptor renderPassDescriptor];
        atlasPass.renderTargetWidth = desc.atlasContentWidth;
        atlasPass.renderTargetHeight = desc.atlasContentHeight;
        atlasPass.colorAttachments[0].loadAction = MTLLoadActionClear;
        atlasPass.colorAttachments[0].storeAction = MTLStoreActionStore;
        atlasPass.colorAttachments[0].texture = m_atlasTexture;
        atlasPass.colorAttachments[0].clearColor =
            MTLClearColorMake(0, 0, 0, 0);

        id<MTLRenderCommandEncoder> atlasEncoder =
            [commandBuffer renderCommandEncoderWithDescriptor:atlasPass];
        [atlasEncoder setViewport:make_viewport(0,
                                                0,
                                                desc.atlasContentWidth,
                                                desc.atlasContentHeight)];
        [atlasEncoder
            setVertexBuffer:mtl_buffer(flushUniformBufferRing())
                     offset:desc.flushUniformDataOffsetInBytes
                    atIndex:METAL_BUFFER_IDX(FLUSH_UNIFORM_BUFFER_IDX)];
        [atlasEncoder
            setFragmentBuffer:mtl_buffer(flushUniformBufferRing())
                       offset:desc.flushUniformDataOffsetInBytes
                      atIndex:METAL_BUFFER_IDX(FLUSH_UNIFORM_BUFFER_IDX)];
        [atlasEncoder setVertexTexture:m_tessVertexTexture
                               atIndex:TESS_VERTEX_TEXTURE_IDX];
        [atlasEncoder setVertexTexture:m_featherTexture
                               atIndex:FEATHER_TEXTURE_IDX];
        [atlasEncoder setFragmentTexture:m_gradientTexture
                                 atIndex:GRAD_TEXTURE_IDX];
        [atlasEncoder setFragmentTexture:m_featherTexture
                                 atIndex:FEATHER_TEXTURE_IDX];
        if (desc.pathCount > 0)
        {
            [atlasEncoder setVertexBuffer:mtl_buffer(pathBufferRing())
                                   offset:desc.firstPath * sizeof(gpu::PathData)
                                  atIndex:METAL_BUFFER_IDX(PATH_BUFFER_IDX)];
            [atlasEncoder
                setVertexBuffer:mtl_buffer(paintBufferRing())
                         offset:desc.firstPaint * sizeof(gpu::PaintData)
                        atIndex:METAL_BUFFER_IDX(PAINT_BUFFER_IDX)];
            [atlasEncoder
                setVertexBuffer:mtl_buffer(paintAuxBufferRing())
                         offset:desc.firstPaintAux * sizeof(gpu::PaintAuxData)
                        atIndex:METAL_BUFFER_IDX(PAINT_AUX_BUFFER_IDX)];
        }
        if (desc.contourCount > 0)
        {
            [atlasEncoder
                setVertexBuffer:mtl_buffer(contourBufferRing())
                         offset:desc.firstContour * sizeof(gpu::ContourData)
                        atIndex:METAL_BUFFER_IDX(CONTOUR_BUFFER_IDX)];
        }
        [atlasEncoder setVertexBuffer:m_pathPatchVertexBuffer
                               offset:0
                              atIndex:0];
        [atlasEncoder setCullMode:MTLCullModeBack];

        if (desc.atlasFillBatchCount != 0)
        {
            [atlasEncoder
                setRenderPipelineState:m_atlasFillPipeline->pipelineState()];
            for (size_t i = 0; i < desc.atlasFillBatchCount; ++i)
            {
                const gpu::AtlasDrawBatch& fillBatch = desc.atlasFillBatches[i];
                [atlasEncoder setScissorRect:make_scissor(fillBatch.scissor)];
                [atlasEncoder
                    setVertexBytes:&fillBatch.basePatch
                            length:sizeof(uint32_t)
                           atIndex:METAL_BUFFER_IDX(
                                       PATH_BASE_INSTANCE_UNIFORM_BUFFER_IDX)];
                [atlasEncoder
                    drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                               indexCount:
                                   gpu::kMidpointFanCenterAAPatchIndexCount
                                indexType:MTLIndexTypeUInt16
                              indexBuffer:m_pathPatchIndexBuffer
                        indexBufferOffset:
                            gpu::kMidpointFanCenterAAPatchBaseIndex *
                            sizeof(uint16_t)
                            instanceCount:fillBatch.patchCount];
            }
        }

        if (desc.atlasStrokeBatchCount != 0)
        {
            [atlasEncoder
                setRenderPipelineState:m_atlasStrokePipeline->pipelineState()];
            for (size_t i = 0; i < desc.atlasStrokeBatchCount; ++i)
            {
                const gpu::AtlasDrawBatch& strokeBatch =
                    desc.atlasStrokeBatches[i];
                [atlasEncoder setScissorRect:make_scissor(strokeBatch.scissor)];
                [atlasEncoder
                    setVertexBytes:&strokeBatch.basePatch
                            length:sizeof(uint32_t)
                           atIndex:METAL_BUFFER_IDX(
                                       PATH_BASE_INSTANCE_UNIFORM_BUFFER_IDX)];
                [atlasEncoder
                    drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                               indexCount:gpu::kMidpointFanPatchBorderIndexCount
                                indexType:MTLIndexTypeUInt16
                              indexBuffer:m_pathPatchIndexBuffer
                        indexBufferOffset:gpu::kMidpointFanPatchBaseIndex *
                                          sizeof(uint16_t)
                            instanceCount:strokeBatch.patchCount];
            }
        }

        [atlasEncoder endEncoding];
    }

    // Generate mipmaps if needed.
    for (const DrawBatch& batch : *desc.drawList)
    {
        if (auto imageTextureMetal =
                static_cast<const TextureMetalImpl*>(batch.imageTexture))
        {
            imageTextureMetal->ensureMipmaps(commandBuffer);
        }
    }

    // Set up a render pass to do the final rendering using (some form of) pixel
    // local storage.
    MTLRenderPassDescriptor* pass =
        [MTLRenderPassDescriptor renderPassDescriptor];
    pass.renderTargetWidth = desc.renderTargetUpdateBounds.right;
    pass.renderTargetHeight = desc.renderTargetUpdateBounds.bottom;
    pass.colorAttachments[COLOR_PLANE_IDX].texture =
        renderTarget->targetTexture();
    switch (desc.colorLoadAction)
    {
        case gpu::LoadAction::clear:
        {
            float cc[4];
            UnpackColorToRGBA32FPremul(desc.colorClearValue, cc);
            pass.colorAttachments[COLOR_PLANE_IDX].loadAction =
                MTLLoadActionClear;
            pass.colorAttachments[COLOR_PLANE_IDX].clearColor =
                MTLClearColorMake(cc[0], cc[1], cc[2], cc[3]);
            break;
        }
        case gpu::LoadAction::preserveRenderTarget:
            pass.colorAttachments[COLOR_PLANE_IDX].loadAction =
                MTLLoadActionLoad;
            break;
        case gpu::LoadAction::dontCare:
            pass.colorAttachments[COLOR_PLANE_IDX].loadAction =
                MTLLoadActionDontCare;
            break;
    }
    pass.colorAttachments[COLOR_PLANE_IDX].storeAction = MTLStoreActionStore;

    auto baselineShaderMiscFlags = gpu::ShaderMiscFlags::none;
    if (desc.interlockMode == gpu::InterlockMode::rasterOrdering)
    {
        // In rasterOrdering mode, the PLS planes are accessed as color
        // attachments.
        pass.colorAttachments[CLIP_PLANE_IDX].texture =
            renderTarget->m_clipMemorylessTexture;
        pass.colorAttachments[CLIP_PLANE_IDX].loadAction = MTLLoadActionClear;
        pass.colorAttachments[CLIP_PLANE_IDX].clearColor =
            MTLClearColorMake(0, 0, 0, 0);
        pass.colorAttachments[CLIP_PLANE_IDX].storeAction =
            MTLStoreActionDontCare;

        pass.colorAttachments[SCRATCH_COLOR_PLANE_IDX].texture =
            renderTarget->m_scratchColorMemorylessTexture;
        pass.colorAttachments[SCRATCH_COLOR_PLANE_IDX].loadAction =
            MTLLoadActionDontCare;
        pass.colorAttachments[SCRATCH_COLOR_PLANE_IDX].storeAction =
            MTLStoreActionDontCare;

        pass.colorAttachments[COVERAGE_PLANE_IDX].texture =
            renderTarget->m_coverageMemorylessTexture;
        pass.colorAttachments[COVERAGE_PLANE_IDX].loadAction =
            MTLLoadActionClear;
        pass.colorAttachments[COVERAGE_PLANE_IDX].clearColor =
            MTLClearColorMake(desc.coverageClearValue, 0, 0, 0);
        pass.colorAttachments[COVERAGE_PLANE_IDX].storeAction =
            MTLStoreActionDontCare;
    }
    else if (desc.atomicFixedFunctionColorOutput)
    {
        assert(desc.interlockMode == gpu::InterlockMode::atomics);
        baselineShaderMiscFlags |=
            gpu::ShaderMiscFlags::fixedFunctionColorOutput;
    }
    else if (desc.colorLoadAction == gpu::LoadAction::preserveRenderTarget)
    {
        // Since we need to preserve the renderTarget during load, and since
        // we're rendering to an offscreen color buffer, we have to literally
        // copy the renderTarget into the color buffer.
        assert(desc.interlockMode == gpu::InterlockMode::atomics);
        id<MTLBlitCommandEncoder> copyEncoder =
            [commandBuffer blitCommandEncoder];
        auto updateOrigin = MTLOriginMake(desc.renderTargetUpdateBounds.left,
                                          desc.renderTargetUpdateBounds.top,
                                          0);
        auto updateSize = MTLSizeMake(desc.renderTargetUpdateBounds.width(),
                                      desc.renderTargetUpdateBounds.height(),
                                      1);
        [copyEncoder copyFromTexture:renderTarget->targetTexture()
                         sourceSlice:0
                         sourceLevel:0
                        sourceOrigin:updateOrigin
                          sourceSize:updateSize
                            toBuffer:renderTarget->colorAtomicBuffer()
                   destinationOffset:(updateOrigin.y * renderTarget->width() +
                                      updateOrigin.x) *
                                     sizeof(uint32_t)
              destinationBytesPerRow:renderTarget->width() * sizeof(uint32_t)
            destinationBytesPerImage:renderTarget->height() *
                                     renderTarget->width() * sizeof(uint32_t)];
        [copyEncoder endEncoding];
    }

    // Execute the DrawList.
    id<MTLRenderCommandEncoder> encoder = makeRenderPassForDraws(
        desc, pass, commandBuffer, baselineShaderMiscFlags);
    for (const DrawBatch& batch : *desc.drawList)
    {
        // Setup the pipeline for this specific drawType and shaderFeatures.
        gpu::ShaderFeatures shaderFeatures;
        if (desc.interlockMode == gpu::InterlockMode::atomics)
        {
            // The combined shader features might have more flags set than are
            // actually relevant for this draw type, so filter them out.
            shaderFeatures =
                desc.combinedShaderFeatures &
                gpu::ShaderFeaturesMaskFor(batch.drawType, desc.interlockMode);
        }
        else
        {
            shaderFeatures = batch.shaderFeatures;
        }

        gpu::ShaderMiscFlags batchMiscFlags =
            baselineShaderMiscFlags | batch.shaderMiscFlags;
        if (desc.interlockMode == gpu::InterlockMode::rasterOrdering &&
            (batch.drawContents & gpu::DrawContents::clockwiseFill))
        {
            batchMiscFlags |= gpu::ShaderMiscFlags::clockwiseFill;
        }
        if (!(batchMiscFlags & gpu::ShaderMiscFlags::fixedFunctionColorOutput))
        {
            if (batch.drawType == gpu::DrawType::atomicResolve)
            {
                // Atomic mode can always do a coalesced resolve when rendering
                // to an offscreen color buffer.
                batchMiscFlags |=
                    gpu::ShaderMiscFlags::coalescedResolveAndTransfer;
            }
            else if (batch.drawType == gpu::DrawType::atomicInitialize)
            {
                if (desc.colorLoadAction == gpu::LoadAction::clear)
                {
                    batchMiscFlags |= gpu::ShaderMiscFlags::storeColorClear;
                }
                else if (desc.colorLoadAction ==
                             gpu::LoadAction::preserveRenderTarget &&
                         renderTarget->pixelFormat() ==
                             MTLPixelFormatBGRA8Unorm)
                {
                    // We already copied the renderTarget to our color buffer,
                    // but since the target is BGRA, we also need to swizzle it
                    // to RGBA before it's ready for PLS.
                    batchMiscFlags |=
                        gpu::ShaderMiscFlags::swizzleColorBGRAToRGBA;
                }
            }
        }
        const DrawPipeline* drawPipeline = findCompatibleDrawPipeline(
            batch.drawType, shaderFeatures, desc, batchMiscFlags);
        if (drawPipeline == nullptr || !drawPipeline->valid())
        {
            // The shader for this draw AND the uber-shader both failed to
            // compile. This should virtually never happen, and can only happen
            // on non-Apple Silicon, where we don't use precompiled shaders.
            // Skip the draw.
            continue;
        }
        id<MTLRenderPipelineState> drawPipelineState =
            drawPipeline->pipelineState(renderTarget->pixelFormat());

        // Bind the appropriate image texture, if any.
        if (auto imageTextureMetal =
                static_cast<const TextureMetalImpl*>(batch.imageTexture))
        {
            [encoder setFragmentTexture:imageTextureMetal->texture()
                                atIndex:IMAGE_TEXTURE_IDX];

            [encoder setFragmentSamplerState:m_imageSamplers[batch.imageSampler
                                                                 .asKey()]
                                     atIndex:IMAGE_SAMPLER_IDX];
        }
        else
        {
            [encoder setFragmentSamplerState:
                         m_imageSamplers[ImageSampler::LINEAR_CLAMP_SAMPLER_KEY]
                                     atIndex:IMAGE_SAMPLER_IDX];
        }

        // Issue any barriers if needed.
        if (batch.barriers &
            (BarrierFlags::plsAtomic | BarrierFlags::plsAtomicPostInit |
             BarrierFlags::plsAtomicPreResolve))
        {
            assert(desc.interlockMode == gpu::InterlockMode::atomics);
            switch (m_metalFeatures.atomicBarrierType)
            {
                case AtomicBarrierType::memoryBarrier:
#if defined(RIVE_MACOSX)
                    if (@available(macOS 10.14, *))
                    {
                        [encoder
                            memoryBarrierWithScope:MTLBarrierScopeBuffers |
                                                   MTLBarrierScopeRenderTargets
                                       afterStages:MTLRenderStageFragment
                                      beforeStages:MTLRenderStageFragment];
                        break;
                    }
#endif
                    // atomicBarrierType shouldn't be "memoryBarrier" in this
                    // case.
                    RIVE_UNREACHABLE();

                case AtomicBarrierType::rasterOrderGroup:
                    break;

                case AtomicBarrierType::renderPassBreak:
                    // On very old hardware that can't support barriers, we just
                    // take a sledge hammer and break the entire render pass
                    // between overlapping draws.
                    // TODO: Is there a lighter way to achieve this?
                    [encoder endEncoding];
                    pass.colorAttachments[COLOR_PLANE_IDX].loadAction =
                        MTLLoadActionLoad;
                    encoder = makeRenderPassForDraws(
                        desc, pass, commandBuffer, baselineShaderMiscFlags);
                    break;
            }
        }

        DrawType drawType = batch.drawType;
        switch (drawType)
        {
            case DrawType::midpointFanPatches:
            case DrawType::midpointFanCenterAAPatches:
            case DrawType::outerCurvePatches:
            {
                // Draw PLS patches that connect the tessellation vertices.
                [encoder setRenderPipelineState:drawPipelineState];
                [encoder setVertexBuffer:m_pathPatchVertexBuffer
                                  offset:0
                                 atIndex:0];
                [encoder setCullMode:MTLCullModeBack];
                // Don't use baseInstance in order to run on Apple GPU Family 2.
                // TODO: Use baseInstance instead once we deprecate Apple2.
                [encoder
                    setVertexBytes:&batch.baseElement
                            length:sizeof(uint32_t)
                           atIndex:METAL_BUFFER_IDX(
                                       PATH_BASE_INSTANCE_UNIFORM_BUFFER_IDX)];
                [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                    indexCount:gpu::PatchIndexCount(drawType)
                                     indexType:MTLIndexTypeUInt16
                                   indexBuffer:m_pathPatchIndexBuffer
                             indexBufferOffset:gpu::PatchBaseIndex(drawType) *
                                               sizeof(uint16_t)
                                 instanceCount:batch.elementCount];
                break;
            }
            case DrawType::interiorTriangulation:
            case DrawType::atlasBlit:
            {
                [encoder setRenderPipelineState:drawPipelineState];
                [encoder setVertexBuffer:mtl_buffer(triangleBufferRing())
                                  offset:0
                                 atIndex:0];
                [encoder setCullMode:MTLCullModeBack];
                [encoder drawPrimitives:MTLPrimitiveTypeTriangle
                            vertexStart:batch.baseElement
                            vertexCount:batch.elementCount];
                break;
            }
            case DrawType::imageRect:
            case DrawType::imageMesh:
            {
                [encoder setRenderPipelineState:drawPipelineState];
                [encoder
                    setVertexBuffer:mtl_buffer(imageDrawUniformBufferRing())
                             offset:batch.imageDrawDataOffset
                            atIndex:METAL_BUFFER_IDX(
                                        IMAGE_DRAW_UNIFORM_BUFFER_IDX)];
                [encoder
                    setFragmentBuffer:mtl_buffer(imageDrawUniformBufferRing())
                               offset:batch.imageDrawDataOffset
                              atIndex:METAL_BUFFER_IDX(
                                          IMAGE_DRAW_UNIFORM_BUFFER_IDX)];
                [encoder setCullMode:MTLCullModeNone];
                if (drawType == DrawType::imageRect)
                {
                    assert(desc.interlockMode == gpu::InterlockMode::atomics);
                    [encoder setVertexBuffer:m_imageRectVertexBuffer
                                      offset:0
                                     atIndex:0];
                    [encoder
                        drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                   indexCount:std::size(gpu::kImageRectIndices)
                                    indexType:MTLIndexTypeUInt16
                                  indexBuffer:m_imageRectIndexBuffer
                            indexBufferOffset:0];
                }
                else
                {
                    LITE_RTTI_CAST_OR_BREAK(vertexBuffer,
                                            RenderBufferMetalImpl*,
                                            batch.vertexBuffer);
                    LITE_RTTI_CAST_OR_BREAK(
                        uvBuffer, RenderBufferMetalImpl*, batch.uvBuffer);
                    LITE_RTTI_CAST_OR_BREAK(
                        indexBuffer, RenderBufferMetalImpl*, batch.indexBuffer);
                    [encoder setVertexBuffer:vertexBuffer->submittedBuffer()
                                      offset:0
                                     atIndex:0];
                    [encoder setVertexBuffer:uvBuffer->submittedBuffer()
                                      offset:0
                                     atIndex:1];
                    [encoder
                        drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                   indexCount:batch.elementCount
                                    indexType:MTLIndexTypeUInt16
                                  indexBuffer:indexBuffer->submittedBuffer()
                            indexBufferOffset:batch.baseElement *
                                              sizeof(uint16_t)];
                }
                break;
            }
            case DrawType::atomicInitialize:
            case DrawType::atomicResolve:
            {
                assert(desc.interlockMode == gpu::InterlockMode::atomics);
                [encoder setRenderPipelineState:drawPipelineState];
                [encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip
                            vertexStart:0
                            vertexCount:4];
                break;
            }
            case DrawType::msaaStrokes:
            case DrawType::msaaMidpointFanBorrowedCoverage:
            case DrawType::msaaMidpointFans:
            case DrawType::msaaMidpointFanStencilReset:
            case DrawType::msaaMidpointFanPathsStencil:
            case DrawType::msaaMidpointFanPathsCover:
            case DrawType::msaaOuterCubics:
            case DrawType::msaaStencilClipReset:
            {
                RIVE_UNREACHABLE();
            }
        }
    }
    [encoder endEncoding];
}

void RenderContextMetalImpl::postFlush(
    const RenderContext::FlushResources& flushResources)
{
    // Schedule a callback that will unlock the buffers used by this flush,
    // after the GPU has finished rendering with them. This unblocks the CPU
    // from reusing them in a future flush.
    id<MTLCommandBuffer> commandBuffer =
        (__bridge id<MTLCommandBuffer>)flushResources.externalCommandBuffer;
    std::mutex& thisFlushLock = m_bufferRingLocks[m_bufferRingIdx];
    [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer>) {
      assert(!thisFlushLock.try_lock()); // The mutex should already be locked.
      thisFlushLock.unlock();
    }];
}
} // namespace rive::gpu
