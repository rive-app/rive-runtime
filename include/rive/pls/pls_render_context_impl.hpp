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
    //
    // PLSRenderContext takes care to minimize how often these methods are called, while also
    // growing and shrinking the memory footprint to fit current usage.
    //
    // 'elementSizeInBytes' represents the size of one array element when the shader accesses this
    // buffer as a storage buffer.
    virtual void resizePathBuffer(size_t sizeInBytes, pls::StorageBufferStructure) = 0;
    virtual void resizePaintBuffer(size_t sizeInBytes, pls::StorageBufferStructure) = 0;
    virtual void resizePaintAuxBuffer(size_t sizeInBytes, pls::StorageBufferStructure) = 0;
    virtual void resizeContourBuffer(size_t sizeInBytes, pls::StorageBufferStructure) = 0;
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
    virtual void* mapPathBuffer(size_t mapSizeInBytes) = 0;
    virtual void* mapPaintBuffer(size_t mapSizeInBytes) = 0;
    virtual void* mapPaintAuxBuffer(size_t mapSizeInBytes) = 0;
    virtual void* mapContourBuffer(size_t mapSizeInBytes) = 0;
    virtual void* mapSimpleColorRampsBuffer(size_t mapSizeInBytes) = 0;
    virtual void* mapGradSpanBuffer(size_t mapSizeInBytes) = 0;
    virtual void* mapTessVertexSpanBuffer(size_t mapSizeInBytes) = 0;
    virtual void* mapTriangleVertexBuffer(size_t mapSizeInBytes) = 0;
    virtual void* mapImageDrawUniformBuffer(size_t mapSizeInBytes) = 0;
    virtual void* mapFlushUniformBuffer(size_t mapSizeInBytes) = 0;

    // Unmap GPU buffers. All buffers will be unmapped before flush().
    virtual void unmapPathBuffer() = 0;
    virtual void unmapPaintBuffer() = 0;
    virtual void unmapPaintAuxBuffer() = 0;
    virtual void unmapContourBuffer() = 0;
    virtual void unmapSimpleColorRampsBuffer() = 0;
    virtual void unmapGradSpanBuffer() = 0;
    virtual void unmapTessVertexSpanBuffer() = 0;
    virtual void unmapTriangleVertexBuffer() = 0;
    virtual void unmapImageDrawUniformBuffer() = 0;
    virtual void unmapFlushUniformBuffer() = 0;

    // Allocate textures that the implementation is responsible to update during flush().
    virtual void resizeGradientTexture(uint32_t width, uint32_t height) = 0;
    virtual void resizeTessellationTexture(uint32_t width, uint32_t height) = 0;

    struct FlushDescriptor
    {
        void* backendSpecificData = nullptr;
        const PLSRenderTarget* renderTarget = nullptr;
        LoadAction loadAction = LoadAction::clear;
        ColorInt clearColor = 0;
        pls::ShaderFeatures combinedShaderFeatures = pls::ShaderFeatures::NONE;
        size_t pathCount = 0;
        size_t firstPath = 0;
        size_t contourCount = 0;
        size_t firstContour = 0;
        size_t complexGradSpanCount = 0;
        size_t tessVertexSpanCount = 0;
        uint32_t simpleGradTexelsWidth = 0;
        uint32_t simpleGradTexelsHeight = 0;
        size_t simpleGradDataOffset = 0;
        uint32_t complexGradRowsTop = 0;
        uint32_t complexGradRowsHeight = 0;
        uint32_t tessDataHeight = 0;
        bool needsClipBuffer = false;
        bool hasTriangleVertices = false;
        bool wireframe = false;
        const PLSRenderContext::PerFlushLinkedList<PLSRenderContext::Draw>* drawList = nullptr;
        pls::InterlockMode interlockMode = pls::InterlockMode::rasterOrdered;

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
