/*
 * Copyright 2023 Rive
 */

#include "rive/pls/metal/pls_render_context_metal_impl.h"

#include "background_shader_compiler.h"
#include "rive/pls/buffer_ring.hpp"
#include "rive/pls/pls_image.hpp"
#include "shaders/constants.glsl"
#include <sstream>

#include "../out/obj/generated/color_ramp.exports.h"
#include "../out/obj/generated/tessellate.exports.h"

namespace rive::pls
{
#ifdef RIVE_IOS
#include "../out/obj/generated/rive_pls_ios.metallib.c"
#elif defined(RIVE_IOS_SIMULATOR)
#include "../out/obj/generated/rive_pls_ios_simulator.metallib.c"
#else
#include "../out/obj/generated/rive_pls_macosx.metallib.c"
#endif

static id<MTLRenderPipelineState> make_pipeline_state(id<MTLDevice> gpu,
                                                      MTLRenderPipelineDescriptor* desc)
{
    NSError* err = [NSError errorWithDomain:@"pls_pipeline_create" code:201 userInfo:nil];
    id<MTLRenderPipelineState> state = [gpu newRenderPipelineStateWithDescriptor:desc error:&err];
    if (!state)
    {
        fprintf(stderr, "%s\n", err.localizedDescription.UTF8String);
        exit(-1);
    }
    return state;
}

// Renders color ramps to the gradient texture.
class PLSRenderContextMetalImpl::ColorRampPipeline
{
public:
    ColorRampPipeline(id<MTLDevice> gpu, id<MTLLibrary> plsLibrary)
    {
        MTLRenderPipelineDescriptor* desc = [[MTLRenderPipelineDescriptor alloc] init];
        desc.vertexFunction = [plsLibrary newFunctionWithName:@GLSL_colorRampVertexMain];
        desc.fragmentFunction = [plsLibrary newFunctionWithName:@GLSL_colorRampFragmentMain];
        desc.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
        m_pipelineState = make_pipeline_state(gpu, desc);
    }

    id<MTLRenderPipelineState> pipelineState() const { return m_pipelineState; }

private:
    id<MTLRenderPipelineState> m_pipelineState;
};

// Renders tessellated vertices to the tessellation texture.
class PLSRenderContextMetalImpl::TessellatePipeline
{
public:
    TessellatePipeline(id<MTLDevice> gpu, id<MTLLibrary> plsLibrary)
    {
        MTLRenderPipelineDescriptor* desc = [[MTLRenderPipelineDescriptor alloc] init];
        desc.vertexFunction = [plsLibrary newFunctionWithName:@GLSL_tessellateVertexMain];
        desc.fragmentFunction = [plsLibrary newFunctionWithName:@GLSL_tessellateFragmentMain];
        desc.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA32Uint;
        m_pipelineState = make_pipeline_state(gpu, desc);
    }

    id<MTLRenderPipelineState> pipelineState() const { return m_pipelineState; }

private:
    id<MTLRenderPipelineState> m_pipelineState;
};

// Renders paths to the main render target.
class PLSRenderContextMetalImpl::DrawPipeline
{
public:
    // Precompiled functions are embedded in namespaces. Return the fully qualified name of the
    // desired function, including its namespace.
    static NSString* GetPrecompiledFunctionName(DrawType drawType,
                                                ShaderFeatures shaderFeatures,
                                                id<MTLLibrary> precompiledLibrary,
                                                const char* functionBaseName)
    {
        // Each feature corresponds to a specific index in the namespaceID. These must stay in
        // sync with generate_draw_combinations.py.
        char namespaceID[] = "0000000";
        if (drawType == DrawType::interiorTriangulation)
        {
            namespaceID[0] = '1';
        }
        for (size_t i = 0; i < pls::kShaderFeatureCount; ++i)
        {
            ShaderFeatures feature = static_cast<ShaderFeatures>(1 << i);
            if (shaderFeatures & feature)
            {
                namespaceID[i + 1] = '1';
            }
            static_assert((int)ShaderFeatures::ENABLE_CLIPPING == 1 << 0);
            static_assert((int)ShaderFeatures::ENABLE_CLIP_RECT == 1 << 1);
            static_assert((int)ShaderFeatures::ENABLE_ADVANCED_BLEND == 1 << 2);
            static_assert((int)ShaderFeatures::ENABLE_EVEN_ODD == 1 << 3);
            static_assert((int)ShaderFeatures::ENABLE_NESTED_CLIPPING == 1 << 4);
            static_assert((int)ShaderFeatures::ENABLE_HSL_BLEND_MODES == 1 << 5);
        }

        char namespacePrefix;
        switch (drawType)
        {
            case DrawType::midpointFanPatches:
            case DrawType::outerCurvePatches:
            case DrawType::interiorTriangulation:
                namespacePrefix = 'p';
                break;
            case DrawType::imageMesh:
                namespacePrefix = 'm';
                break;
        }

        return
            [NSString stringWithFormat:@"%c%s::%s", namespacePrefix, namespaceID, functionBaseName];
    }

