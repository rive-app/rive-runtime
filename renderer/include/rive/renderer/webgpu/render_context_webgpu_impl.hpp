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
class RenderContextWebGPUVulkan;

class RenderTargetWebGPU : public RenderTarget
{
public:
    wgpu::TextureFormat framebufferFormat() const
    {
        return m_framebufferFormat;
    }

    void setTargetTextureView(wgpu::TextureView);

private:
    friend class RenderContextWebGPUImpl;
    friend class RenderContextWebGPUVulkan;

    RenderTargetWebGPU(wgpu::Device device,
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

class RenderContextWebGPUImpl : public RenderContextHelperImpl
{
public:
    enum class PixelLocalStorageType
    {
        // Pixel local storage cannot be supported; make a best reasonable
        // effort to draw shapes.
        none,

        // Backend is OpenGL ES 3.1+ and has GL_EXT_shader_pixel_local_storage.
        // Use "raw-glsl" shaders that take advantage of the extension.
        EXT_shader_pixel_local_storage,

        // Backend is Vulkan with VK_EXT_rasterization_order_attachment_access.
        // Use nonstandard WebGPU APIs to set up vulkan input attachments and
        // subpassLoad() in shaders.
        subpassLoad,
    };

    struct ContextOptions
    {
        PixelLocalStorageType plsType = PixelLocalStorageType::none;
        bool disableStorageBuffers = false;
        // Invert Y when drawing to client-provided RenderTargets.
        // TODO: We may need to eventually make this configurable
        // per-RenderTarget.
        bool invertRenderTargetY = false;
        // Invert the front face when drawing to client-provied RenderTargets.
        bool invertRenderTargetFrontFace = false;
    };

    static std::unique_ptr<RenderContext> MakeContext(wgpu::Device,
                                                      wgpu::Queue,
                                                      const ContextOptions&);

    virtual ~RenderContextWebGPUImpl();

    virtual rcp<RenderTargetWebGPU> makeRenderTarget(wgpu::TextureFormat,
                                                     uint32_t width,
                                                     uint32_t height);

    rcp<RenderBuffer> makeRenderBuffer(RenderBufferType,
                                       RenderBufferFlags,
                                       size_t) override;

    rcp<Texture> makeImageTexture(uint32_t width,
                                  uint32_t height,
                                  uint32_t mipLevelCount,
                                  const uint8_t imageDataRGBAPremul[]) override;

protected:
    RenderContextWebGPUImpl(wgpu::Device device,
                            wgpu::Queue queue,
                            const ContextOptions&);

    // Create the BindGroupLayout that binds the PLS attachments as textures.
    // This is not necessary on all implementations.
    virtual wgpu::BindGroupLayout initTextureBindGroup()
    {
        // Only supported by RenderContextWebGPUVulkan for now.
        RIVE_UNREACHABLE();
    }

    // Create a standard PLS "draw" pipeline for the current implementation.
    virtual wgpu::RenderPipeline makeDrawPipeline(
        rive::gpu::DrawType drawType,
        wgpu::TextureFormat framebufferFormat,
        wgpu::ShaderModule vertexShader,
        wgpu::ShaderModule fragmentShader,
        EmJsHandle* pipelineJSHandleIfNeeded);

    // Create a standard PLS "draw" render pass for the current implementation.
    virtual wgpu::RenderPassEncoder makePLSRenderPass(
        wgpu::CommandEncoder,
        const RenderTargetWebGPU*,
        wgpu::LoadOp,
        const wgpu::Color& clearColor,
        EmJsHandle* renderPassJSHandleIfNeeded);

    wgpu::Device device() const { return m_device; }
    wgpu::FrontFace frontFaceForRenderTargetDraws() const
    {
        return m_contextOptions.invertRenderTargetFrontFace
                   ? wgpu::FrontFace::CCW
                   : wgpu::FrontFace::CW;
    }
    wgpu::PipelineLayout drawPipelineLayout() const
    {
        return m_drawPipelineLayout;
    }

private:
    // Called outside the constructor so we can use virtual methods.
    void initGPUObjects();

    void generateMipmaps(wgpu::Texture);

    // PLS always expects a clockwise front face.
    constexpr static wgpu::FrontFace kFrontFaceForOffscreenDraws =
        wgpu::FrontFace::CW;

    std::unique_ptr<BufferRing> makeUniformBufferRing(
        size_t capacityInBytes) override;
    std::unique_ptr<BufferRing> makeStorageBufferRing(
        size_t capacityInBytes,
        gpu::StorageBufferStructure) override;
    std::unique_ptr<BufferRing> makeVertexBufferRing(
        size_t capacityInBytes) override;

    void resizeGradientTexture(uint32_t width, uint32_t height) override;
    void resizeTessellationTexture(uint32_t width, uint32_t height) override;
    void resizeAtlasTexture(uint32_t width, uint32_t height) override;

    void flush(const FlushDescriptor&) override;

    const wgpu::Device m_device;
    const wgpu::Queue m_queue;
    const ContextOptions m_contextOptions;

    constexpr static int COLOR_RAMP_BINDINGS_COUNT = 1;
    constexpr static int TESS_BINDINGS_COUNT = 6;
    constexpr static int ATLAS_BINDINGS_COUNT = 7;
    constexpr static int DRAW_BINDINGS_COUNT = 10;
    std::array<wgpu::BindGroupLayoutEntry, DRAW_BINDINGS_COUNT>
        m_perFlushBindingLayouts;

    // Draws emulated render-pass load/store actions for
    // EXT_shader_pixel_local_storage.
    class LoadStoreEXTPipeline;
    std::map<LoadStoreActionsEXT, LoadStoreEXTPipeline> m_loadStoreEXTPipelines;
    EmJsHandle m_loadStoreEXTVertexShaderHandle;
    wgpu::ShaderModule m_loadStoreEXTVertexShader;
    std::unique_ptr<BufferRing> m_loadStoreEXTUniforms;

    // Blits texture-to-texture using a draw command.
    class BlitTextureAsDrawPipeline;
    std::unique_ptr<BlitTextureAsDrawPipeline> m_blitTextureAsDrawPipeline;

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

    // Renders feathers to the atlas.
    class AtlasPipeline;
    std::unique_ptr<AtlasPipeline> m_atlasPipeline;
    wgpu::Texture m_atlasTexture;
    wgpu::TextureView m_atlasTextureView;

    // Draw paths and image meshes using the gradient and tessellation textures.
    class DrawPipeline;
    std::map<uint32_t, DrawPipeline> m_drawPipelines;
    wgpu::BindGroupLayout m_drawBindGroupLayouts[4 /*BINDINGS_SET_COUNT*/];
    wgpu::Sampler m_linearSampler;
    wgpu::Sampler m_mipmapSampler;
    wgpu::BindGroup m_samplerBindings;
    wgpu::PipelineLayout m_drawPipelineLayout;
    wgpu::BindGroupLayout m_emptyBindingsLayout; // For when a set is unused.
    wgpu::Buffer m_pathPatchVertexBuffer;
    wgpu::Buffer m_pathPatchIndexBuffer;

    // Gaussian integral table for feathering.
    wgpu::Texture m_featherTexture;
    wgpu::TextureView m_featherTextureView;

    // Bound when there is not an image paint.
    wgpu::Texture m_nullImagePaintTexture;
    wgpu::TextureView m_nullImagePaintTextureView;
};
} // namespace rive::gpu
