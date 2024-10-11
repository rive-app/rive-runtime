/*
 * Copyright 2022 Rive
 */

#include "render_context_null.hpp"

#include "rive/renderer/rive_render_image.hpp"
#include "utils/factory_utils.hpp"

using namespace rive;
using namespace rive::gpu;

std::unique_ptr<rive::gpu::RenderContext> RenderContextNULL::MakeContext()
{
    return std::make_unique<RenderContext>(
        std::make_unique<RenderContextNULL>());
}

RenderContextNULL::RenderContextNULL()
{
    m_platformFeatures.supportsRasterOrdering = true;
    m_platformFeatures.supportsFragmentShaderAtomics = true;
}

class BufferRingNULL : public BufferRing
{
public:
    BufferRingNULL(size_t capacityInBytes) : BufferRing(capacityInBytes) {}

private:
    void* onMapBuffer(int bufferIdx, size_t bytesWritten)
    {
        return shadowBuffer();
    }
    void onUnmapAndSubmitBuffer(int bufferIdx, size_t bytesWritten) {}
};

rive::rcp<rive::gpu::RenderTarget> RenderContextNULL::makeRenderTarget(
    uint32_t width,
    uint32_t height)
{
    class RenderTargetNULL : public rive::gpu::RenderTarget
    {
    public:
        RenderTargetNULL(uint32_t width, uint32_t height) :
            RenderTarget(width, height)
        {}
    };
    return rive::make_rcp<RenderTargetNULL>(width, height);
}

rive::rcp<rive::RenderBuffer> RenderContextNULL::makeRenderBuffer(
    rive::RenderBufferType type,
    rive::RenderBufferFlags flags,
    size_t sizeInBytes)
{
    return make_rcp<DataRenderBuffer>(type, flags, sizeInBytes);
}

rcp<Texture> RenderContextNULL::makeImageTexture(uint32_t width,
                                                 uint32_t height,
                                                 uint32_t mipLevelCount,
                                                 const uint8_t imageDataRGBA[])
{
    return make_rcp<Texture>(width, height);
}

std::unique_ptr<rive::gpu::BufferRing> RenderContextNULL::makeUniformBufferRing(
    size_t capacityInBytes)
{
    return std::make_unique<BufferRingNULL>(capacityInBytes);
}

std::unique_ptr<rive::gpu::BufferRing> RenderContextNULL::makeStorageBufferRing(
    size_t capacityInBytes,
    rive::gpu::StorageBufferStructure)
{
    return std::make_unique<BufferRingNULL>(capacityInBytes);
}

std::unique_ptr<rive::gpu::BufferRing> RenderContextNULL::makeVertexBufferRing(
    size_t capacityInBytes)
{
    return std::make_unique<BufferRingNULL>(capacityInBytes);
}

std::unique_ptr<rive::gpu::BufferRing> RenderContextNULL::
    makeTextureTransferBufferRing(size_t capacityInBytes)
{
    return std::make_unique<BufferRingNULL>(capacityInBytes);
}
