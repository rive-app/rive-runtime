/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/pls/pls_render_context_helper_impl.hpp"
#include "rive/pls/webgpu/em_js_handle.hpp"
#include "rive/pls/gl/load_store_actions_ext.hpp"
#include <map>
#include <webgpu/webgpu_cpp.h>

namespace rive::pls
{
class PLSRenderTargetWebGPU : public PLSRenderTarget
{
public:
    wgpu::TextureFormat framebufferFormat() const { return m_framebufferFormat; }

    void setTargetTextureView(wgpu::TextureView);

private:
    friend class PLSRenderContextWebGPUImpl;

    PLSRenderTargetWebGPU(wgpu::Device device,
                          wgpu::TextureFormat framebufferFormat,
                          size_t width,
                          size_t height);

    const wgpu::TextureFormat m_framebufferFormat;

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
    enum class PixelLocalStorageType
    {
        // Backend is OpenGL ES 3.1+ and has GL_EXT_shader_pixel_local_storage. Use "raw-glsl"
        // shaders that take advantage of the extension.
        EXT_shader_pixel_local_storage,

        // Pixel local storage cannot be supported; make a best reasonable effort to draw shapes.
        bestEffort,
    };

    static std::unique_ptr<PLSRenderContext> MakeContext(
        wgpu::Device,
        wgpu::Queue,
        const pls::PlatformFeatures& baselinePlatformFeatures = {},
        PixelLocalStorageType = PixelLocalStorageType::bestEffort);

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
    PLSRenderContextWebGPUImpl(wgpu::Device device,
                               wgpu::Queue queue,
                               const pls::PlatformFeatures& baselinePlatformFeatures,
                               PixelLocalStorageType);

    // PLS always expects a clockwise front face.
    constexpr static wgpu::FrontFace kFrontFaceForOffscreenDraws = wgpu::FrontFace::CW;

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

    const wgpu::Device m_device;
    const wgpu::Queue m_queue;
    const PixelLocalStorageType m_pixelLocalStorageType;

    // PLS always expects CW, but when using "glsl-raw" shaders, we may need to use CCW. This is
    // because the WebGPU layer might actually flip our frontFace if it anticipates negating our Y
    // coordinate in the vertex shader. But when we use raw-glsl shaders, the WebGPU layer doesn't
    // actually get a chance to negate Y like it thinks it will. Therefore, we emit the wrong
    // frontFace, in anticipation of it getting flipped into the correct frontFace on its way to the
    // driver.
    wgpu::FrontFace m_frontFaceForOnScreenDraws;

    // Draws emulated render-pass load/store actions for EXT_shader_pixel_local_storage.
    class LoadStoreEXTPipeline;
    std::map<LoadStoreActionsEXT, LoadStoreEXTPipeline> m_loadStoreEXTPipelines;
    EmJsHandle m_loadStoreEXTVertexShaderHandle;
    wgpu::ShaderModule m_loadStoreEXTVertexShader;
    std::unique_ptr<BufferRing> m_loadStoreEXTUniforms;

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
    wgpu::BindGroupLayout m_drawBindGroupLayouts[2];
    wgpu::Sampler m_linearSampler;
    wgpu::Sampler m_mipmapSampler;
    wgpu::BindGroup m_samplerBindGroup;
    wgpu::PipelineLayout m_drawPipelineLayout;
    wgpu::Buffer m_pathPatchVertexBuffer;
    wgpu::Buffer m_pathPatchIndexBuffer;
    wgpu::Texture m_nullImagePaintTexture; // Bound when there is not an image paint.
    wgpu::TextureView m_nullImagePaintTextureView;
};
} // namespace rive::pls
