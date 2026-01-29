/*
 * Copyright 2025 Rive
 */
#include "rive/renderer/d3d12/d3d12_pipeline_manager.hpp"
#include "rive/renderer/d3d/d3d_constants.hpp"

#include "generated/shaders/tessellate.glsl.exports.h"

// offline shaders
namespace shader
{
namespace tess::vert
{
#include "generated/shaders/d3d/tessellate.vert.h"
}
namespace tess::frag
{
#include "generated/shaders/d3d/tessellate.frag.h"
}
namespace grad::vert
{
#include "generated/shaders/d3d/color_ramp.vert.h"
}
namespace grad::frag
{
#include "generated/shaders/d3d/color_ramp.frag.h"
}
namespace atlas::vert
{
#include "generated/shaders/d3d/render_atlas.vert.h"
}
namespace atlas::fill
{
#include "generated/shaders/d3d/render_atlas_fill.frag.h"
}
namespace atlas::stroke
{
#include "generated/shaders/d3d/render_atlas_stroke.frag.h"
}
} // namespace shader
// offline sigs
namespace sig
{
#include "generated/shaders/d3d/root.sig.h"
} // namespace sig

namespace rive::gpu
{

D3D12PipelineManager::D3D12PipelineManager(
    ComPtr<ID3D12Device> device,
    const D3DCapabilities& capabilities,
    ShaderCompilationMode shaderCompilationMode) :
    Super(device, capabilities, shaderCompilationMode, "vs_5_1", "ps_5_1")
{
    VERIFY_OK(
        this->device()->CreateRootSignature(0,
                                            sig::g_ROOT_SIG,
                                            std::size(sig::g_ROOT_SIG),
                                            IID_PPV_ARGS(&m_rootSignature)));
}

std::unique_ptr<D3D12DrawVertexShader> D3D12PipelineManager::
    compileVertexShaderBlobToFinalType(DrawType drawType, ComPtr<ID3DBlob> blob)
{
    auto result = std::make_unique<D3D12DrawVertexShader>();
    switch (drawType)
    {
        case DrawType::midpointFanPatches:
        case DrawType::midpointFanCenterAAPatches:
        case DrawType::outerCurvePatches:
            result->m_layoutDesc[0] = {
                GLSL_a_patchVertexData,
                0,
                DXGI_FORMAT_R32G32B32A32_FLOAT,
                PATCH_VERTEX_DATA_SLOT,
                D3D12_APPEND_ALIGNED_ELEMENT,
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                0};
            result->m_layoutDesc[1] = {
                GLSL_a_mirroredVertexData,
                0,
                DXGI_FORMAT_R32G32B32A32_FLOAT,
                PATCH_VERTEX_DATA_SLOT,
                D3D12_APPEND_ALIGNED_ELEMENT,
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                0};
            result->m_vertexAttribCount = 2;
            break;
        case DrawType::interiorTriangulation:
        case DrawType::atlasBlit:
            result->m_layoutDesc[0] = {
                GLSL_a_triangleVertex,
                0,
                DXGI_FORMAT_R32G32B32_FLOAT,
                TRIANGLE_VERTEX_DATA_SLOT,
                0,
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                0};
            result->m_vertexAttribCount = 1;
            break;
        case DrawType::imageRect:
            result->m_layoutDesc[0] = {
                GLSL_a_imageRectVertex,
                0,
                DXGI_FORMAT_R32G32B32A32_FLOAT,
                IMAGE_RECT_VERTEX_DATA_SLOT,
                0,
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                0};
            result->m_vertexAttribCount = 1;
            break;
        case DrawType::imageMesh:
            result->m_layoutDesc[0] = {
                GLSL_a_position,
                0,
                DXGI_FORMAT_R32G32_FLOAT,
                IMAGE_MESH_VERTEX_DATA_SLOT,
                D3D12_APPEND_ALIGNED_ELEMENT,
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                0};
            result->m_layoutDesc[1] = {
                GLSL_a_texCoord,
                0,
                DXGI_FORMAT_R32G32_FLOAT,
                IMAGE_MESH_UV_DATA_SLOT,
                D3D12_APPEND_ALIGNED_ELEMENT,
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                0};
            result->m_vertexAttribCount = 2;
            break;
        case DrawType::renderPassResolve:
            result->m_vertexAttribCount = 0;
            break;
        case DrawType::msaaStrokes:
        case DrawType::msaaMidpointFanBorrowedCoverage:
        case DrawType::msaaMidpointFans:
        case DrawType::msaaMidpointFanStencilReset:
        case DrawType::msaaMidpointFanPathsStencil:
        case DrawType::msaaMidpointFanPathsCover:
        case DrawType::msaaOuterCubics:
        case DrawType::msaaStencilClipReset:
        case DrawType::renderPassInitialize:
            RIVE_UNREACHABLE();
    }

    result->m_shader = blob;
    return result;
}

std::unique_ptr<D3D12DrawPixelShader> D3D12PipelineManager::
    compilePixelShaderBlobToFinalType(ComPtr<ID3DBlob> blob)
{
    auto result = std::make_unique<D3D12DrawPixelShader>();
    result->m_shader = std::move(blob);
    return result;
}

std::unique_ptr<D3D12Pipeline> D3D12PipelineManager::linkPipeline(
    const PipelineProps& props,
    const D3D12DrawVertexShader& vs,
    const D3D12DrawPixelShader& ps)
{
    ComPtr<ID3D12PipelineState> pipelineState;

    D3D12_RASTERIZER_DESC rasterDesc = {};
    rasterDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterDesc.FrontCounterClockwise = FALSE;
    rasterDesc.DepthBias = 0;
    rasterDesc.SlopeScaledDepthBias = 0;
    rasterDesc.DepthBiasClamp = 0;
    rasterDesc.DepthClipEnable = TRUE;
    rasterDesc.MultisampleEnable = FALSE;
    rasterDesc.AntialiasedLineEnable = FALSE;

    switch (props.drawType)
    {
        case DrawType::midpointFanPatches:
        case DrawType::midpointFanCenterAAPatches:
        case DrawType::outerCurvePatches:
        case DrawType::interiorTriangulation:
        case DrawType::atlasBlit:
            rasterDesc.CullMode = D3D12_CULL_MODE_BACK;
            break;
        case DrawType::imageRect:
        case DrawType::imageMesh:
        case DrawType::renderPassResolve:
            rasterDesc.CullMode = D3D12_CULL_MODE_NONE;
            break;
        case DrawType::msaaStrokes:
        case DrawType::msaaMidpointFanBorrowedCoverage:
        case DrawType::msaaMidpointFans:
        case DrawType::msaaMidpointFanStencilReset:
        case DrawType::msaaMidpointFanPathsStencil:
        case DrawType::msaaMidpointFanPathsCover:
        case DrawType::msaaOuterCubics:
        case DrawType::msaaStencilClipReset:
        case DrawType::renderPassInitialize:
            break;
    }

    D3D12_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].BlendEnable =
        props.shaderMiscFlags & ShaderMiscFlags::fixedFunctionColorOutput;
    blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask =
        props.shaderMiscFlags & (ShaderMiscFlags::fixedFunctionColorOutput |
                                 ShaderMiscFlags::coalescedResolveAndTransfer)
            ? D3D12_COLOR_WRITE_ENABLE_ALL
            : 0;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = {vs.m_layoutDesc, vs.m_vertexAttribCount};
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(vs.m_shader.Get());
    psoDesc.PS = CD3DX12_SHADER_BYTECODE(ps.m_shader.Get());
    psoDesc.RasterizerState = rasterDesc;
    psoDesc.BlendState = blendDesc;
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;

