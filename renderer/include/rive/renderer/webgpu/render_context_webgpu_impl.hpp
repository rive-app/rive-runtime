/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/renderer/render_context_helper_impl.hpp"
#include "rive/renderer/webgpu/em_js_handle.hpp"
#include "rive/renderer/gl/load_store_actions_ext.hpp"
#include <map>
#include <webgpu/webgpu_cpp.h>

namespace rive::gpu
{
class PLSRenderContextWebGPUVulkan;

class PLSRenderTargetWebGPU : public PLSRenderTarget
{
public:
    wgpu::TextureFormat framebufferFormat() const { return m_framebufferFormat; }

    void setTargetTextureView(wgpu::TextureView);

private:
    friend class PLSRenderContextWebGPUImpl;
    friend class PLSRenderContextWebGPUVulkan;

    PLSRenderTargetWebGPU(wgpu::Device device,
                          wgpu::TextureFormat framebufferFormat,
                          uint32_t width,
                          uint32_t height,
                          wgpu::TextureUsage additionalTextureFlags);

    const wgpu::TextureFormat m_framebufferFormat;

    wgpu::Texture m_coverageTexture;
    wgpu::Texture m_clipTexture;
    wgpu::Texture m_scratchColorTexture;

    wgpu::TextureView m_targetTextureView;
    wgpu::TextureView m_coverageTextureView;
    wgpu::TextureView m_clipTextureView;
    wgpu::TextureView m_scratchColorTextureView;
};

class PLSRenderContextWebGPUImpl : public PLSRenderContextHelperImpl
{
public:
    enum class PixelLocalStorageType
    {
        // Pixel local storage cannot be supported; make a best reasonable effort to draw shapes.
        none,

        // Backend is OpenGL ES 3.1+ and has GL_EXT_shader_pixel_local_storage. Use "raw-glsl"
        // shaders that take advantage of the extension.
        EXT_shader_pixel_local_storage,

        // Backend is Vulkan with VK_EXT_rasterization_order_attachment_access. Use nonstandard
        // WebGPU APIs to set up vulkan input attachments and subpassLoad() in shaders.
        subpassLoad,
    };

    struct ContextOptions
    {
        PixelLocalStorageType plsType = PixelLocalStorageType::none;
        bool disableStorageBuffers = false;
    };

    static std::unique_ptr<PLSRenderContext> MakeContext(
        wgpu::Device,
        wgpu::Queue,
        const ContextOptions&,
        const gpu::PlatformFeatures& baselinePlatformFeatures = {});

    virtual ~PLSRenderContextWebGPUImpl();

    virtual rcp<PLSRenderTargetWebGPU> makeRenderTarget(wgpu::TextureFormat,
                                                        uint32_t width,
                                                        uint32_t height);

    rcp<RenderBuffer> makeRenderBuffer(RenderBufferType, RenderBufferFlags, size_t) override;

    rcp<PLSTexture> makeImageTexture(uint32_t width,
                                     uint32_t height,
                                     uint32_t mipLevelCount,
                                     const uint8_t imageDataRGBA[]) override;

protected:
    PLSRenderContextWebGPUImpl(wgpu::Device device,
                               wgpu::Queue queue,
                               const ContextOptions&,
                               const gpu::PlatformFeatures& baselinePlatformFeatures);

    // Create the BindGroupLayout that binds the PLS attachments as textures. This is not necessary
    // on all implementations.
    virtual wgpu::BindGroupLayout initPLSTextureBindGroup()
    {
        // Only supported by PLSRenderContextWebGPUVulkan for now.
        RIVE_UNREACHABLE();
    }

    // Create a standard PLS "draw" pipeline for the current implementation.
    virtual wgpu::RenderPipeline makePLSDrawPipeline(rive::gpu::DrawType drawType,
                                                     wgpu::TextureFormat framebufferFormat,
                                                     wgpu::ShaderModule vertexShader,
                                                     wgpu::ShaderModule fragmentShader,
                                                     EmJsHandle* pipelineJSHandleIfNeeded);

    // Create a standard PLS "draw" render pass for the current implementation.
    virtual wgpu::RenderPassEncoder makePLSRenderPass(wgpu::CommandEncoder,
                                                      const PLSRenderTargetWebGPU*,
                                                      wgpu::LoadOp,
                                                      const wgpu::Color& clearColor,
                                                      EmJsHandle* renderPassJSHandleIfNeeded);

    wgpu::Device device() const { return m_device; }
    wgpu::FrontFace frontFaceForOnScreenDraws() const { return m_frontFaceForOnScreenDraws; }
    wgpu::PipelineLayout drawPipelineLayout() const { return m_drawPipelineLayout; }

private:
    // Called outside the constructor so we can use virtual methods.
    void initGPUObjects();

    // PLS always expects a clockwise front face.
    constexpr static wgpu::FrontFace kFrontFaceForOffscreenDraws = wgpu::FrontFace::CW;

    std::unique_ptr<BufferRing> makeUniformBufferRing(size_t capacityInBytes) override;
    std::unique_ptr<BufferRing> makeStorageBufferRing(size_t capacityInBytes,
                                                      gpu::StorageBufferStructure) override;
    std::unique_ptr<BufferRing> makeVertexBufferRing(size_t capacityInBytes) override;
    std::unique_ptr<BufferRing> makeTextureTransferBufferRing(size_t capacityInBytes) override;

    void resizeGradientTexture(uint32_t width, uint32_t height) override;
    void resizeTessellationTexture(uint32_t width, uint32_t height) override;

    void prepareToMapBuffers() override {}

    void flush(const FlushDescriptor&) override;

    const wgpu::Device m_device;
    const wgpu::Queue m_queue;
    const ContextOptions m_contextOptions;

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
    wgpu::BindGroupLayout m_drawBindGroupLayouts[4 /*BINDINGS_SET_COUNT*/];
    wgpu::Sampler m_linearSampler;
    wgpu::Sampler m_mipmapSampler;
    wgpu::BindGroup m_samplerBindings;
    wgpu::PipelineLayout m_drawPipelineLayout;
    wgpu::Buffer m_pathPatchVertexBuffer;
    wgpu::Buffer m_pathPatchIndexBuffer;
    wgpu::Texture m_nullImagePaintTexture; // Bound when there is not an image paint.
    wgpu::TextureView m_nullImagePaintTextureView;
};
} // namespace rive::gpu