    DrawPipeline(id<MTLDevice> gpu,
                 id<MTLLibrary> library,
                 NSString* vertexFunctionName,
                 NSString* fragmentFunctionName)
    {
        constexpr static auto makePipelineState = [](id<MTLDevice> gpu,
                                                     id<MTLFunction> vertexMain,
                                                     id<MTLFunction> fragmentMain,
                                                     MTLPixelFormat pixelFormat) {
            MTLRenderPipelineDescriptor* desc = [[MTLRenderPipelineDescriptor alloc] init];
            desc.vertexFunction = vertexMain;
            desc.fragmentFunction = fragmentMain;
            desc.colorAttachments[0].pixelFormat = pixelFormat;
            desc.colorAttachments[1].pixelFormat = MTLPixelFormatR32Uint;
            desc.colorAttachments[2].pixelFormat = pixelFormat;
            desc.colorAttachments[3].pixelFormat = MTLPixelFormatR32Uint;
            return make_pipeline_state(gpu, desc);
        };
        id<MTLFunction> vertexMain = [library newFunctionWithName:vertexFunctionName];
        id<MTLFunction> fragmentMain = [library newFunctionWithName:fragmentFunctionName];
        m_pipelineStateRGBA8 =
            makePipelineState(gpu, vertexMain, fragmentMain, MTLPixelFormatRGBA8Unorm);
        m_pipelineStateBGRA8 =
            makePipelineState(gpu, vertexMain, fragmentMain, MTLPixelFormatBGRA8Unorm);
    }

    id<MTLRenderPipelineState> pipelineState(MTLPixelFormat pixelFormat) const
    {
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
    id<MTLRenderPipelineState> m_pipelineStateRGBA8;
    id<MTLRenderPipelineState> m_pipelineStateBGRA8;
};

#ifdef RIVE_IOS
static bool is_apple_ios_silicon(id<MTLDevice> gpu)
{
    if (@available(iOS 13, *))
    {
        return [gpu supportsFamily:MTLGPUFamilyApple4];
    }
    return false;
}
#endif

class BufferRingMetalImpl : public BufferRing
{
public:
    BufferRingMetalImpl(id<MTLDevice> gpu, size_t capacity, size_t itemSizeInBytes) :
        BufferRing(capacity, itemSizeInBytes)
    {
        for (int i = 0; i < kBufferRingSize; ++i)
        {
            m_buffers[i] = [gpu newBufferWithLength:capacity * itemSizeInBytes
                                            options:MTLResourceStorageModeShared];
        }
    }

    id<MTLBuffer> submittedBuffer() const { return m_buffers[submittedBufferIdx()]; }

protected:
    void* onMapBuffer(int bufferIdx) override { return m_buffers[bufferIdx].contents; }
    void onUnmapAndSubmitBuffer(int bufferIdx, size_t bytesWritten) override {}

private:
    id<MTLBuffer> m_buffers[kBufferRingSize];
};

class TexelBufferMetalImpl : public TexelBufferRing
{
public:
    TexelBufferMetalImpl(id<MTLDevice> gpu,
                         Format format,
                         size_t widthInItems,
                         size_t height,
                         size_t texelsPerItem,
                         MTLTextureUsage extraUsageFlags = 0) :
        TexelBufferRing(format, widthInItems, height, texelsPerItem)
    {
        MTLTextureDescriptor* desc = [[MTLTextureDescriptor alloc] init];
        switch (format)
        {
            case Format::rgba8:
                desc.pixelFormat = MTLPixelFormatRGBA8Unorm;
                break;
            case Format::rgba32f:
                desc.pixelFormat = MTLPixelFormatRGBA32Float;
                break;
            case Format::rgba32ui:
                desc.pixelFormat = MTLPixelFormatRGBA32Uint;
                break;
        }
        desc.width = widthInItems * texelsPerItem;
        desc.height = height;
        desc.mipmapLevelCount = 1;
        desc.usage = MTLTextureUsageShaderRead | extraUsageFlags;
        desc.storageMode = MTLStorageModeShared;
        desc.textureType = MTLTextureType2D;
        for (int i = 0; i < kBufferRingSize; ++i)
        {
            m_textures[i] = [gpu newTextureWithDescriptor:desc];
        }
    }

