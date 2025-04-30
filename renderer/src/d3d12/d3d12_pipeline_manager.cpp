/*
 * Copyright 2025 Rive
 */
#include "rive/renderer/d3d12/d3d12_pipeline_manager.hpp"
#include "rive/renderer/d3d/d3d_constants.hpp"

#include "generated/shaders/advanced_blend.glsl.hpp"
#include "generated/shaders/atomic_draw.glsl.hpp"
#include "generated/shaders/color_ramp.glsl.hpp"
#include "generated/shaders/constants.glsl.hpp"
#include "generated/shaders/common.glsl.hpp"
#include "generated/shaders/draw_image_mesh.glsl.hpp"
#include "generated/shaders/draw_path_common.glsl.hpp"
#include "generated/shaders/draw_path.glsl.hpp"
#include "generated/shaders/hlsl.glsl.hpp"
#include "generated/shaders/bezier_utils.glsl.hpp"
#include "generated/shaders/render_atlas.glsl.hpp"
#include "generated/shaders/tessellate.glsl.hpp"

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
    const D3DCapabilities& capabilities) :
    D3DPipelineManager<D3D12DrawVertexShader, ComPtr<ID3DBlob>, ID3D12Device>(
        device,
        capabilities,
        "vs_5_1",
        "ps_5_1")
{
    VERIFY_OK(
        this->device()->CreateRootSignature(0,
                                            sig::g_ROOT_SIG,
                                            std::size(sig::g_ROOT_SIG),
                                            IID_PPV_ARGS(&m_rootSignature)));
}

ID3D12PipelineState* D3D12PipelineManager::getDrawPipelineState(
    DrawType drawType,
    gpu::ShaderFeatures shaderFeatures,
    gpu::InterlockMode interlockMode,
    gpu::ShaderMiscFlags shaderMiscFlags)
{
    uint32_t pixelShaderKey = ShaderUniqueKey(drawType,
                                              shaderFeatures,
                                              interlockMode,
                                              shaderMiscFlags);

    auto pipelineEntry = m_drawPipelines.find(pixelShaderKey);
    if (pipelineEntry != m_drawPipelines.end())
    {
        return pipelineEntry->second.Get();
    }

    ShaderCompileResult result{};
    if (!getShader({drawType,
                    shaderFeatures,
                    interlockMode,
                    shaderMiscFlags,
                    d3dCapabilities()},
                   &result))
    {
        // this should never happen
        RIVE_UNREACHABLE();
    }

    return static_cast<ID3D12PipelineState*>(result.resultData);
}

