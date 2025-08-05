/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/renderer/render_context_helper_impl.hpp"
#include <map>
#include <webgpu/webgpu_cpp.h>

#ifdef RIVE_WAGYU
#include "rive/renderer/gl/load_store_actions_ext.hpp"
#endif

namespace rive::gpu
{
class RenderContextWebGPUVulkan;
class RenderTargetWebGPU;

class RenderContextWebGPUImpl : public RenderContextHelperImpl
{
public:
    struct ContextOptions
    {
        // Invert Y when drawing to client-provided RenderTargets.
        // TODO: We may need to eventually make this configurable
        // per-RenderTarget.
        bool invertRenderTargetY = false;
        // Invert the front face when drawing to client-provied RenderTargets.
        bool invertRenderTargetFrontFace = false;
    };

    enum class PixelLocalStorageType
    {
        // Pixel local storage cannot be supported; make a best reasonable
        // effort to draw shapes.
        none,

#ifdef RIVE_WAGYU
        // Backend is OpenGL ES 3.1+ and has GL_EXT_shader_pixel_local_storage.
        // Use "raw-glsl" shaders that take advantage of the extension.
        GL_EXT_shader_pixel_local_storage,

        // Backend is Vulkan with VK_EXT_rasterization_order_attachment_access.
        // Use nonstandard WebGPU APIs to set up vulkan input attachments and
        // subpassLoad() in shaders.
        VK_EXT_rasterization_order_attachment_access,
#endif
    };

    struct Capabilities
    {
#ifdef RIVE_WAGYU
        PixelLocalStorageType plsType = PixelLocalStorageType::none;

        // Rive requires 4 storage buffers in the vertex shader. We polyfill
        // them if the hardware doesn't support this.
        bool polyfillVertexStorageBuffers = false;
#endif
    };

    static std::unique_ptr<RenderContext> MakeContext(wgpu::Adapter,
                                                      wgpu::Device,
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

private:
    RenderContextWebGPUImpl(wgpu::Adapter,
                            wgpu::Device,
                            wgpu::Queue,
                            const ContextOptions&);

    // Create a standard PLS "draw" pipeline for the current implementation.
    wgpu::RenderPipeline makeDrawPipeline(rive::gpu::DrawType drawType,
                                          wgpu::TextureFormat framebufferFormat,
                                          wgpu::ShaderModule vertexShader,
                                          wgpu::ShaderModule fragmentShader);

    // Create a standard PLS "draw" render pass for the current implementation.
    wgpu::RenderPassEncoder makePLSRenderPass(wgpu::CommandEncoder,
                                              const RenderTargetWebGPU*,
                                              wgpu::LoadOp,
                                              const wgpu::Color& clearColor);

    wgpu::Device device() const { return m_device; }
    const ContextOptions& contextOptions() const { return m_contextOptions; }
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
    Capabilities m_capabilities;

    constexpr static int COLOR_RAMP_BINDINGS_COUNT = 1;
    constexpr static int TESS_BINDINGS_COUNT = 6;
    constexpr static int ATLAS_BINDINGS_COUNT = 7;
    constexpr static int DRAW_BINDINGS_COUNT = 10;
    std::array<wgpu::BindGroupLayoutEntry, DRAW_BINDINGS_COUNT>
        m_perFlushBindingLayouts;

#ifdef RIVE_WAGYU
    // Draws emulated render-pass load/store actions for
    // EXT_shader_pixel_local_storage.
    class LoadStoreEXTPipeline;
    std::map<LoadStoreActionsEXT, LoadStoreEXTPipeline> m_loadStoreEXTPipelines;
    wgpu::ShaderModule m_loadStoreEXTVertexShader;
    std::unique_ptr<BufferRing> m_loadStoreEXTUniforms;
#endif

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
    wgpu::Sampler m_imageSamplers[ImageSampler::MAX_SAMPLER_PERMUTATIONS];
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
                       const RenderContextWebGPUImpl::Capabilities&,
                       wgpu::TextureFormat framebufferFormat,
                       uint32_t width,
                       uint32_t height);

    const wgpu::TextureFormat m_framebufferFormat;

    wgpu::Texture m_coverageTexture;
    wgpu::Texture m_clipTexture;
    wgpu::Texture m_scratchColorTexture;

    wgpu::TextureView m_targetTextureView;
    wgpu::TextureView m_coverageTextureView;
    wgpu::TextureView m_clipTextureView;
    wgpu::TextureView m_scratchColorTextureView;
};
} // namespace rive::gpu