    id<MTLTexture> submittedTexture() const { return m_textures[submittedBufferIdx()]; }

protected:
    void submitTexels(int textureIdx, size_t updateWidthInTexels, size_t updateHeight) override
    {
        if (updateWidthInTexels > 0 && updateHeight > 0)
        {
            MTLRegion region = {MTLOriginMake(0, 0, 0),
                                MTLSizeMake(updateWidthInTexels, updateHeight, 1)};
            [m_textures[textureIdx] replaceRegion:region
                                      mipmapLevel:0
                                        withBytes:shadowBuffer()
                                      bytesPerRow:widthInTexels() * BytesPerPixel(m_format)];
        }
    }

private:
    id<MTLTexture> m_textures[kBufferRingSize];
};

std::unique_ptr<PLSRenderContext> PLSRenderContextMetalImpl::MakeContext(id<MTLDevice> gpu,
                                                                         id<MTLCommandQueue> queue)
{
    if (![gpu supportsFamily:MTLGPUFamilyApple1])
    {
        fprintf(stderr, "error: GPU is not Apple family.");
        return nullptr;
    }
    auto plsContextImpl =
        std::unique_ptr<PLSRenderContextMetalImpl>(new PLSRenderContextMetalImpl(gpu, queue));
    return std::make_unique<PLSRenderContext>(std::move(plsContextImpl));
}

PLSRenderContextMetalImpl::PLSRenderContextMetalImpl(id<MTLDevice> gpu, id<MTLCommandQueue> queue) :
    m_gpu(gpu),
    m_queue(queue),
    m_backgroundShaderCompiler(std::make_unique<BackgroundShaderCompiler>(m_gpu))
{
#ifdef RIVE_IOS
    if (!is_apple_ios_silicon(gpu))
    {
        // The PowerVR GPU, at least on A10, has fp16 precision issues. We can't use the the bottom
        // 3 bits of the path and clip IDs in order for our equality testing to work.
        m_platformFeatures.pathIDGranularity = 8;
    }
#endif
    // It appears, so far, that we don't need to use flat interpolation for path IDs on any Apple
    // device, and it's faster not to.
    m_platformFeatures.avoidFlatVaryings = true;
    m_platformFeatures.invertOffscreenY = true;

    // Load the precompiled shaders.
    dispatch_data_t metallibData = dispatch_data_create(
#ifdef RIVE_IOS
        rive_pls_ios_metallib,
        rive_pls_ios_metallib_len,
#elif defined(RIVE_IOS_SIMULATOR)
        rive_pls_ios_simulator_metallib,
        rive_pls_ios_simulator_metallib_len,
#else
        rive_pls_macosx_metallib,
        rive_pls_macosx_metallib_len,
#endif
        nil,
        nil);
    NSError* err = [NSError errorWithDomain:@"pls_metallib_load" code:200 userInfo:nil];
    m_plsPrecompiledLibrary = [m_gpu newLibraryWithData:metallibData error:&err];
    if (m_plsPrecompiledLibrary == nil)
    {
        fprintf(stderr, "Failed to load pls metallib.\n");
        fprintf(stderr, "%s\n", err.localizedDescription.UTF8String);
        exit(-1);
    }

    m_colorRampPipeline = std::make_unique<ColorRampPipeline>(gpu, m_plsPrecompiledLibrary);
    m_tessPipeline = std::make_unique<TessellatePipeline>(gpu, m_plsPrecompiledLibrary);
    m_tessSpanIndexBuffer = [gpu newBufferWithBytes:pls::kTessSpanIndices
                                             length:sizeof(pls::kTessSpanIndices)
                                            options:MTLResourceStorageModeShared];

    // Load the fully-featured, pre-compiled draw shaders.
    for (auto drawType :
         {DrawType::midpointFanPatches, DrawType::interiorTriangulation, DrawType::imageMesh})
    {
        pls::ShaderFeatures allShaderFeatures = pls::AllShaderFeaturesForDrawType(drawType);
        uint32_t pipelineKey = ShaderUniqueKey(drawType, allShaderFeatures);
        m_drawPipelines[pipelineKey] = std::make_unique<DrawPipeline>(
            m_gpu,
            m_plsPrecompiledLibrary,
            DrawPipeline::GetPrecompiledFunctionName(drawType,
                                                     allShaderFeatures &
                                                         pls::kVertexShaderFeaturesMask,
                                                     m_plsPrecompiledLibrary,
                                                     GLSL_drawVertexMain),
            DrawPipeline::GetPrecompiledFunctionName(
                drawType, allShaderFeatures, m_plsPrecompiledLibrary, GLSL_drawFragmentMain));
    }

    // Create vertex and index buffers for the different PLS patches.
    m_pathPatchVertexBuffer = [gpu newBufferWithLength:kPatchVertexBufferCount * sizeof(PatchVertex)
                                               options:MTLResourceStorageModeShared];
    m_pathPatchIndexBuffer = [gpu newBufferWithLength:kPatchIndexBufferCount * sizeof(uint16_t)
                                              options:MTLResourceStorageModeShared];
    GeneratePatchBufferData(reinterpret_cast<PatchVertex*>(m_pathPatchVertexBuffer.contents),
                            reinterpret_cast<uint16_t*>(m_pathPatchIndexBuffer.contents));
}

PLSRenderContextMetalImpl::~PLSRenderContextMetalImpl() {}

// All PLS planes besides the main framebuffer can exist in ephemeral "memoryless" storage. This
// means their contents are never actually written to main memory, and they only exist in fast tiled
// memory.
static id<MTLTexture> make_memoryless_pls_texture(id<MTLDevice> gpu,
                                                  MTLPixelFormat pixelFormat,
                                                  size_t width,
                                                  size_t height)
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

PLSRenderTargetMetal::PLSRenderTargetMetal(id<MTLDevice> gpu,
                                           MTLPixelFormat pixelFormat,
                                           size_t width,
                                           size_t height,
                                           const PlatformFeatures& platformFeatures) :
    PLSRenderTarget(width, height), m_pixelFormat(pixelFormat)
{
    m_targetTexture = nil; // Will be configured later by setTargetTexture().
    m_coverageMemorylessTexture =
        make_memoryless_pls_texture(gpu, MTLPixelFormatR32Uint, width, height);
    m_originalDstColorMemorylessTexture =
        make_memoryless_pls_texture(gpu, m_pixelFormat, width, height);
    m_clipMemorylessTexture =
        make_memoryless_pls_texture(gpu, MTLPixelFormatR32Uint, width, height);
}

void PLSRenderTargetMetal::setTargetTexture(id<MTLTexture> texture)
{
    assert(texture.width == width());
    assert(texture.height == height());
    assert(texture.pixelFormat == m_pixelFormat);
    m_targetTexture = texture;
}

rcp<PLSRenderTargetMetal> PLSRenderContextMetalImpl::makeRenderTarget(MTLPixelFormat pixelFormat,
                                                                      size_t width,
                                                                      size_t height)
{
    return rcp(new PLSRenderTargetMetal(m_gpu, pixelFormat, width, height, m_platformFeatures));
}

class RenderBufferMetalImpl : public lite_rtti_override<RenderBuffer, RenderBufferMetalImpl>
{
public:
    RenderBufferMetalImpl(RenderBufferType renderBufferType,
                          RenderBufferFlags renderBufferFlags,
                          size_t sizeInBytes,
                          id<MTLDevice> gpu) :
        lite_rtti_override(renderBufferType, renderBufferFlags, sizeInBytes), m_gpu(gpu)
    {
        int bufferCount =
            flags() & RenderBufferFlags::mappedOnceAtInitialization ? 1 : pls::kBufferRingSize;
        for (int i = 0; i < bufferCount; ++i)
        {
            m_buffers[i] = [gpu newBufferWithLength:sizeInBytes
                                            options:MTLResourceStorageModeShared];
        }
    }

