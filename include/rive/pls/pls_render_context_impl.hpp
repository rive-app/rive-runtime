/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/pls/pls_render_context.hpp"

namespace rive::pls
{
class PLSTexture;

// This class manages GPU buffers and isues the actual rendering commands from PLSRenderContext.
class PLSRenderContextImpl
{
public:
    virtual ~PLSRenderContextImpl() {}

    const PlatformFeatures& platformFeatures() const { return m_platformFeatures; }

    virtual rcp<RenderBuffer> makeRenderBuffer(RenderBufferType, RenderBufferFlags, size_t) = 0;

    // Decodes the image bytes and creates a texture that can be bound to the draw shader for an
    // image paint.
    virtual rcp<PLSTexture> decodeImageTexture(Span<const uint8_t> encodedBytes) = 0;

    // Resize GPU buffers. These methods cannot fail, and must allocate the exact size requested.
    // PLSRenderContext takes care to minimize how often these methods are called, while also
    // growing and shrinking the memory footprint to fit current usage.
    virtual void resizePathBuffer(size_t sizeInBytes) = 0;
    virtual void resizeContourBuffer(size_t sizeInBytes) = 0;
    virtual void resizeSimpleColorRampsBuffer(size_t sizeInBytes) = 0;
    virtual void resizeGradSpanBuffer(size_t sizeInBytes) = 0;
    virtual void resizeTessVertexSpanBuffer(size_t sizeInBytes) = 0;
    virtual void resizeTriangleVertexBuffer(size_t sizeInBytes) = 0;
    virtual void resizeImageDrawUniformBuffer(size_t sizeInBytes) = 0;

    // Perform any synchronization or other tasks that need to run immediately before
    // PLSRenderContext begins mapping buffers for the next flush.
    virtual void prepareToMapBuffers() {}

    // Map GPU buffers. (The implementation may wish to allocate the mappable buffers in rings, in
    // order to avoid expensive synchronization with the GPU pipeline. See
    // PLSRenderContextBufferRingImpl.)
    virtual void mapPathBuffer(WriteOnlyMappedMemory<pls::PathData>*) = 0;
    virtual void mapContourBuffer(WriteOnlyMappedMemory<pls::ContourData>*) = 0;
    virtual void mapSimpleColorRampsBuffer(WriteOnlyMappedMemory<pls::TwoTexelRamp>*) = 0;
    virtual void mapGradSpanBuffer(WriteOnlyMappedMemory<pls::GradientSpan>*) = 0;
    virtual void mapTessVertexSpanBuffer(WriteOnlyMappedMemory<pls::TessVertexSpan>*) = 0;
    virtual void mapTriangleVertexBuffer(WriteOnlyMappedMemory<pls::TriangleVertex>*) = 0;
    virtual void mapImageDrawUniformBuffer(WriteOnlyMappedMemory<pls::ImageDrawUniforms>*) = 0;
    virtual void mapFlushUniformBuffer(WriteOnlyMappedMemory<pls::FlushUniforms>*) = 0;

    // Unmap GPU buffers. All buffers will be unmapped before flush().
    virtual void unmapPathBuffer(size_t bytesWritten) = 0;
    virtual void unmapContourBuffer(size_t bytesWritten) = 0;
    virtual void unmapSimpleColorRampsBuffer(size_t bytesWritten) = 0;
    virtual void unmapGradSpanBuffer(size_t bytesWritten) = 0;
    virtual void unmapTessVertexSpanBuffer(size_t bytesWritten) = 0;
    virtual void unmapTriangleVertexBuffer(size_t bytesWritten) = 0;
    virtual void unmapImageDrawUniformBuffer(size_t bytesWritten) = 0;
    virtual void unmapFlushUniformBuffer() = 0;

    // Allocate textures that the implementation is responsible to update during flush().
    virtual void resizePathTexture(uint32_t width, uint32_t height) = 0;
    virtual void resizeContourTexture(uint32_t width, uint32_t height) = 0;
    virtual void resizeGradientTexture(uint32_t width, uint32_t height) = 0;
    virtual void resizeTessellationTexture(uint32_t width, uint32_t height) = 0;

    struct FlushDescriptor
    {
        void* backendSpecificData = nullptr;
        const PLSRenderTarget* renderTarget = nullptr;
        LoadAction loadAction = LoadAction::clear;
        ColorInt clearColor = 0;
        pls::ShaderFeatures combinedShaderFeatures = pls::ShaderFeatures::NONE;
        uint32_t pathTexelsWidth = 0;
        uint32_t pathTexelsHeight = 0;
        size_t pathDataOffset = 0;
        uint32_t contourTexelsWidth = 0;
        uint32_t contourTexelsHeight = 0;
        size_t contourDataOffset = 0;
        size_t complexGradSpanCount = 0;
        size_t tessVertexSpanCount = 0;
        uint32_t simpleGradTexelsWidth = 0;
        uint32_t simpleGradTexelsHeight = 0;
        uint32_t complexGradRowsTop = 0;
        uint32_t complexGradRowsHeight = 0;
        uint32_t tessDataHeight = 0;
        bool needsClipBuffer = false;
        bool hasTriangleVertices = false;
        bool wireframe = false;
        const PLSRenderContext::PerFlushLinkedList<PLSRenderContext::Draw>* drawList = nullptr;
        size_t pathCount = 0;
        pls::InterlockMode interlockMode = pls::InterlockMode::rasterOrdered;
        pls::ExperimentalAtomicModeData* experimentalAtomicModeData = nullptr;

        // In atomic mode, we can skip the inital clear of the color buffer in some cases. This is
        // because we will be resolving the framebuffer anyway, and can work the clear into that
        // operation.
        bool canSkipColorClear() const
        {
            return interlockMode == pls::InterlockMode::experimentalAtomics &&
                   loadAction == LoadAction::clear && colorAlpha(clearColor) == 255;
        }
    };

    // Perform rendering in three steps:
    //
    //  1. Prepare the gradient texture:
    //      * Render the GradientSpan instances into the gradient texture.
    //      * Copy the TwoTexelRamp data directly into the gradient texture.
    //
    //  2. Render the TessVertexSpan instances into the tessellation texture.
    //
    //  3. Execute the draw list. (The Rive renderer shaders read the gradient and tessellation
    //     textures in order to do path rendering.)
    //
    virtual void flush(const FlushDescriptor&) = 0;

protected:
    using Draw = PLSRenderContext::Draw;
    PlatformFeatures m_platformFeatures;
};
} // namespace rive::pls
