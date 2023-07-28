/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/pls/pls_render_context.hpp"

namespace rive::pls
{
class PLSRenderContextImpl
{
public:
    virtual ~PLSRenderContextImpl() {}

    const PlatformFeatures& platformFeatures() const { return m_platformFeatures; }

    virtual void prepareToMapBuffers() {}

    virtual void resizePathTexture(size_t width, size_t height) = 0;
    virtual void resizeContourTexture(size_t width, size_t height) = 0;
    virtual void resizeSimpleColorRampsBuffer(size_t sizeInBytes) = 0;
    virtual void resizeGradSpanBuffer(size_t sizeInBytes) = 0;
    virtual void resizeTessVertexSpanBuffer(size_t sizeInBytes) = 0;
    virtual void resizeTriangleVertexBuffer(size_t sizeInBytes) = 0;
    virtual void resizeGradientTexture(size_t height) = 0;
    virtual void resizeTessellationTexture(size_t height) = 0;

    virtual void* mapPathTexture() = 0;
    virtual void* mapContourTexture() = 0;
    virtual void* mapSimpleColorRampsBuffer() = 0;
    virtual void* mapGradSpanBuffer() = 0;
    virtual void* mapTessVertexSpanBuffer() = 0;
    virtual void* mapTriangleVertexBuffer() = 0;

    virtual void unmapPathTexture(size_t widthWritten, size_t heightWritten) = 0;
    virtual void unmapContourTexture(size_t widthWritten, size_t heightWritten) = 0;
    virtual void unmapSimpleColorRampsBuffer(size_t bytesWritten) = 0;
    virtual void unmapGradSpanBuffer(size_t bytesWritten) = 0;
    virtual void unmapTessVertexSpanBuffer(size_t bytesWritten) = 0;
    virtual void unmapTriangleVertexBuffer(size_t bytesWritten) = 0;

    virtual void updateFlushUniforms(const FlushUniforms*) = 0;

    virtual void flush(const PLSRenderContext::FlushDescriptor&) = 0;

protected:
    PlatformFeatures m_platformFeatures;
};
} // namespace rive::pls
