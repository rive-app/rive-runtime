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

    // Decodes the image bytes and creates a texture that can be bound to the draw shader for an
    // image paint.
    virtual rcp<PLSTexture> decodeImageTexture(Span<const uint8_t> encodedBytes) = 0;

    // Resize GPU resources. These methods cannot fail, and must allocate the exact size requested.
    // PLSRenderContext takes care to minimize how often these methods are called, while also
    // growing and shrinking the memory footprint to fit current usage.
    virtual void resizePathTexture(size_t width, size_t height) = 0;
    virtual void resizeContourTexture(size_t width, size_t height) = 0;
    virtual void resizeSimpleColorRampsBuffer(size_t sizeInBytes) = 0;
    virtual void resizeGradSpanBuffer(size_t sizeInBytes) = 0;
    virtual void resizeTessVertexSpanBuffer(size_t sizeInBytes) = 0;
    virtual void resizeTriangleVertexBuffer(size_t sizeInBytes) = 0;
    virtual void resizeGradientTexture(size_t height) = 0;
    virtual void resizeTessellationTexture(size_t height) = 0;

    // Perform any synchronization or other tasks that need to run immediately before
    // PLSRenderContext begins mapping buffers for the next flush.
    virtual void prepareToMapBuffers() {}

    // Map GPU resources. (The implementation may wish to allocate the mappable resources in rings,
    // in order to avoid expensive synchronization with the GPU pipeline.
    // See PLSRenderContextBufferRingImpl.)
    virtual void mapPathTexture(WriteOnlyMappedMemory<PathData>*) = 0;
    virtual void mapContourTexture(WriteOnlyMappedMemory<ContourData>*) = 0;
    virtual void mapSimpleColorRampsBuffer(WriteOnlyMappedMemory<TwoTexelRamp>*) = 0;
    virtual void mapGradSpanBuffer(WriteOnlyMappedMemory<GradientSpan>*) = 0;
    virtual void mapTessVertexSpanBuffer(WriteOnlyMappedMemory<TessVertexSpan>*) = 0;
    virtual void mapTriangleVertexBuffer(WriteOnlyMappedMemory<TriangleVertex>*) = 0;

    // Unmap GPU resources. All resources will be unmapped before flush().
    virtual void unmapPathTexture(size_t widthWritten, size_t heightWritten) = 0;
    virtual void unmapContourTexture(size_t widthWritten, size_t heightWritten) = 0;
    virtual void unmapSimpleColorRampsBuffer(size_t bytesWritten) = 0;
    virtual void unmapGradSpanBuffer(size_t bytesWritten) = 0;
    virtual void unmapTessVertexSpanBuffer(size_t bytesWritten) = 0;
    virtual void unmapTriangleVertexBuffer(size_t bytesWritten) = 0;

    // Update the FlushUniforms used by the Rive renderer shaders. This method is called immediately
    // before flush(), and only if the uniform data has changed.
    virtual void updateFlushUniforms(const FlushUniforms*) = 0;

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
    virtual void flush(const PLSRenderContext::FlushDescriptor&) = 0;

protected:
    PlatformFeatures m_platformFeatures;
};
} // namespace rive::pls
