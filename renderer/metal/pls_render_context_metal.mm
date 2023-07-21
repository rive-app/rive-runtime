/*
 * Copyright 2023 Rive
 */

#include "rive/pls/metal/pls_render_context_metal.h"

#include "buffer_ring_metal.h"
#include "rive/pls/metal/pls_render_target_metal.h"
#include <sstream>

#include "../out/obj/generated/color_ramp.exports.h"
#include "../out/obj/generated/draw.exports.h"
#include "../out/obj/generated/tessellate.exports.h"

namespace rive::pls
{
#ifdef RIVE_IOS
#include "../out/obj/generated/rive_pls_ios.metallib.c"
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
class PLSRenderContextMetal::ColorRampPipeline
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
class PLSRenderContextMetal::TessellatePipeline
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
class PLSRenderContextMetal::DrawPipeline
{
public:
    DrawPipeline(PLSRenderContextMetal* context,
                 DrawType drawType,
                 const ShaderFeatures& shaderFeatures)
    {
        id<MTLFunction> vertexMain =
            GetMainFunction(context, drawType, SourceType::vertexOnly, shaderFeatures);
        id<MTLFunction> fragmentMain =
            GetMainFunction(context, drawType, SourceType::wholeProgram, shaderFeatures);
        constexpr static auto makePipelineState = [](id<MTLDevice> gpu,
                                                     id<MTLFunction> vertexMain,
                                                     id<MTLFunction> fragmentMain,
                                                     MTLPixelFormat pixelFormat) {
            MTLRenderPipelineDescriptor* desc = [[MTLRenderPipelineDescriptor alloc] init];
            desc.vertexFunction = vertexMain;
            desc.fragmentFunction = fragmentMain;
            desc.colorAttachments[0].pixelFormat = pixelFormat;
            desc.colorAttachments[1].pixelFormat = MTLPixelFormatRG16Float;
            desc.colorAttachments[2].pixelFormat = pixelFormat;
            desc.colorAttachments[3].pixelFormat = MTLPixelFormatRG16Float;
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
    id<MTLFunction> GetMainFunction(PLSRenderContextMetal* context,
                                    DrawType drawType,
                                    SourceType sourceType,
                                    const ShaderFeatures& shaderFeatures)
    {
        // Namespaces beginning in 'r' indicate the normal Rive renderer.
        char namespaceName[] = "r0000";
        if (drawType == DrawType::interiorTriangulation)
        {
            // Namespaces beginning in 't' indicate the special case when we draw non-overlapping
            // interior triangles.
            namespaceName[0] = 't';
        }
        // draw.metal uses the following bits in the namespace names for each flag:
        //     ENABLE_ADVANCED_BLEND:   r0001 / t0001
        //     ENABLE_PATH_CLIPPING:    r0010 / t0010
        //     ENABLE_EVEN_ODD:         r0100 / t0100
        //     ENABLE_HSL_BLEND_MODES:  r1000 / t1000
        uint64_t shaderFeatureDefines = shaderFeatures.getPreprocessorDefines(sourceType);
        if (shaderFeatureDefines & ShaderFeatures::PreprocessorDefines::ENABLE_ADVANCED_BLEND)
        {
            namespaceName[4] = '1';
        }
        if (shaderFeatureDefines & ShaderFeatures::PreprocessorDefines::ENABLE_PATH_CLIPPING)
        {
            namespaceName[3] = '1';
        }
        if (shaderFeatureDefines & ShaderFeatures::PreprocessorDefines::ENABLE_EVEN_ODD)
        {
            namespaceName[2] = '1';
        }
        if (shaderFeatureDefines & ShaderFeatures::PreprocessorDefines::ENABLE_HSL_BLEND_MODES)
        {
            namespaceName[1] = '1';
        }
        const char* mainFunctionName =
            sourceType == SourceType::vertexOnly ? GLSL_drawVertexMain : GLSL_drawFragmentMain;
        NSString* fullyQualifiedName =
            [NSString stringWithFormat:@"%s::%s", namespaceName, mainFunctionName];
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

std::unique_ptr<PLSRenderContextMetal> PLSRenderContextMetal::Make(id<MTLDevice> gpu,
                                                                   id<MTLCommandQueue> queue)
{
    if (![gpu supportsFamily:MTLGPUFamilyApple1])
    {
        printf("error: GPU is not Apple family.");
        return nullptr;
    }
    PlatformFeatures platformFeatures;
#ifdef RIVE_IOS
    if (!is_apple_ios_silicon(gpu))
    {
        // The PowerVR GPU, at least on A10, has fp16 precision issues. We can't use the the bottom
        // 3 bits of the path and clip IDs in order for our equality testing to work.
        platformFeatures.pathIDGranularity = 8;
    }
#endif
    // It appears, so far, that we don't need to use flat interpolation for path IDs on any Apple
    // device, and it's faster not to.
    platformFeatures.avoidFlatVaryings = true;
    platformFeatures.invertOffscreenY = true;
    return std::unique_ptr<PLSRenderContextMetal>(
        new PLSRenderContextMetal(platformFeatures, gpu, queue));
}

PLSRenderContextMetal::PLSRenderContextMetal(const PlatformFeatures& platformFeatures,
                                             id<MTLDevice> gpu,
                                             id<MTLCommandQueue> queue) :
    PLSRenderContext(platformFeatures), m_gpu(gpu), m_queue(queue)
{
    // Load the precompiled shaders.
    dispatch_data_t metallibData = dispatch_data_create(
#ifdef RIVE_IOS
        rive_pls_ios_metallib,
        rive_pls_ios_metallib_len,
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

PLSRenderContextMetal::~PLSRenderContextMetal() {}

rcp<PLSRenderTargetMetal> PLSRenderContextMetal::makeRenderTarget(MTLPixelFormat pixelFormat,
                                                                  size_t width,
                                                                  size_t height)
{
    return rcp<PLSRenderTargetMetal>(
        new PLSRenderTargetMetal(m_gpu, pixelFormat, width, height, m_platformFeatures));
}

std::unique_ptr<BufferRingImpl> PLSRenderContextMetal::makeVertexBufferRing(size_t capacity,
                                                                            size_t itemSizeInBytes)
{
    return std::make_unique<BufferMetal>(m_gpu, capacity, itemSizeInBytes);
}

std::unique_ptr<TexelBufferRing> PLSRenderContextMetal::makeTexelBufferRing(
    TexelBufferRing::Format format,
    Renderable,
    size_t widthInItems,
    size_t height,
    size_t texelsPerItem,
    int textureIdx,
    TexelBufferRing::Filter)
{
    return std::make_unique<TexelBufferMetal>(m_gpu, format, widthInItems, height, texelsPerItem);
}

std::unique_ptr<BufferRingImpl> PLSRenderContextMetal::makeUniformBufferRing(size_t capacity,
                                                                             size_t itemSizeInBytes)
{
    return std::make_unique<BufferMetal>(m_gpu, capacity, itemSizeInBytes);
}

void PLSRenderContextMetal::allocateTessellationTexture(size_t height)
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

void PLSRenderContextMetal::lockNextBufferRingIndex()
{
    // Wait until the GPU finishes rendering flush "N + 1 - kBufferRingSize". This ensures it
    // is safe for the CPU to begin modifying the next buffers in our rings.
    m_bufferRingIdx = (m_bufferRingIdx + 1) % kBufferRingSize;
    m_bufferRingLocks[m_bufferRingIdx].lock();
}

static id<MTLBuffer> mtl_buffer(const BufferRingImpl* bufferRing)
{
    return static_cast<const BufferMetal*>(bufferRing)->submittedBuffer();
}

static id<MTLTexture> mtl_texture(const TexelBufferRing* texelBufferRing)
{
    return static_cast<const TexelBufferMetal*>(texelBufferRing)->submittedTexture();
}

void PLSRenderContextMetal::onFlush(FlushType flushType,
                                    LoadAction loadAction,
                                    size_t gradSpanCount,
                                    size_t gradSpansHeight,
                                    size_t tessVertexSpanCount,
                                    size_t tessDataHeight,
                                    bool needsClipBuffer)
{
    auto* renderTarget =
        static_cast<const PLSRenderTargetMetal*>(frameDescriptor().renderTarget.get());
    id<MTLCommandBuffer> commandBuffer = [m_queue commandBuffer];

    // Render the complex color ramps to the gradient texture.
    if (gradSpanCount > 0)
    {
        MTLRenderPassDescriptor* gradPass = [MTLRenderPassDescriptor renderPassDescriptor];
        gradPass.renderTargetWidth = kGradTextureWidth;
        gradPass.renderTargetHeight = gradTextureRowsForSimpleRamps() + gradSpansHeight;
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
        gradPass.colorAttachments[0].loadAction = MTLLoadActionLoad;
        gradPass.colorAttachments[0].storeAction = MTLStoreActionStore;
        gradPass.colorAttachments[0].texture = mtl_texture(gradTexelBufferRing());

        id<MTLRenderCommandEncoder> gradEncoder =
            [commandBuffer renderCommandEncoderWithDescriptor:gradPass];
        [gradEncoder setViewport:(MTLViewport){0.f,
                                               static_cast<double>(gradTextureRowsForSimpleRamps()),
                                               kGradTextureWidth,
                                               static_cast<float>(gradSpansHeight),
                                               0.0,
                                               1.0}];
        [gradEncoder setRenderPipelineState:m_colorRampPipeline->pipelineState()];
        [gradEncoder setVertexBuffer:mtl_buffer(uniformBufferRing()) offset:0 atIndex:0];
        [gradEncoder setVertexBuffer:mtl_buffer(gradSpanBufferRing()) offset:0 atIndex:1];
        [gradEncoder setCullMode:MTLCullModeBack];
        [gradEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip
                        vertexStart:0
                        vertexCount:4
                      instanceCount:gradSpanCount];
        [gradEncoder endEncoding];
    }

    // Tessellate all curves into vertices in the tessellation texture.
    if (tessVertexSpanCount > 0)
    {
        MTLRenderPassDescriptor* tessPass = [MTLRenderPassDescriptor renderPassDescriptor];
        tessPass.renderTargetWidth = kTessTextureWidth;
        tessPass.renderTargetHeight = tessDataHeight;
        tessPass.colorAttachments[0].loadAction = MTLLoadActionDontCare;
        tessPass.colorAttachments[0].storeAction = MTLStoreActionStore;
        tessPass.colorAttachments[0].texture = m_tessVertexTexture;

        id<MTLRenderCommandEncoder> tessEncoder =
            [commandBuffer renderCommandEncoderWithDescriptor:tessPass];
        [tessEncoder setViewport:(MTLViewport){0.f,
                                               0.f,
                                               kTessTextureWidth,
                                               static_cast<float>(tessDataHeight),
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
                      instanceCount:tessVertexSpanCount * 2];
        [tessEncoder endEncoding];
    }

    // Set up the render pass that draws path patches and triangles.
    MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = renderTarget->targetTexture();
    if (loadAction == LoadAction::clear)
    {
        float cc[4];
        UnpackColorToRGBA32F(frameDescriptor().clearColor, cc);
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
    [encoder setFragmentTexture:mtl_texture(gradTexelBufferRing()) atIndex:kGradTextureIdx];
    [encoder setCullMode:MTLCullModeBack];
    if (frameDescriptor().wireframe)
    {
        [encoder setTriangleFillMode:MTLTriangleFillModeLines];
    }

    // Execute the DrawList.
    for (const Draw& draw : m_drawList)
    {
        if (draw.vertexOrInstanceCount == 0)
        {
            continue;
        }

        DrawType drawType = draw.drawType;

        // Setup the pipeline for this specific drawType and shaderFeatures.
        uint32_t pipelineKey =
            ShaderUniqueKey(SourceType::wholeProgram, drawType, draw.shaderFeatures);
        const DrawPipeline& drawPipeline =
            m_drawPipelines.try_emplace(pipelineKey, this, drawType, draw.shaderFeatures)
                .first->second;
        [encoder setRenderPipelineState:drawPipeline.pipelineState(renderTarget->pixelFormat())];

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

    if (flushType == FlushType::intermediate)
    {
        // The frame isn't complete yet. The caller will begin preparing a new flush immediately
        // after this method returns, so lock buffers for the next flush now.
        lockNextBufferRingIndex();
    }

    [commandBuffer commit];
}
} // namespace rive::pls
