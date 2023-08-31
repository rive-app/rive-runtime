/*
 * Copyright 2023 Rive
 */

#include "rive/pls/metal/pls_render_context_metal_impl.h"

#include "rive/pls/buffer_ring.hpp"
#include "rive/pls/pls_image.hpp"
#include <sstream>

#include "../out/obj/generated/color_ramp.exports.h"
#include "../out/obj/generated/draw.exports.h"
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
        printf("%s\n", err.localizedDescription.UTF8String);
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
    DrawPipeline(PLSRenderContextMetalImpl* context,
                 DrawType drawType,
                 const ShaderFeatures& shaderFeatures)
    {
        id<MTLFunction> vertexMain = GetFunction(
            context, drawType, shaderFeatures & kVertexShaderFeaturesMask, GLSL_drawVertexMain);
        id<MTLFunction> fragmentMain =
            GetFunction(context, drawType, shaderFeatures, GLSL_drawFragmentMain);
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
        m_pipelineStateRGBA8 =
            makePipelineState(context->m_gpu, vertexMain, fragmentMain, MTLPixelFormatRGBA8Unorm);
        m_pipelineStateBGRA8 =
            makePipelineState(context->m_gpu, vertexMain, fragmentMain, MTLPixelFormatBGRA8Unorm);
    }

    id<MTLRenderPipelineState> pipelineState(MTLPixelFormat pixelFormat) const
    {
        assert(pixelFormat == MTLPixelFormatRGBA8Unorm || pixelFormat == MTLPixelFormatBGRA8Unorm);
        return pixelFormat == MTLPixelFormatRGBA8Unorm ? m_pipelineStateRGBA8
                                                       : m_pipelineStateBGRA8;
    }

private:
    id<MTLFunction> GetFunction(PLSRenderContextMetalImpl* context,
                                DrawType drawType,
                                ShaderFeatures shaderFeatures,
                                const char* functionName)
    {
        // Each feature corresponds to a specific index in the namespaceID. These must stay in sync
        // with generate_draw_combinations.py.
        char namespaceID[] = "0000000";
        if (drawType == DrawType::interiorTriangulation)
        {
            namespaceID[0] = '1';
        }
        for (size_t i = 0; i < kShaderFeatureCount; ++i)
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
        NSString* fullyQualifiedName =
            [NSString stringWithFormat:@"r%s::%s", namespaceID, functionName];
        return [context->m_plsLibrary newFunctionWithName:fullyQualifiedName];
    }

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

std::unique_ptr<PLSRenderContext> PLSRenderContextMetalImpl::MakeContext(id<MTLDevice> gpu,
                                                                         id<MTLCommandQueue> queue)
{
    if (![gpu supportsFamily:MTLGPUFamilyApple1])
    {
        printf("error: GPU is not Apple family.");
        return nullptr;
    }
    auto plsContextImpl =
        std::unique_ptr<PLSRenderContextMetalImpl>(new PLSRenderContextMetalImpl(gpu, queue));
    return std::make_unique<PLSRenderContext>(std::move(plsContextImpl));
}

PLSRenderContextMetalImpl::PLSRenderContextMetalImpl(id<MTLDevice> gpu, id<MTLCommandQueue> queue) :
    m_gpu(gpu), m_queue(queue)
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
    m_plsLibrary = [m_gpu newLibraryWithData:metallibData error:&err];
    if (m_plsLibrary == nil)
    {
        printf("Failed to load pls metallib.\n");
        printf("%s\n", err.localizedDescription.UTF8String);
        exit(-1);
    }
    m_colorRampPipeline = std::make_unique<ColorRampPipeline>(gpu, m_plsLibrary);
    m_tessPipeline = std::make_unique<TessellatePipeline>(gpu, m_plsLibrary);

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