    auto result = std::make_unique<D3D12Pipeline>();
#ifdef WITH_RIVE_TOOLS
    if (props.synthesizedFailureType ==
            SynthesizedFailureType::pipelineCreation ||
        props.synthesizedFailureType ==
            SynthesizedFailureType::shaderCompilation)
    {
        // An empty result is what counts as "failed"
        return result;
    }
#endif

    VERIFY_OK(device()->CreateGraphicsPipelineState(
        &psoDesc,
        IID_PPV_ARGS(&result->m_d3dPipelineState)));
    return result;
}

void D3D12PipelineManager::compileTesselationPipeline()
{

    D3D12_INPUT_ELEMENT_DESC layoutDesc[] = {
        {GLSL_a_p0p1_,
         0,
         DXGI_FORMAT_R32G32B32A32_FLOAT,
         0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,
         1},
        {GLSL_a_p2p3_,
         0,
         DXGI_FORMAT_R32G32B32A32_FLOAT,
         0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,
         1},
        {GLSL_a_joinTan_and_ys,
         0,
         DXGI_FORMAT_R32G32B32A32_FLOAT,
         0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,
         1},
        {GLSL_a_args,
         0,
         DXGI_FORMAT_R32G32B32A32_UINT,
         0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,
         1}};

    D3D12_RASTERIZER_DESC rasterDesc = {};
    rasterDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterDesc.CullMode = D3D12_CULL_MODE_BACK;
    rasterDesc.FrontCounterClockwise = FALSE;
    rasterDesc.DepthBias = 0;
    rasterDesc.SlopeScaledDepthBias = 0;
    rasterDesc.DepthBiasClamp = 0;
    rasterDesc.DepthClipEnable = FALSE;
    rasterDesc.MultisampleEnable = FALSE;
    rasterDesc.AntialiasedLineEnable = FALSE;

    D3D12_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].BlendEnable = FALSE;
    blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask =
        D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = {layoutDesc, _countof(layoutDesc)};
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.VS = {shader::tess::vert::g_main,
                  std::size(shader::tess::vert::g_main)};
    psoDesc.PS = {shader::tess::frag::g_main,
                  std::size(shader::tess::frag::g_main)};
    psoDesc.RasterizerState = rasterDesc;
    psoDesc.BlendState = blendDesc;
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_UINT;
    psoDesc.SampleDesc.Count = 1;

    VERIFY_OK(device()->CreateGraphicsPipelineState(
        &psoDesc,
        IID_PPV_ARGS(&m_tesselationPipeline)));
}