void D3D12PipelineManager::compileBlobToFinalType(
    const ShaderCompileRequest& request,
    ComPtr<ID3DBlob> vertexShader,
    ComPtr<ID3DBlob> pixelShader,
    ShaderCompileResult* result)
{
    if (!result->vertexResult.hasResult)
    {
        switch (request.drawType)
        {
            case DrawType::midpointFanPatches:
            case DrawType::midpointFanCenterAAPatches:
            case DrawType::outerCurvePatches:
                result->vertexResult.vertexShaderResult.m_layoutDesc[0] = {
                    GLSL_a_patchVertexData,
                    0,
                    DXGI_FORMAT_R32G32B32A32_FLOAT,
                    PATCH_VERTEX_DATA_SLOT,
                    D3D12_APPEND_ALIGNED_ELEMENT,
                    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                    0};
                result->vertexResult.vertexShaderResult.m_layoutDesc[1] = {
                    GLSL_a_mirroredVertexData,
                    0,
                    DXGI_FORMAT_R32G32B32A32_FLOAT,
                    PATCH_VERTEX_DATA_SLOT,
                    D3D12_APPEND_ALIGNED_ELEMENT,
                    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                    0};
                result->vertexResult.vertexShaderResult.m_vertexAttribCount = 2;
                break;
            case DrawType::interiorTriangulation:
            case DrawType::atlasBlit:
                result->vertexResult.vertexShaderResult.m_layoutDesc[0] = {
                    GLSL_a_triangleVertex,
                    0,
                    DXGI_FORMAT_R32G32B32_FLOAT,
                    TRIANGLE_VERTEX_DATA_SLOT,
                    0,
                    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                    0};
                result->vertexResult.vertexShaderResult.m_vertexAttribCount = 1;
                break;
            case DrawType::imageRect:
                result->vertexResult.vertexShaderResult.m_layoutDesc[0] = {
                    GLSL_a_imageRectVertex,
                    0,
                    DXGI_FORMAT_R32G32B32A32_FLOAT,
                    IMAGE_RECT_VERTEX_DATA_SLOT,
                    0,
                    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                    0};
                result->vertexResult.vertexShaderResult.m_vertexAttribCount = 1;
                break;
            case DrawType::imageMesh:
                result->vertexResult.vertexShaderResult.m_layoutDesc[0] = {
                    GLSL_a_position,
                    0,
                    DXGI_FORMAT_R32G32_FLOAT,
                    IMAGE_MESH_VERTEX_DATA_SLOT,
                    D3D12_APPEND_ALIGNED_ELEMENT,
                    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                    0};
                result->vertexResult.vertexShaderResult.m_layoutDesc[1] = {
                    GLSL_a_texCoord,
                    0,
                    DXGI_FORMAT_R32G32_FLOAT,
                    IMAGE_MESH_UV_DATA_SLOT,
                    D3D12_APPEND_ALIGNED_ELEMENT,
                    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                    0};
                result->vertexResult.vertexShaderResult.m_vertexAttribCount = 2;
                break;
            case DrawType::atomicResolve:
                result->vertexResult.vertexShaderResult.m_vertexAttribCount = 0;
                break;
            case DrawType::atomicInitialize:
            case DrawType::msaaStrokes:
            case DrawType::msaaMidpointFanBorrowedCoverage:
            case DrawType::msaaMidpointFans:
            case DrawType::msaaMidpointFanStencilReset:
            case DrawType::msaaMidpointFanPathsStencil:
            case DrawType::msaaMidpointFanPathsCover:
            case DrawType::msaaOuterCubics:
            case DrawType::msaaStencilClipReset:
                RIVE_UNREACHABLE();
        }

        result->vertexResult.vertexShaderResult.m_shader = vertexShader;
        result->vertexResult.hasResult = true;
    }

    if (result->pixelResult.hasResult == false)
    {
        result->pixelResult.pixelShaderResult = pixelShader;
        result->pixelResult.hasResult = true;
    }

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

    switch (request.drawType)
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
        case DrawType::atomicResolve:
            rasterDesc.CullMode = D3D12_CULL_MODE_NONE;
            break;
        case DrawType::atomicInitialize:
        case DrawType::msaaStrokes:
        case DrawType::msaaMidpointFanBorrowedCoverage:
        case DrawType::msaaMidpointFans:
        case DrawType::msaaMidpointFanStencilReset:
        case DrawType::msaaMidpointFanPathsStencil:
        case DrawType::msaaMidpointFanPathsCover:
        case DrawType::msaaOuterCubics:
        case DrawType::msaaStencilClipReset:
            break;
    }

    D3D12_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].BlendEnable =
        request.shaderMiscFlags &
        (ShaderMiscFlags::fixedFunctionColorOutput |
         ShaderMiscFlags::coalescedResolveAndTransfer);
    blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask =
        request.shaderMiscFlags & (ShaderMiscFlags::fixedFunctionColorOutput |
                                   ShaderMiscFlags::coalescedResolveAndTransfer)
            ? D3D12_COLOR_WRITE_ENABLE_ALL
            : 0;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = {
        result->vertexResult.vertexShaderResult.m_layoutDesc,
        result->vertexResult.vertexShaderResult.m_vertexAttribCount};
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(
        result->vertexResult.vertexShaderResult.m_shader.Get());
    psoDesc.PS =
        CD3DX12_SHADER_BYTECODE(result->pixelResult.pixelShaderResult.Get());
    psoDesc.RasterizerState = rasterDesc;
    psoDesc.BlendState = blendDesc;
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;

    VERIFY_OK(
        device()->CreateGraphicsPipelineState(&psoDesc,
                                              IID_PPV_ARGS(&pipelineState)));

    result->resultData =
        m_drawPipelines.insert({result->pixelShaderKey, pipelineState})
            .first->second.Get();
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