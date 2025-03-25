/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/renderer/render_context.hpp"

namespace rive::gpu
{
class Texture;

// This class manages GPU buffers and isues the actual rendering commands from
// RenderContext.
class RenderContextImpl
{
public:
    virtual ~RenderContextImpl() {}

    const PlatformFeatures& platformFeatures() const
    {
        return m_platformFeatures;
    }

    virtual rcp<RenderBuffer> makeRenderBuffer(RenderBufferType,
                                               RenderBufferFlags,
                                               size_t) = 0;

    // Decodes the image bytes and creates a texture that can be bound to the
    // draw shader for an image paint.
    virtual rcp<Texture> decodeImageTexture(
        Span<const uint8_t> encodedBytes) = 0;

    // Resize GPU buffers. These methods cannot fail, and must allocate the
    // exact size requested.
    //
    // RenderContext takes care to minimize how often these methods are called,
    // while also growing and shrinking the memory footprint to fit current
    // usage.
    //
    // 'elementSizeInBytes' represents the size of one array element when the
    // shader accesses this buffer as a storage buffer.
    virtual void resizeFlushUniformBuffer(size_t sizeInBytes) = 0;
    virtual void resizeImageDrawUniformBuffer(size_t sizeInBytes) = 0;
    virtual void resizePathBuffer(size_t sizeInBytes,
                                  gpu::StorageBufferStructure) = 0;
    virtual void resizePaintBuffer(size_t sizeInBytes,
                                   gpu::StorageBufferStructure) = 0;
    virtual void resizePaintAuxBuffer(size_t sizeInBytes,
                                      gpu::StorageBufferStructure) = 0;
    virtual void resizeContourBuffer(size_t sizeInBytes,
                                     gpu::StorageBufferStructure) = 0;
    virtual void resizeGradSpanBuffer(size_t sizeInBytes) = 0;
    virtual void resizeTessVertexSpanBuffer(size_t sizeInBytes) = 0;
    virtual void resizeTriangleVertexBuffer(size_t sizeInBytes) = 0;

    // Perform any bookkeeping or other tasks that need to run before
    // RenderContext begins accessing GPU resources for the flush. (Update
    // counters, advance buffer pools, etc.)
    //
    // The provided resource lifetime counters communicate how the client is
    // performing CPU-GPU synchronization. Resources used during the upcoming
    // flush will belong to 'nextFrameNumber'. Resources last used on or before
    // 'safeFrameNumber' are safe to be released or recycled.
    virtual void prepareToFlush(uint64_t nextFrameNumber,
                                uint64_t safeFrameNumber)
    {}

    // Map GPU buffers. (The implementation may wish to allocate the mappable
    // buffers in rings, in order to avoid expensive synchronization with the
    // GPU pipeline. See RenderContextBufferRingImpl.)
    virtual void* mapFlushUniformBuffer(size_t mapSizeInBytes) = 0;
    virtual void* mapImageDrawUniformBuffer(size_t mapSizeInBytes) = 0;
    virtual void* mapPathBuffer(size_t mapSizeInBytes) = 0;
    virtual void* mapPaintBuffer(size_t mapSizeInBytes) = 0;
    virtual void* mapPaintAuxBuffer(size_t mapSizeInBytes) = 0;
    virtual void* mapContourBuffer(size_t mapSizeInBytes) = 0;
    virtual void* mapGradSpanBuffer(size_t mapSizeInBytes) = 0;
    virtual void* mapTessVertexSpanBuffer(size_t mapSizeInBytes) = 0;
    virtual void* mapTriangleVertexBuffer(size_t mapSizeInBytes) = 0;

    // Unmap GPU buffers. All buffers will be unmapped before flush().
    virtual void unmapFlushUniformBuffer() = 0;
    virtual void unmapImageDrawUniformBuffer() = 0;
    virtual void unmapPathBuffer() = 0;
    virtual void unmapPaintBuffer() = 0;
    virtual void unmapPaintAuxBuffer() = 0;
    virtual void unmapContourBuffer() = 0;
    virtual void unmapGradSpanBuffer() = 0;
    virtual void unmapTessVertexSpanBuffer() = 0;
    virtual void unmapTriangleVertexBuffer() = 0;

    // Allocate resources that are updated and used during flush().
    virtual void resizeGradientTexture(uint32_t width, uint32_t height) = 0;
    virtual void resizeTessellationTexture(uint32_t width, uint32_t height) = 0;
    virtual void resizeAtlasTexture(uint32_t width, uint32_t height)
    {
        // Override this method to support atlas feathering.
        assert(width == 0 && height == 0);
    }
    virtual void resizeCoverageBuffer(size_t sizeInBytes)
    {
        // Override this method to support the experimental clockwiseAtomic
        // mode.
        assert(sizeInBytes == 0);
    }

    // Perform rendering in three steps:
    //
    //  1. Prepare the gradient texture:
    //      * Render the GradientSpan instances into the gradient texture.
    //      * Copy the TwoTexelRamp data directly into the gradient texture.
    //
    //  2. Render the TessVertexSpan instances into the tessellation texture.
    //
    //  3. Execute the draw list. (The Rive renderer shaders read the gradient
    //  and tessellation
    //     textures in order to do path rendering.)
    //
    virtual void flush(const gpu::FlushDescriptor&) = 0;

    // Steady clock, used to determine when we should trim our resource
    // allocations.
    virtual double secondsNow() const = 0;

protected:
    PlatformFeatures m_platformFeatures;
};
} // namespace rive::gpu