    id<MTLBuffer> submittedBuffer() const { return m_buffers[m_submittedBufferIdx]; }

protected:
    void* onMap() override
    {
        m_submittedBufferIdx = (m_submittedBufferIdx + 1) % pls::kBufferRingSize;
        assert(m_buffers[m_submittedBufferIdx] != nil);
        return m_buffers[m_submittedBufferIdx].contents;
    }

    void onUnmap() override {}

private:
    id<MTLDevice> m_gpu;
    id<MTLBuffer> m_buffers[pls::kBufferRingSize];
    int m_submittedBufferIdx = -1;
};

rcp<RenderBuffer> PLSRenderContextMetalImpl::makeRenderBuffer(RenderBufferType type,
                                                              RenderBufferFlags flags,
                                                              size_t sizeInBytes)
{
    return make_rcp<RenderBufferMetalImpl>(type, flags, sizeInBytes, m_gpu);
}

class PLSTextureMetalImpl : public PLSTexture
{
public:
    PLSTextureMetalImpl(id<MTLDevice> gpu,
                        uint32_t width,
                        uint32_t height,
                        uint32_t mipLevelCount,
                        const uint8_t imageDataRGBA[]) :
        PLSTexture(width, height)
    {
        // Create the texture.
        MTLTextureDescriptor* desc = [[MTLTextureDescriptor alloc] init];
        desc.pixelFormat = MTLPixelFormatRGBA8Unorm;
        desc.width = width;
        desc.height = height;
        desc.mipmapLevelCount = mipLevelCount;
        desc.usage = MTLTextureUsageShaderRead;
        desc.storageMode = MTLStorageModeShared;
        desc.textureType = MTLTextureType2D;
        m_texture = [gpu newTextureWithDescriptor:desc];

        // Specify the top-level image in the mipmap chain.
        MTLRegion region = MTLRegionMake2D(0, 0, width, height);
        [m_texture replaceRegion:region
                     mipmapLevel:0
                       withBytes:imageDataRGBA
                     bytesPerRow:width * 4];
    }

