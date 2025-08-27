/*
 * Copyright 2025 Rive
 */
#pragma once
#include "rive/renderer/d3d12/d3d12.hpp"
#include "rive/renderer/d3d/pipeline_manager.hpp"
#include "rive/renderer/gpu.hpp"
// holds all shader stuff including inputlayouts, source blobs and pipeline
// states
struct D3D12DrawVertexShader
{
    D3D12_INPUT_ELEMENT_DESC m_layoutDesc[2];
    uint32_t m_vertexAttribCount;
    ComPtr<ID3DBlob> m_shader;
};

namespace rive::gpu
{
struct D3D12Pipeline
{
    using VertexShaderType = D3D12DrawVertexShader;
    using PixelShaderType = ComPtr<ID3DBlob>;

    ComPtr<ID3D12PipelineState> m_d3dPipelineState;

    bool succeeded() const { return m_d3dPipelineState != nullptr; }
};

class D3D12PipelineManager
    : public D3DPipelineManager<D3D12Pipeline, ID3D12Device>
{
    using Super = D3DPipelineManager<D3D12Pipeline, ID3D12Device>;

public:
    D3D12PipelineManager(ComPtr<ID3D12Device>,
                         const D3DCapabilities&,
                         ShaderCompilationMode);

    ~D3D12PipelineManager() { shutdownBackgroundThread(); }

    void compileTesselationPipeline();
    void compileGradientPipeline();
    void compileAtlasPipeline();

    void setRootSig(ID3D12GraphicsCommandList* cmdList) const
    {
        assert(m_rootSignature);
        cmdList->SetGraphicsRootSignature(m_rootSignature.Get());
    }

    void setTesselationPipeline(ID3D12GraphicsCommandList* cmdList) const
    {
        assert(m_tesselationPipeline);
        cmdList->SetPipelineState(m_tesselationPipeline.Get());
    }

    void setAtlasFillPipeline(ID3D12GraphicsCommandList* cmdList) const
    {
        assert(m_atlasFillPipeline);
        cmdList->SetPipelineState(m_atlasFillPipeline.Get());
    }

    void setAtlasStrokePipeline(ID3D12GraphicsCommandList* cmdList) const
    {
        assert(m_atlasStrokePipeline);
        cmdList->SetPipelineState(m_atlasStrokePipeline.Get());
    }

    void setGradientPipeline(ID3D12GraphicsCommandList* cmdList) const
    {
        assert(m_gradientPipeline);
        cmdList->SetPipelineState(m_gradientPipeline.Get());
    }

protected:
    virtual D3D12DrawVertexShader compileVertexShaderBlobToFinalType(
        DrawType,
        ComPtr<ID3DBlob>) override;

    virtual ComPtr<ID3DBlob> compilePixelShaderBlobToFinalType(
        ComPtr<ID3DBlob> blob) override
    {
        return blob;
    }

    virtual D3D12Pipeline linkPipeline(const PipelineProps&,
                                       D3D12DrawVertexShader&&,
                                       ComPtr<ID3DBlob>&&) override;

private:
    // maybe these could be moved to D3DPipelineState but to do so
    // required a lot of extra complexity that didnt seem worth it
    ComPtr<ID3D12PipelineState> m_tesselationPipeline;
    ComPtr<ID3D12PipelineState> m_atlasStrokePipeline;
    ComPtr<ID3D12PipelineState> m_atlasFillPipeline;
    ComPtr<ID3D12PipelineState> m_gradientPipeline;

    ComPtr<ID3D12RootSignature> m_rootSignature;
};
} // namespace rive::gpu