void D3D12PipelineManager::compileGradientPipeline()
{

    D3D12_INPUT_ELEMENT_DESC layoutDesc = {
        GLSL_a_span,
        0,
        DXGI_FORMAT_R32G32B32A32_UINT,
        0,
        0,
        D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,
        1};

    D3D12_RASTERIZER_DESC rasterDesc = {};
    rasterDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterDesc.CullMode = D3D12_CULL_MODE_BACK;
    rasterDesc.FrontCounterClockwise = FALSE;
    rasterDesc.DepthBias = 0;
    rasterDesc.SlopeScaledDepthBias = 0;
    rasterDesc.DepthBiasClamp = 0;
    rasterDesc.DepthClipEnable = FALSE;
    rasterDesc.MultisampleEnable = FALSE;
    rasterDesc.AntialiasedLineEnable = FALSE;

    D3D12_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].BlendEnable = FALSE;
    blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask =
        D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = {&layoutDesc, 1};
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.VS = {shader::grad::vert::g_main,
                  std::size(shader::grad::vert::g_main)};
    psoDesc.PS = {shader::grad::frag::g_main,
                  std::size(shader::grad::frag::g_main)};
    psoDesc.RasterizerState = rasterDesc;
    psoDesc.BlendState = blendDesc;
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;

    VERIFY_OK(device()->CreateGraphicsPipelineState(
        &psoDesc,
        IID_PPV_ARGS(&m_gradientPipeline)));
}

void D3D12PipelineManager::compileAtlasPipeline()
{

    D3D12_INPUT_ELEMENT_DESC layoutDesc[2];
    layoutDesc[0] = {GLSL_a_patchVertexData,
                     0,
                     DXGI_FORMAT_R32G32B32A32_FLOAT,
                     PATCH_VERTEX_DATA_SLOT,
                     D3D12_APPEND_ALIGNED_ELEMENT,
                     D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                     0};
    layoutDesc[1] = {GLSL_a_mirroredVertexData,
                     0,
                     DXGI_FORMAT_R32G32B32A32_FLOAT,
                     PATCH_VERTEX_DATA_SLOT,
                     D3D12_APPEND_ALIGNED_ELEMENT,
                     D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                     0};

    D3D12_RASTERIZER_DESC rasterDesc = {};
    rasterDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterDesc.CullMode = D3D12_CULL_MODE_BACK;
    rasterDesc.FrontCounterClockwise = FALSE;
    rasterDesc.DepthBias = 0;
    rasterDesc.SlopeScaledDepthBias = 0;
    rasterDesc.DepthBiasClamp = 0;
    rasterDesc.DepthClipEnable = FALSE;
    rasterDesc.MultisampleEnable = FALSE;
    rasterDesc.AntialiasedLineEnable = FALSE;

    D3D12_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_MAX;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_MAX;
    blendDesc.RenderTarget[0].RenderTargetWriteMask =
        D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = {layoutDesc, 2};
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.VS = {shader::atlas::vert::g_main,
                  std::size(shader::atlas::vert::g_main)};
    psoDesc.PS = {shader::atlas::stroke::g_main,
                  std::size(shader::atlas::stroke::g_main)};
    psoDesc.RasterizerState = rasterDesc;
    psoDesc.BlendState = blendDesc;
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R32_FLOAT;
    psoDesc.SampleDesc.Count = 1;

    VERIFY_OK(device()->CreateGraphicsPipelineState(
        &psoDesc,
        IID_PPV_ARGS(&m_atlasStrokePipeline)));

    psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

    psoDesc.PS = {shader::atlas::fill::g_main,
                  std::size(shader::atlas::fill::g_main)};

    VERIFY_OK(device()->CreateGraphicsPipelineState(
        &psoDesc,
        IID_PPV_ARGS(&m_atlasFillPipeline)));
}
} // namespace rive::gpu