class PLSTextureMetalImpl : public PLSTexture
{
public:
    PLSTextureMetalImpl(id<MTLDevice> gpu,
                        id<MTLCommandQueue> queue,
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

        // Generate mipmaps.
        id<MTLCommandBuffer> commandBuffer = [queue commandBuffer];
        id<MTLBlitCommandEncoder> mipEncoder = [commandBuffer blitCommandEncoder];
        [mipEncoder generateMipmapsForTexture:m_texture];
        [mipEncoder endEncoding];
        [commandBuffer commit];
    }

    id<MTLTexture> texture() const { return m_texture; }

private:
    id<MTLTexture> m_texture;
};

rcp<PLSTexture> PLSRenderContextMetalImpl::makeImageTexture(uint32_t width,
                                                            uint32_t height,
                                                            uint32_t mipLevelCount,
                                                            const uint8_t imageDataRGBA[])
{
    return make_rcp<PLSTextureMetalImpl>(
        m_gpu, m_queue, width, height, mipLevelCount, imageDataRGBA);
}

class TexelBufferMetal : public TexelBufferRing
{
public:
    TexelBufferMetal(id<MTLDevice> gpu,
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

std::unique_ptr<TexelBufferRing> PLSRenderContextMetalImpl::makeTexelBufferRing(
    TexelBufferRing::Format format,
    size_t widthInItems,
    size_t height,
    size_t texelsPerItem,
    int textureIdx,
    TexelBufferRing::Filter)
{
    return std::make_unique<TexelBufferMetal>(m_gpu, format, widthInItems, height, texelsPerItem);
}

class BufferMetal : public BufferRing
{
public:
    BufferMetal(id<MTLDevice> gpu, size_t capacity, size_t itemSizeInBytes) :
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

std::unique_ptr<BufferRing> PLSRenderContextMetalImpl::makeVertexBufferRing(size_t capacity,
                                                                            size_t itemSizeInBytes)
{
    return std::make_unique<BufferMetal>(m_gpu, capacity, itemSizeInBytes);
}

std::unique_ptr<BufferRing> PLSRenderContextMetalImpl::makePixelUnpackBufferRing(
    size_t capacity, size_t itemSizeInBytes)
{
    return std::make_unique<BufferMetal>(m_gpu, capacity, itemSizeInBytes);
}

std::unique_ptr<BufferRing> PLSRenderContextMetalImpl::makeUniformBufferRing(size_t itemSizeInBytes)
{
    return std::make_unique<BufferMetal>(m_gpu, 1, itemSizeInBytes);
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
    return static_cast<const BufferMetal*>(bufferRing)->submittedBuffer();
}

static id<MTLTexture> mtl_texture(const TexelBufferRing* texelBufferRing)
{
    return static_cast<const TexelBufferMetal*>(texelBufferRing)->submittedTexture();
}

void PLSRenderContextMetalImpl::flush(const PLSRenderContext::FlushDescriptor& desc)
{
    auto* renderTarget = static_cast<const PLSRenderTargetMetal*>(desc.renderTarget);
    id<MTLCommandBuffer> commandBuffer = [m_queue commandBuffer];

    // Render the complex color ramps to the gradient texture.
    if (desc.complexGradSpanCount > 0)
    {
        MTLRenderPassDescriptor* gradPass = [MTLRenderPassDescriptor renderPassDescriptor];
        gradPass.renderTargetWidth = kGradTextureWidth;
        gradPass.renderTargetHeight = desc.complexGradRowsTop + desc.complexGradRowsHeight;
        // TODO: Uploading the "simple" gradient texels directly to this texture requires us to use
        // MTLLoadActionLoad here, in addition to triple buffering the entire gradient texture. We
        // should consider different approaches:
        //
        //   * Upload the simple texels from the CPU to a separate buffer, and then transfer.
        //   * Just render the simple ramps too.
        //   * Upload the simple texels to the bottom of the texture, and then use
        //     "gradPass.renderTargetHeight" in combination with MTLLoadActionDontCare. (Still
        //     requires triple buffering of the gradient texture.)
        //
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
        [gradEncoder setVertexBuffer:mtl_buffer(uniformBufferRing()) offset:0 atIndex:0];
        [gradEncoder setVertexBuffer:mtl_buffer(gradSpanBufferRing()) offset:0 atIndex:1];
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
        [tessEncoder setVertexBuffer:mtl_buffer(uniformBufferRing()) offset:0 atIndex:0];
        [tessEncoder setVertexBuffer:mtl_buffer(tessSpanBufferRing()) offset:0 atIndex:1];
        [tessEncoder setVertexTexture:mtl_texture(pathBufferRing()) atIndex:kPathTextureIdx];
        [tessEncoder setVertexTexture:mtl_texture(contourBufferRing()) atIndex:kContourTextureIdx];
        [tessEncoder setCullMode:MTLCullModeBack];
        // Draw two instances per TessVertexSpan: one normal and one optional reflection.
        [tessEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip
                        vertexStart:0
                        vertexCount:4
                      instanceCount:desc.tessVertexSpanCount * 2];
        [tessEncoder endEncoding];
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
    [encoder setVertexBuffer:mtl_buffer(uniformBufferRing()) offset:0 atIndex:0];
    [encoder setVertexTexture:m_tessVertexTexture atIndex:kTessVertexTextureIdx];
    [encoder setVertexTexture:mtl_texture(pathBufferRing()) atIndex:kPathTextureIdx];
    [encoder setVertexTexture:mtl_texture(contourBufferRing()) atIndex:kContourTextureIdx];
    [encoder setFragmentTexture:m_gradientTexture atIndex:kGradTextureIdx];
    [encoder setCullMode:MTLCullModeBack];
    if (desc.wireframe)
    {
        [encoder setTriangleFillMode:MTLTriangleFillModeLines];
    }

    // Execute the DrawList.
    for (const Draw& draw : *desc.drawList)
    {
        if (draw.vertexOrInstanceCount == 0)
        {
            continue;
        }

        DrawType drawType = draw.drawType;

        // Setup the pipeline for this specific drawType and shaderFeatures.
        uint32_t pipelineKey = ShaderUniqueKey(drawType, draw.shaderFeatures);
        const DrawPipeline& drawPipeline =
            m_drawPipelines.try_emplace(pipelineKey, this, drawType, draw.shaderFeatures)
                .first->second;
        [encoder setRenderPipelineState:drawPipeline.pipelineState(renderTarget->pixelFormat())];

        // Bind the appropriate image texture, if any.
        if (auto imageTextureMetal = static_cast<const PLSTextureMetalImpl*>(draw.imageTextureRef))
        {
            [encoder setFragmentTexture:imageTextureMetal->texture() atIndex:kImageTextureIdx];
        }

        switch (drawType)
        {
            case DrawType::midpointFanPatches:
            case DrawType::outerCurvePatches:
            {
                // Draw PLS patches that connect the tessellation vertices.
                [encoder setVertexBuffer:m_pathPatchVertexBuffer offset:0 atIndex:1];
                [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                    indexCount:PatchIndexCount(drawType)
                                     indexType:MTLIndexTypeUInt16
                                   indexBuffer:m_pathPatchIndexBuffer
                             indexBufferOffset:PatchBaseIndex(drawType) * sizeof(uint16_t)
                                 instanceCount:draw.vertexOrInstanceCount
                                    baseVertex:0
                                  baseInstance:draw.baseVertexOrInstance];
                break;
            }
            case DrawType::interiorTriangulation:
                // Draw generic triangles.
                [encoder setVertexBuffer:mtl_buffer(triangleBufferRing()) offset:0 atIndex:1];
                [encoder drawPrimitives:MTLPrimitiveTypeTriangle
                            vertexStart:draw.baseVertexOrInstance
                            vertexCount:draw.vertexOrInstanceCount];
                break;
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

    [commandBuffer commit];
}
} // namespace rive::pls
