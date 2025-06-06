/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/renderer/d3d/pipeline_manager.hpp"
#include "rive/renderer/d3d11/d3d11.hpp"
#include "rive/renderer/render_context_helper_impl.hpp"
#include <map>
#include <string>

namespace rive::gpu
{
class RenderContextD3DImpl;

// D3D backend implementation of RenderTarget.
class RenderTargetD3D : public RenderTarget
{
public:
    RenderTargetD3D(RenderContextD3DImpl*, uint32_t width, uint32_t height);
    ~RenderTargetD3D() override {}

    void setTargetTexture(ComPtr<ID3D11Texture2D> tex);

    ID3D11Texture2D* targetTexture() const { return m_targetTexture.Get(); }
    bool targetTextureSupportsUAV() const { return m_targetTextureSupportsUAV; }
    ID3D11RenderTargetView* targetRTV();

    // Alternate rendering target when targetTextureSupportsUAV() is false.
    ID3D11Texture2D* offscreenTexture();

    // Returns an unordered access view of targetTexture(), if
    // targetTextureSupportsUAV() is true, otherwise returns a UAV of
    // offscreenTexture().
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

struct D3D11DrawVertexShader
{
    ComPtr<ID3D11InputLayout> layout;
    ComPtr<ID3D11VertexShader> shader;
};

class D3D11PipelineManager
    : public D3DPipelineManager<D3D11DrawVertexShader,
                                ComPtr<ID3D11PixelShader>,
                                ID3D11Device>
{
public:
    D3D11PipelineManager(ComPtr<ID3D11DeviceContext> context,
                         ComPtr<ID3D11Device> device,
                         const D3DCapabilities& capabilities);

    void setPipelineState(rive::gpu::DrawType,
                          rive::gpu::ShaderFeatures,
                          rive::gpu::InterlockMode,
                          rive::gpu::ShaderMiscFlags);

    void setColorRampState() const
    {
        m_context->IASetInputLayout(m_colorRampLayout.Get());
        m_context->VSSetShader(m_colorRampVertexShader.Get(), NULL, 0);
        m_context->PSSetShader(m_colorRampPixelShader.Get(), NULL, 0);
    }

    void setTesselationState() const
    {
        m_context->IASetInputLayout(m_tessellateLayout.Get());
        m_context->VSSetShader(m_tessellateVertexShader.Get(), NULL, 0);
        m_context->PSSetShader(m_tessellatePixelShader.Get(), NULL, 0);
    }

    void setAtlasVertexState() const
    {
        m_context->IASetInputLayout(m_atlasLayout.Get());
        m_context->VSSetShader(m_atlasVertexShader.Get(), NULL, 0);
    }

    void setAtlasFillState() const
    {
        m_context->PSSetShader(m_atlasFillPixelShader.Get(), NULL, 0);
    }

    void setAtlasStrokeState() const
    {
        m_context->PSSetShader(m_atlasStrokePixelShader.Get(), NULL, 0);
    }

protected:
    virtual void compileBlobToFinalType(const ShaderCompileRequest&,
                                        ComPtr<ID3DBlob> vertexShader,
                                        ComPtr<ID3DBlob> pixelShader,
                                        ShaderCompileResult*) override;

private:
    ComPtr<ID3D11DeviceContext> m_context;

    ComPtr<ID3D11InputLayout> m_colorRampLayout;
    ComPtr<ID3D11VertexShader> m_colorRampVertexShader;
    ComPtr<ID3D11PixelShader> m_colorRampPixelShader;

    ComPtr<ID3D11InputLayout> m_tessellateLayout;
    ComPtr<ID3D11VertexShader> m_tessellateVertexShader;
    ComPtr<ID3D11PixelShader> m_tessellatePixelShader;

    ComPtr<ID3D11InputLayout> m_atlasLayout;
    ComPtr<ID3D11VertexShader> m_atlasVertexShader;
    ComPtr<ID3D11PixelShader> m_atlasFillPixelShader;
    ComPtr<ID3D11PixelShader> m_atlasStrokePixelShader;
};

// D3D backend implementation of RenderContextImpl.
class RenderContextD3DImpl : public RenderContextHelperImpl
{
public:
    static std::unique_ptr<RenderContext> MakeContext(
        ComPtr<ID3D11Device>,
        ComPtr<ID3D11DeviceContext>,
        const D3DContextOptions&);

    rcp<RenderTargetD3D> makeRenderTarget(uint32_t width, uint32_t height)
    {
        return make_rcp<RenderTargetD3D>(this, width, height);
    }

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
    ComPtr<ID3D11UnorderedAccessView> makeSimple2DUAV(ID3D11Texture2D* tex,
                                                      DXGI_FORMAT format);
    ComPtr<ID3D11Buffer> makeSimpleImmutableBuffer(size_t sizeInBytes,
                                                   UINT bindFlags,
                                                   const void* data);

private:
    RenderContextD3DImpl(ComPtr<ID3D11Device>,
                         ComPtr<ID3D11DeviceContext>,
                         const D3DCapabilities&);

    rcp<RenderBuffer> makeRenderBuffer(RenderBufferType,
                                       RenderBufferFlags,
                                       size_t) override;

    rcp<Texture> makeImageTexture(uint32_t width,
                                  uint32_t height,
                                  uint32_t mipLevelCount,
                                  const uint8_t imageDataRGBAPremul[]) override;

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

    D3D11PipelineManager m_pipelineManager;

    const D3DCapabilities m_d3dCapabilities;

    ComPtr<ID3D11Device> m_gpu;
    ComPtr<ID3D11DeviceContext> m_gpuContext;

    ComPtr<ID3D11Texture2D> m_gradTexture;
    ComPtr<ID3D11ShaderResourceView> m_gradTextureSRV;
    ComPtr<ID3D11RenderTargetView> m_gradTextureRTV;

    // Gaussian integral table for feathering.
    ComPtr<ID3D11Texture1D> m_featherTexture;
    ComPtr<ID3D11ShaderResourceView> m_featherTextureSRV;

    ComPtr<ID3D11Texture2D> m_tessTexture;
    ComPtr<ID3D11ShaderResourceView> m_tessTextureSRV;
    ComPtr<ID3D11RenderTargetView> m_tessTextureRTV;
    ComPtr<ID3D11Buffer> m_tessSpanIndexBuffer;

    ComPtr<ID3D11Texture2D> m_atlasTexture;
    ComPtr<ID3D11ShaderResourceView> m_atlasTextureSRV;
    ComPtr<ID3D11RenderTargetView> m_atlasTextureRTV;

    // Vertex/index buffers for drawing path patches.
    ComPtr<ID3D11Buffer> m_patchVertexBuffer;
    ComPtr<ID3D11Buffer> m_patchIndexBuffer;

    // Vertex/index buffers for drawing image rects.
    // (gpu::InterlockMode::atomics only.)
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
    ComPtr<ID3D11SamplerState>
        m_samplerStates[rive::ImageSampler::MAX_SAMPLER_PERMUTATIONS];

    ComPtr<ID3D11RasterizerState> m_atlasRasterState;
    ComPtr<ID3D11RasterizerState> m_backCulledRasterState[2];
    ComPtr<ID3D11RasterizerState> m_doubleSidedRasterState[2];

    ComPtr<ID3D11BlendState> m_srcOverBlendState;
    ComPtr<ID3D11BlendState> m_plusBlendState;
    ComPtr<ID3D11BlendState> m_maxBlendState;
};
} // namespace rive::gpu
