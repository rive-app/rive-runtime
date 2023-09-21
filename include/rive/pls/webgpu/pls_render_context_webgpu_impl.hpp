/*
 * Copyright 2023 Rive
 */

#pragma once

// For GLExtensions.
#include "rive/pls/gl/gles3.hpp"

#include "rive/pls/pls_render_context_helper_impl.hpp"

#include <map>
#include <webgpu/webgpu_cpp.h>

namespace rive::pls
{
class PLSRenderTargetWebGPU : public PLSRenderTarget
{
public:
    wgpu::TextureFormat pixelFormat() const { return m_pixelFormat; }

    void setTargetTextureView(wgpu::TextureView);

private:
    friend class PLSRenderContextWebGPUImpl;

    PLSRenderTargetWebGPU(wgpu::Device device,
                          wgpu::TextureFormat pixelFormat,
                          size_t width,
                          size_t height);

    const wgpu::TextureFormat m_pixelFormat;

    wgpu::Texture m_coverageMemorylessTexture;
    wgpu::Texture m_originalDstColorMemorylessTexture;
    wgpu::Texture m_clipMemorylessTexture;

    wgpu::TextureView m_targetTextureView;
    wgpu::TextureView m_coverageMemorylessTextureView;
    wgpu::TextureView m_originalDstColorMemorylessTextureView;
    wgpu::TextureView m_clipMemorylessTextureView;
};

class PLSRenderContextWebGPUImpl : public PLSRenderContextHelperImpl
{
public:
    static std::unique_ptr<PLSRenderContext> MakeContext(wgpu::Device, wgpu::Queue);

    rcp<PLSRenderTargetWebGPU> makeRenderTarget(wgpu::TextureFormat, size_t width, size_t height);

    rcp<RenderBuffer> makeRenderBuffer(RenderBufferType, RenderBufferFlags, size_t) override
    {
        return nullptr;
    }

    rcp<PLSTexture> makeImageTexture(uint32_t width,
                                     uint32_t height,
                                     uint32_t mipLevelCount,
                                     const uint8_t imageDataRGBA[]) override
    {
        return nullptr;
    }

private:
    PLSRenderContextWebGPUImpl(wgpu::Device device, wgpu::Queue queue, GLExtensions extensions);

    std::unique_ptr<BufferRing> makeVertexBufferRing(size_t capacity,
                                                     size_t itemSizeInBytes) override;

    std::unique_ptr<TexelBufferRing> makeTexelBufferRing(TexelBufferRing::Format,
                                                         size_t widthInItems,
                                                         size_t height,
                                                         size_t texelsPerItem,
                                                         int textureIdx,
                                                         TexelBufferRing::Filter) override;

    std::unique_ptr<BufferRing> makePixelUnpackBufferRing(size_t capacity,
                                                          size_t itemSizeInBytes) override;
    std::unique_ptr<BufferRing> makeUniformBufferRing(size_t capacity,
                                                      size_t itemSizeInBytes) override;

    void resizeGradientTexture(size_t height) override;
    void resizeTessellationTexture(size_t height) override;

    void prepareToMapBuffers() override {}

    void flush(const PLSRenderContext::FlushDescriptor&) override;

    wgpu::ShaderModule DrawModule(GLExtensions extensions,
                                  const ShaderFeatures& shaderFeatures,
                                  GLuint type,
                                  DrawType drawType);

    wgpu::Device m_device;
    wgpu::Queue m_queue;

    wgpu::BindGroupLayout m_drawBindGroupLayouts[2];
    wgpu::Sampler m_linearSampler;
    wgpu::Sampler m_mipmapSampler;
    wgpu::BindGroup m_samplerBindGroup;
    wgpu::PipelineLayout m_drawPipelineLayout;

    wgpu::ShaderModule m_plsLoadStoreModule;

    // Renders color ramps to the gradient texture.
    class ColorRampPipeline;
    std::unique_ptr<ColorRampPipeline> m_colorRampPipeline;
    wgpu::Texture m_gradientTexture;
    wgpu::TextureView m_gradientTextureView;

    // Renders tessellated vertices to the tessellation texture.
    class TessellatePipeline;
    std::unique_ptr<TessellatePipeline> m_tessellatePipeline;
    wgpu::Buffer m_tessSpanIndexBuffer;
    wgpu::Texture m_tessVertexTexture;
    wgpu::TextureView m_tessVertexTextureView;

    // Draw paths and image meshes using the gradient and tessellation textures.
    class DrawPipeline;
    std::map<uint32_t, DrawPipeline> m_drawPipelines;
    wgpu::Buffer m_pathPatchVertexBuffer;
    wgpu::Buffer m_pathPatchIndexBuffer;
    wgpu::Texture m_nullImagePaintTexture; // Bound when there is not an image paint.
    wgpu::TextureView m_nullImagePaintTextureView;
};
} // namespace rive::pls