    void ensureMipmaps(id<MTLCommandBuffer> commandBuffer) const
    {
        if (m_mipsDirty)
        {
            // Generate mipmaps.
            id<MTLBlitCommandEncoder> mipEncoder = [commandBuffer blitCommandEncoder];
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

rcp<PLSTexture> PLSRenderContextMetalImpl::makeImageTexture(uint32_t width,
                                                            uint32_t height,
                                                            uint32_t mipLevelCount,
                                                            const uint8_t imageDataRGBA[])
{
    return make_rcp<PLSTextureMetalImpl>(m_gpu, width, height, mipLevelCount, imageDataRGBA);
}

std::unique_ptr<TexelBufferRing> PLSRenderContextMetalImpl::makeTexelBufferRing(
    TexelBufferRing::Format format,
    size_t widthInItems,
    size_t height,
    size_t texelsPerItem,
    int textureIdx,
    TexelBufferRing::Filter)
{
    return std::make_unique<TexelBufferMetalImpl>(
        m_gpu, format, widthInItems, height, texelsPerItem);
}

std::unique_ptr<BufferRing> PLSRenderContextMetalImpl::makeVertexBufferRing(size_t capacity,
                                                                            size_t itemSizeInBytes)
{
    return std::make_unique<BufferRingMetalImpl>(m_gpu, capacity, itemSizeInBytes);
}

std::unique_ptr<BufferRing> PLSRenderContextMetalImpl::makePixelUnpackBufferRing(
    size_t capacity, size_t itemSizeInBytes)
{
    return std::make_unique<BufferRingMetalImpl>(m_gpu, capacity, itemSizeInBytes);
}

std::unique_ptr<BufferRing> PLSRenderContextMetalImpl::makeUniformBufferRing(size_t capacity,
                                                                             size_t itemSizeInBytes)
{
    return std::make_unique<BufferRingMetalImpl>(m_gpu, capacity, itemSizeInBytes);
}

void PLSRenderContextMetalImpl::resizeGradientTexture(size_t height)
{
    MTLTextureDescriptor* desc = [[MTLTextureDescriptor alloc] init];
    desc.pixelFormat = MTLPixelFormatRGBA8Unorm;
    desc.width = kGradTextureWidth;
    desc.height = height;
    desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    desc.textureType = MTLTextureType2D;
    desc.mipmapLevelCount = 1;
    desc.storageMode = MTLStorageModePrivate;
    m_gradientTexture = [m_gpu newTextureWithDescriptor:desc];
}

void PLSRenderContextMetalImpl::resizeTessellationTexture(size_t height)
{
    MTLTextureDescriptor* desc = [[MTLTextureDescriptor alloc] init];
    desc.pixelFormat = MTLPixelFormatRGBA32Uint;
    desc.width = kTessTextureWidth;
    desc.height = height;
    desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    desc.textureType = MTLTextureType2D;
    desc.mipmapLevelCount = 1;
    desc.storageMode = MTLStorageModePrivate;
    m_tessVertexTexture = [m_gpu newTextureWithDescriptor:desc];
}

void PLSRenderContextMetalImpl::prepareToMapBuffers()
{
    // Wait until the GPU finishes rendering flush "N + 1 - kBufferRingSize". This ensures it
    // is safe for the CPU to begin modifying the next buffers in our rings.
    m_bufferRingIdx = (m_bufferRingIdx + 1) % kBufferRingSize;
    m_bufferRingLocks[m_bufferRingIdx].lock();
}

static id<MTLBuffer> mtl_buffer(const BufferRing* bufferRing)
{
    return static_cast<const BufferRingMetalImpl*>(bufferRing)->submittedBuffer();
}

static id<MTLTexture> mtl_texture(const TexelBufferRing* texelBufferRing)
{
    return static_cast<const TexelBufferMetalImpl*>(texelBufferRing)->submittedTexture();
}

const PLSRenderContextMetalImpl::DrawPipeline* PLSRenderContextMetalImpl::
    findCompatibleDrawPipeline(pls::DrawType drawType, pls::ShaderFeatures shaderFeatures)
{
    uint32_t pipelineKey = pls::ShaderUniqueKey(drawType, shaderFeatures);
    auto pipelineIter = m_drawPipelines.find(pipelineKey);
    if (pipelineIter == m_drawPipelines.end())
    {
        // The shader for this pipeline hasn't been scheduled for compiling yet. Schedule it to
        // compile in the background.
        m_backgroundShaderCompiler->pushJob(
            {.drawType = drawType, .shaderFeatures = shaderFeatures});
        pipelineIter = m_drawPipelines.insert({pipelineKey, nullptr}).first;
    }

    if (pipelineIter->second == nullptr)
    {
        // The shader for this pipeline hasn't finished compiling yet. Poll and see if it's done.
        BackgroundCompileJob job;
        while (m_backgroundShaderCompiler->popFinishedJob(&job, m_shouldWaitForShaderCompilations))
        {
            uint32_t jobKey = pls::ShaderUniqueKey(job.drawType, job.shaderFeatures);
            m_drawPipelines[jobKey] = std::make_unique<DrawPipeline>(
                m_gpu, job.compiledLibrary, @GLSL_drawVertexMain, @GLSL_drawFragmentMain);
            if (jobKey == pipelineKey)
            {
                assert(pipelineIter->second != nullptr);
                break;
            }
        }
        if (pipelineIter->second == nullptr)
        {
            // The shader for pipeline set hasn't finished compiling. Use the pipeline that has all
            // features enabled while we wait for it to finish.
            pipelineKey = ShaderUniqueKey(drawType, pls::AllShaderFeaturesForDrawType(drawType));
            pipelineIter = m_drawPipelines.find(pipelineKey);
            assert(pipelineIter->second != nullptr);
        }
    }
    return pipelineIter->second.get();
}

void PLSRenderContextMetalImpl::flush(const PLSRenderContext::FlushDescriptor& desc)
{
    auto* renderTarget = static_cast<const PLSRenderTargetMetal*>(desc.renderTarget);

    id<MTLCommandBuffer> commandBuffer =
        desc.backendSpecificData != nullptr
            ? (__bridge id<MTLCommandBuffer>)desc.backendSpecificData
            : [m_queue commandBuffer];

    // Render the complex color ramps to the gradient texture.
    if (desc.complexGradSpanCount > 0)
    {
        MTLRenderPassDescriptor* gradPass = [MTLRenderPassDescriptor renderPassDescriptor];
        gradPass.renderTargetWidth = kGradTextureWidth;
        gradPass.renderTargetHeight = desc.complexGradRowsTop + desc.complexGradRowsHeight;
        gradPass.colorAttachments[0].loadAction = MTLLoadActionDontCare;
        gradPass.colorAttachments[0].storeAction = MTLStoreActionStore;
        gradPass.colorAttachments[0].texture = m_gradientTexture;

        id<MTLRenderCommandEncoder> gradEncoder =
            [commandBuffer renderCommandEncoderWithDescriptor:gradPass];
        [gradEncoder setViewport:(MTLViewport){0.f,
                                               static_cast<double>(desc.complexGradRowsTop),
                                               kGradTextureWidth,
                                               static_cast<float>(desc.complexGradRowsHeight),
                                               0.0,
                                               1.0}];
        [gradEncoder setRenderPipelineState:m_colorRampPipeline->pipelineState()];
        [gradEncoder setVertexBuffer:mtl_buffer(flushUniformBufferRing())
                              offset:0
                             atIndex:FLUSH_UNIFORM_BUFFER_IDX];
        [gradEncoder setVertexBuffer:mtl_buffer(gradSpanBufferRing()) offset:0 atIndex:0];
        [gradEncoder setCullMode:MTLCullModeBack];
        [gradEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip
                        vertexStart:0
                        vertexCount:4
                      instanceCount:desc.complexGradSpanCount];
        [gradEncoder endEncoding];
    }

    // Copy the simple color ramps to the gradient texture.
    if (desc.simpleGradTexelsHeight > 0)
    {
        id<MTLBlitCommandEncoder> blitEncoder = [commandBuffer blitCommandEncoder];
        [blitEncoder copyFromBuffer:mtl_buffer(simpleColorRampsBufferRing())
                       sourceOffset:0
                  sourceBytesPerRow:kGradTextureWidth * 4
                sourceBytesPerImage:desc.simpleGradTexelsHeight * kGradTextureWidth * 4
                         sourceSize:MTLSizeMake(
                                        desc.simpleGradTexelsWidth, desc.simpleGradTexelsHeight, 1)
                          toTexture:m_gradientTexture
                   destinationSlice:0
                   destinationLevel:0
                  destinationOrigin:MTLOriginMake(0, 0, 0)];

        [blitEncoder endEncoding];
    }

    // Tessellate all curves into vertices in the tessellation texture.
    if (desc.tessVertexSpanCount > 0)
    {
        MTLRenderPassDescriptor* tessPass = [MTLRenderPassDescriptor renderPassDescriptor];
        tessPass.renderTargetWidth = kTessTextureWidth;
        tessPass.renderTargetHeight = desc.tessDataHeight;
        tessPass.colorAttachments[0].loadAction = MTLLoadActionDontCare;
        tessPass.colorAttachments[0].storeAction = MTLStoreActionStore;
        tessPass.colorAttachments[0].texture = m_tessVertexTexture;

        id<MTLRenderCommandEncoder> tessEncoder =
            [commandBuffer renderCommandEncoderWithDescriptor:tessPass];
        [tessEncoder setViewport:(MTLViewport){0.f,
                                               0.f,
                                               kTessTextureWidth,
                                               static_cast<float>(desc.tessDataHeight),
                                               0.0,
                                               1.0}];
        [tessEncoder setRenderPipelineState:m_tessPipeline->pipelineState()];
        [tessEncoder setVertexBuffer:mtl_buffer(flushUniformBufferRing())
                              offset:0
                             atIndex:FLUSH_UNIFORM_BUFFER_IDX];
        [tessEncoder setVertexBuffer:mtl_buffer(tessSpanBufferRing()) offset:0 atIndex:0];
        [tessEncoder setVertexTexture:mtl_texture(pathBufferRing()) atIndex:PATH_TEXTURE_IDX];
        [tessEncoder setVertexTexture:mtl_texture(contourBufferRing()) atIndex:CONTOUR_TEXTURE_IDX];
        [tessEncoder setCullMode:MTLCullModeBack];
        [tessEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                indexCount:std::size(pls::kTessSpanIndices)
                                 indexType:MTLIndexTypeUInt16
                               indexBuffer:m_tessSpanIndexBuffer
                         indexBufferOffset:0
                             instanceCount:desc.tessVertexSpanCount];
        [tessEncoder endEncoding];
    }

    // Generate mipmaps if needed.
    for (const Draw& draw : *desc.drawList)
    {
        // Bind the appropriate image texture, if any.
        if (auto imageTextureMetal = static_cast<const PLSTextureMetalImpl*>(draw.imageTextureRef))
        {
            imageTextureMetal->ensureMipmaps(commandBuffer);
        }
    }

    // Set up the render pass that draws path patches and triangles.
    MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = renderTarget->targetTexture();
    if (desc.loadAction == LoadAction::clear)
    {
        float cc[4];
        UnpackColorToRGBA32F(desc.clearColor, cc);
        pass.colorAttachments[0].loadAction = MTLLoadActionClear;
        pass.colorAttachments[0].clearColor = MTLClearColorMake(cc[0], cc[1], cc[2], cc[3]);
    }
    else
    {
        pass.colorAttachments[0].loadAction = MTLLoadActionLoad;
    }
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;

    pass.colorAttachments[1].texture = renderTarget->m_coverageMemorylessTexture;
    pass.colorAttachments[1].loadAction = MTLLoadActionClear;
    pass.colorAttachments[1].clearColor = MTLClearColorMake(0, 0, 0, 0);
    pass.colorAttachments[1].storeAction = MTLStoreActionDontCare;

    pass.colorAttachments[2].texture = renderTarget->m_originalDstColorMemorylessTexture;
    pass.colorAttachments[2].loadAction = MTLLoadActionDontCare;
    pass.colorAttachments[2].storeAction = MTLStoreActionDontCare;

    pass.colorAttachments[3].texture = renderTarget->m_clipMemorylessTexture;
    pass.colorAttachments[3].loadAction = MTLLoadActionClear;
    pass.colorAttachments[3].clearColor = MTLClearColorMake(0, 0, 0, 0);
    pass.colorAttachments[3].storeAction = MTLStoreActionDontCare;

    id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:pass];
    [encoder setViewport:(MTLViewport){0.f,
                                       0.f,
                                       static_cast<float>(renderTarget->width()),
                                       static_cast<float>(renderTarget->height()),
                                       0.0,
                                       1.0}];
    [encoder setVertexBuffer:mtl_buffer(flushUniformBufferRing())
                      offset:0
                     atIndex:FLUSH_UNIFORM_BUFFER_IDX];
    [encoder setVertexTexture:m_tessVertexTexture atIndex:TESS_VERTEX_TEXTURE_IDX];
    [encoder setVertexTexture:mtl_texture(pathBufferRing()) atIndex:PATH_TEXTURE_IDX];
    [encoder setVertexTexture:mtl_texture(contourBufferRing()) atIndex:CONTOUR_TEXTURE_IDX];
    [encoder setFragmentTexture:m_gradientTexture atIndex:GRAD_TEXTURE_IDX];
    if (desc.wireframe)
    {
        [encoder setTriangleFillMode:MTLTriangleFillModeLines];
    }

    // Execute the DrawList.
    for (const Draw& draw : *desc.drawList)
    {
        if (draw.elementCount == 0)
        {
            continue;
        }

        // Setup the pipeline for this specific drawType and shaderFeatures.
        const DrawPipeline* drawPipeline =
            findCompatibleDrawPipeline(draw.drawType, draw.shaderFeatures);
        [encoder setRenderPipelineState:drawPipeline->pipelineState(renderTarget->pixelFormat())];

        // Bind the appropriate image texture, if any.
        if (auto imageTextureMetal = static_cast<const PLSTextureMetalImpl*>(draw.imageTextureRef))
        {
            [encoder setFragmentTexture:imageTextureMetal->texture() atIndex:IMAGE_TEXTURE_IDX];
        }

        DrawType drawType = draw.drawType;
        switch (drawType)
        {
            case DrawType::midpointFanPatches:
            case DrawType::outerCurvePatches:
            {
                // Draw PLS patches that connect the tessellation vertices.
                [encoder setVertexBuffer:m_pathPatchVertexBuffer offset:0 atIndex:0];
                [encoder setCullMode:MTLCullModeBack];
                [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                    indexCount:PatchIndexCount(drawType)
                                     indexType:MTLIndexTypeUInt16
                                   indexBuffer:m_pathPatchIndexBuffer
                             indexBufferOffset:PatchBaseIndex(drawType) * sizeof(uint16_t)
                                 instanceCount:draw.elementCount
                                    baseVertex:0
                                  baseInstance:draw.baseElement];
                break;
            }
            case DrawType::interiorTriangulation:
            {
                [encoder setVertexBuffer:mtl_buffer(triangleBufferRing()) offset:0 atIndex:0];
                [encoder setCullMode:MTLCullModeBack];
                [encoder drawPrimitives:MTLPrimitiveTypeTriangle
                            vertexStart:draw.baseElement
                            vertexCount:draw.elementCount];
                break;
            }
            case DrawType::imageMesh:
            {
                LITE_RTTI_CAST_OR_BREAK(
                    vertexBuffer, const RenderBufferMetalImpl*, draw.vertexBufferRef);
                LITE_RTTI_CAST_OR_BREAK(uvBuffer, const RenderBufferMetalImpl*, draw.uvBufferRef);
                LITE_RTTI_CAST_OR_BREAK(
                    indexBuffer, const RenderBufferMetalImpl*, draw.indexBufferRef);
                [encoder setVertexBuffer:mtl_buffer(imageMeshUniformBufferRing())
                                  offset:draw.imageMeshDataOffset
                                 atIndex:IMAGE_MESH_UNIFORM_BUFFER_IDX];
                [encoder setFragmentBuffer:mtl_buffer(imageMeshUniformBufferRing())
                                    offset:draw.imageMeshDataOffset
                                   atIndex:IMAGE_MESH_UNIFORM_BUFFER_IDX];
                [encoder setVertexBuffer:vertexBuffer->submittedBuffer() offset:0 atIndex:0];
                [encoder setVertexBuffer:uvBuffer->submittedBuffer() offset:0 atIndex:1];
                [encoder setCullMode:MTLCullModeNone];
                [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                    indexCount:draw.elementCount
                                     indexType:MTLIndexTypeUInt16
                                   indexBuffer:indexBuffer->submittedBuffer()
                             indexBufferOffset:draw.baseElement * sizeof(uint16_t)];
                break;
            }
        }
    }
    [encoder endEncoding];

    // Schedule a callback that will unlock the buffers used by this flush, after the GPU has
    // finished rendering with them. This unblocks the CPU from reusing them in a future flush.
    std::mutex& thisFlushLock = m_bufferRingLocks[m_bufferRingIdx];
    [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer>) {
      assert(!thisFlushLock.try_lock()); // The mutex should already be locked.
      thisFlushLock.unlock();
    }];

    // Commit our commandBuffer if it's one generated on our own queue. If it's
    // external, then whoever supplied it is responsible for committing.
    if (desc.backendSpecificData == nullptr)
    {
        [commandBuffer commit];
    }
}
} // namespace rive::pls
