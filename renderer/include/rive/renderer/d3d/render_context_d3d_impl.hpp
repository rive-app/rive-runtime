/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/renderer/d3d/d3d11.hpp"
#include "rive/renderer/render_context_helper_impl.hpp"
#include <map>
#include <string>

namespace rive::gpu
{
class PLSRenderContextD3DImpl;

// D3D backend implementation of PLSRenderTarget.
class PLSRenderTargetD3D : public PLSRenderTarget
{
public:
    PLSRenderTargetD3D(PLSRenderContextD3DImpl*, uint32_t width, uint32_t height);
    ~PLSRenderTargetD3D() override {}

    void setTargetTexture(ComPtr<ID3D11Texture2D> tex);

    ID3D11Texture2D* targetTexture() const { return m_targetTexture.Get(); }
    bool targetTextureSupportsUAV() const { return m_targetTextureSupportsUAV; }
    ID3D11RenderTargetView* targetRTV();

    // Alternate rendering target when targetTextureSupportsUAV() is false.
    ID3D11Texture2D* offscreenTexture();

    // Returns an unordered access view of targetTexture(), if targetTextureSupportsUAV() is true,
    // otherwise returns a UAV of offscreenTexture().
    ID3D11UnorderedAccessView* targetUAV();

    ID3D11UnorderedAccessView* clipUAV();
    ID3D11UnorderedAccessView* scratchColorUAV();
    ID3D11UnorderedAccessView* coverageUAV();

private:
    const ComPtr<ID3D11Device> m_gpu;
    const bool m_gpuSupportsTypedUAVLoadStore;

    ComPtr<ID3D11Texture2D> m_targetTexture;
    bool m_targetTextureSupportsUAV = false;
    DXGI_FORMAT m_targetFormat = DXGI_FORMAT_UNKNOWN;

    ComPtr<ID3D11Texture2D> m_offscreenTexture;
    ComPtr<ID3D11Texture2D> m_coverageTexture;
    ComPtr<ID3D11Texture2D> m_scratchColorTexture;
    ComPtr<ID3D11Texture2D> m_clipTexture;

    ComPtr<ID3D11RenderTargetView> m_targetRTV;
    ComPtr<ID3D11UnorderedAccessView> m_targetUAV;
    ComPtr<ID3D11UnorderedAccessView> m_coverageUAV;
    ComPtr<ID3D11UnorderedAccessView> m_clipUAV;
    ComPtr<ID3D11UnorderedAccessView> m_scratchColorUAV;
};

// D3D backend implementation of PLSRenderContextImpl.
class PLSRenderContextD3DImpl : public PLSRenderContextHelperImpl
{
public:
    struct ContextOptions
    {
        bool disableRasterizerOrderedViews = false; // Primarily for testing.
        bool disableTypedUAVLoadStore = false;      // Primarily for testing.
        bool isIntel = false;
    };

    static std::unique_ptr<PLSRenderContext> MakeContext(ComPtr<ID3D11Device>,
                                                         ComPtr<ID3D11DeviceContext>,
                                                         const ContextOptions&);

    rcp<PLSRenderTargetD3D> makeRenderTarget(uint32_t width, uint32_t height)
    {
        return make_rcp<PLSRenderTargetD3D>(this, width, height);
    }

    struct D3DCapabilities
    {
        bool supportsRasterizerOrderedViews = false;
        bool supportsTypedUAVLoadStore = false; // Can we load/store all UAV formats used by Rive?
        bool supportsMin16Precision = false;    // Can we use minimum 16-bit types (e.g. min16int)?
        bool isIntel = false;
    };

    const D3DCapabilities& d3dCapabilities() const { return m_d3dCapabilities; }
    ID3D11Device* gpu() const { return m_gpu.Get(); }
    ID3D11DeviceContext* gpuContext() const { return m_gpuContext.Get(); }

    // D3D helpers
    ComPtr<ID3D11Texture2D> makeSimple2DTexture(DXGI_FORMAT format,
                                                UINT width,
                                                UINT height,
                                                UINT mipLevelCount,
                                                UINT bindFlags,
                                                UINT miscFlags = 0);
    ComPtr<ID3D11UnorderedAccessView> makeSimple2DUAV(ID3D11Texture2D* tex, DXGI_FORMAT format);
    ComPtr<ID3D11Buffer> makeSimpleImmutableBuffer(size_t sizeInBytes,
                                                   UINT bindFlags,
                                                   const void* data);
    ComPtr<ID3DBlob> compileSourceToBlob(const char* shaderTypeDefineName,
                                         const std::string& commonSource,
                                         const char* entrypoint,
                                         const char* target);

private:
    PLSRenderContextD3DImpl(ComPtr<ID3D11Device>,
                            ComPtr<ID3D11DeviceContext>,
                            const D3DCapabilities&);

