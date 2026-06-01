#pragma once
#include "rive/renderer/ore/ore_pipeline.hpp"
#include "ore_shader_module_d3d11.hpp"
#include <d3d11.h>
#include <wrl/client.h>

namespace rive::ore
{
class ContextD3D11;

class PipelineD3D11 : public LITE_RTTI_OVERRIDE(Pipeline, PipelineD3D11)
{
public:
    PipelineD3D11(const PipelineDesc& desc) : lite_rtti_override(desc) {}
    ~PipelineD3D11() override = default; // ComPtrs released automatically

private:
    friend class ContextD3D11;
    friend class RenderPassD3D11;
    // Raw shader pointers — valid as long as m_vsModule / m_psModule are alive.
    ID3D11VertexShader* m_d3dVS = nullptr;
    ID3D11PixelShader* m_d3dPS = nullptr;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_d3dInputLayout;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_d3dRasterizer;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_d3dDepthStencil;
    Microsoft::WRL::ComPtr<ID3D11BlendState> m_d3dBlend;
    // Keep modules alive so that m_d3dVS / m_d3dPS remain valid.
    rcp<ShaderModuleD3D11> m_vsModule;
    rcp<ShaderModuleD3D11> m_psModule;
};
} // namespace rive::ore
