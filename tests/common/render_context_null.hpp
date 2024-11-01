/*
 * Copyright 2022 Rive
 */

#pragma once

#include "rive/renderer/render_context_helper_impl.hpp"

class RenderContextNULL : public rive::gpu::RenderContextHelperImpl
{
public:
    static std::unique_ptr<rive::gpu::RenderContext> MakeContext();

    RenderContextNULL();

    rive::rcp<rive::gpu::RenderTarget> makeRenderTarget(uint32_t width,
                                                        uint32_t height);

private:
    rive::rcp<rive::RenderBuffer> makeRenderBuffer(rive::RenderBufferType,
                                                   rive::RenderBufferFlags,
                                                   size_t) override;

    rive::rcp<rive::gpu::Texture> makeImageTexture(
        uint32_t width,
        uint32_t height,
        uint32_t mipLevelCount,
        const uint8_t imageDataRGBA[]) override;

    std::unique_ptr<rive::gpu::BufferRing> makeUniformBufferRing(
        size_t capacityInBytes) override;
    std::unique_ptr<rive::gpu::BufferRing> makeStorageBufferRing(
        size_t capacityInBytes,
        rive::gpu::StorageBufferStructure) override;
    std::unique_ptr<rive::gpu::BufferRing> makeVertexBufferRing(
        size_t capacityInBytes) override;
    std::unique_ptr<rive::gpu::BufferRing> makeTextureTransferBufferRing(
        size_t capacityInBytes) override;

    void resizeGradientTexture(uint32_t width, uint32_t height) override {}
    void resizeTessellationTexture(uint32_t width, uint32_t height) override {}
    void resizeCoverageBuffer(size_t) override {}

    void flush(const rive::gpu::FlushDescriptor&) override {}
};