    rcp<RenderBuffer> makeRenderBuffer(RenderBufferType, RenderBufferFlags, size_t) override;

    rcp<PLSTexture> makeImageTexture(uint32_t width,
                                     uint32_t height,
                                     uint32_t mipLevelCount,
                                     const uint8_t imageDataRGBA[]) override;

    std::unique_ptr<BufferRing> makeUniformBufferRing(size_t capacityInBytes) override;
    std::unique_ptr<BufferRing> makeStorageBufferRing(size_t capacityInBytes,
                                                      gpu::StorageBufferStructure) override;
    std::unique_ptr<BufferRing> makeVertexBufferRing(size_t capacityInBytes) override;
    std::unique_ptr<BufferRing> makeTextureTransferBufferRing(size_t capacityInBytes) override;

    void resizeGradientTexture(uint32_t width, uint32_t height) override;
    void resizeTessellationTexture(uint32_t width, uint32_t height) override;

    void flush(const FlushDescriptor&) override;

    template <typename HighLevelStruct>
    ID3D11ShaderResourceView* replaceStructuredBufferSRV(const BufferRing*,
                                                         UINT highLevelStructCount,
                                                         UINT firstHighLevelStruct);

    void setPipelineLayoutAndShaders(DrawType,
                                     gpu::ShaderFeatures,
                                     gpu::InterlockMode,
                                     gpu::ShaderMiscFlags pixelShaderMiscFlags);

    const D3DCapabilities m_d3dCapabilities;

    ComPtr<ID3D11Device> m_gpu;
    ComPtr<ID3D11DeviceContext> m_gpuContext;

    ComPtr<ID3D11Texture2D> m_gradTexture;
    ComPtr<ID3D11ShaderResourceView> m_gradTextureSRV;
    ComPtr<ID3D11RenderTargetView> m_gradTextureRTV;

    ComPtr<ID3D11Texture2D> m_tessTexture;
    ComPtr<ID3D11ShaderResourceView> m_tessTextureSRV;
    ComPtr<ID3D11RenderTargetView> m_tessTextureRTV;

    ComPtr<ID3D11RasterizerState> m_backCulledRasterState[2];
    ComPtr<ID3D11RasterizerState> m_doubleSidedRasterState[2];

    ComPtr<ID3D11InputLayout> m_colorRampLayout;
    ComPtr<ID3D11VertexShader> m_colorRampVertexShader;
    ComPtr<ID3D11PixelShader> m_colorRampPixelShader;

    ComPtr<ID3D11InputLayout> m_tessellateLayout;
    ComPtr<ID3D11VertexShader> m_tessellateVertexShader;
    ComPtr<ID3D11PixelShader> m_tessellatePixelShader;
    ComPtr<ID3D11Buffer> m_tessSpanIndexBuffer;

    struct DrawVertexShader
    {
        ComPtr<ID3D11InputLayout> layout;
        ComPtr<ID3D11VertexShader> shader;
    };
    std::map<uint32_t, DrawVertexShader> m_drawVertexShaders;
    std::map<uint32_t, ComPtr<ID3D11PixelShader>> m_drawPixelShaders;

    // Vertex/index buffers for drawing path patches.
    ComPtr<ID3D11Buffer> m_patchVertexBuffer;
    ComPtr<ID3D11Buffer> m_patchIndexBuffer;

    // Vertex/index buffers for drawing image rects. (gpu::InterlockMode::atomics only.)
    ComPtr<ID3D11Buffer> m_imageRectVertexBuffer;
    ComPtr<ID3D11Buffer> m_imageRectIndexBuffer;

    struct DrawUniforms
    {
        DrawUniforms(uint32_t baseInstance_) : baseInstance(baseInstance_) {}
        uint32_t baseInstance;
        uint32_t pad0;
        uint32_t pad1;
        uint32_t pad2;
    };
    static_assert(sizeof(DrawUniforms) == 16);

    ComPtr<ID3D11Buffer> m_flushUniforms;
    ComPtr<ID3D11Buffer> m_drawUniforms;
    ComPtr<ID3D11Buffer> m_imageDrawUniforms;

    ComPtr<ID3D11SamplerState> m_linearSampler;
    ComPtr<ID3D11SamplerState> m_mipmapSampler;

    ComPtr<ID3D11BlendState> m_srcOverBlendState;
};
} // namespace rive::gpu
