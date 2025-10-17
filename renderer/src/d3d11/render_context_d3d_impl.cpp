/*
 * Copyright 2023 Rive
 */

#include "rive/renderer/d3d11/render_context_d3d_impl.hpp"

#include "rive/renderer/d3d/d3d_constants.hpp"

#include "rive/renderer/texture.hpp"

#include <D3DCompiler.h>

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

namespace rive::gpu
{
ComPtr<ID3D11Texture2D> make_simple_2d_texture(ID3D11Device* gpu,
                                               DXGI_FORMAT format,
                                               UINT width,
                                               UINT height,
                                               UINT mipLevelCount,
                                               UINT bindFlags,
                                               UINT miscFlags = 0)
{
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = mipLevelCount;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = bindFlags;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = miscFlags;

    ComPtr<ID3D11Texture2D> tex;
    VERIFY_OK(gpu->CreateTexture2D(&desc, NULL, tex.ReleaseAndGetAddressOf()));
    return tex;
}

static ComPtr<ID3D11UnorderedAccessView> make_simple_2d_uav(
    ID3D11Device* gpu,
    ID3D11Texture2D* tex,
    DXGI_FORMAT format)
{
    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.Format = format;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;

    ComPtr<ID3D11UnorderedAccessView> uav;
    VERIFY_OK(gpu->CreateUnorderedAccessView(tex,
                                             &uavDesc,
                                             uav.ReleaseAndGetAddressOf()));
    return uav;
}

D3D11PipelineManager::D3D11PipelineManager(
    ComPtr<ID3D11DeviceContext> context,
    ComPtr<ID3D11Device> device,
    const D3DCapabilities& capabilities,
    ShaderCompilationMode shaderCompilationMode) :
    Super(device, capabilities, shaderCompilationMode, "vs_5_0", "ps_5_0"),
    m_context(context)
{
    D3D11_INPUT_ELEMENT_DESC spanDesc = {GLSL_a_span,
                                         0,
                                         DXGI_FORMAT_R32G32B32A32_UINT,
                                         0,
                                         0,
                                         D3D11_INPUT_PER_INSTANCE_DATA,
                                         1};

    VERIFY_OK(
        this->device()->CreateInputLayout(&spanDesc,
                                          1,
                                          shader::grad::vert::g_main,
                                          std::size(shader::grad::vert::g_main),
                                          &m_colorRampLayout));
    VERIFY_OK(this->device()->CreateVertexShader(
        shader::grad::vert::g_main,
        std::size(shader::grad::vert::g_main),
        nullptr,
        &m_colorRampVertexShader));
    VERIFY_OK(
        this->device()->CreatePixelShader(shader::grad::frag::g_main,
                                          std::size(shader::grad::frag::g_main),
                                          nullptr,
                                          &m_colorRampPixelShader));

    D3D11_INPUT_ELEMENT_DESC attribsDesc[] = {{GLSL_a_p0p1_,
                                               0,
                                               DXGI_FORMAT_R32G32B32A32_FLOAT,
                                               0,
                                               D3D11_APPEND_ALIGNED_ELEMENT,
                                               D3D11_INPUT_PER_INSTANCE_DATA,
                                               1},
                                              {GLSL_a_p2p3_,
                                               0,
                                               DXGI_FORMAT_R32G32B32A32_FLOAT,
                                               0,
                                               D3D11_APPEND_ALIGNED_ELEMENT,
                                               D3D11_INPUT_PER_INSTANCE_DATA,
                                               1},
                                              {GLSL_a_joinTan_and_ys,
                                               0,
                                               DXGI_FORMAT_R32G32B32A32_FLOAT,
                                               0,
                                               D3D11_APPEND_ALIGNED_ELEMENT,
                                               D3D11_INPUT_PER_INSTANCE_DATA,
                                               1},
                                              {GLSL_a_args,
                                               0,
                                               DXGI_FORMAT_R32G32B32A32_UINT,
                                               0,
                                               D3D11_APPEND_ALIGNED_ELEMENT,
                                               D3D11_INPUT_PER_INSTANCE_DATA,
                                               1}};
    VERIFY_OK(
        this->device()->CreateInputLayout(attribsDesc,
                                          std::size(attribsDesc),
                                          shader::tess::vert::g_main,
                                          std::size(shader::tess::vert::g_main),
                                          &m_tessellateLayout));
    VERIFY_OK(this->device()->CreateVertexShader(
        shader::tess::vert::g_main,
        std::size(shader::tess::vert::g_main),
        nullptr,
        &m_tessellateVertexShader));
    VERIFY_OK(
        this->device()->CreatePixelShader(shader::tess::frag::g_main,
                                          std::size(shader::tess::frag::g_main),
                                          nullptr,
                                          &m_tessellatePixelShader));

    D3D11_INPUT_ELEMENT_DESC layoutDesc[2];
    layoutDesc[0] = {GLSL_a_patchVertexData,
                     0,
                     DXGI_FORMAT_R32G32B32A32_FLOAT,
                     PATCH_VERTEX_DATA_SLOT,
                     D3D11_APPEND_ALIGNED_ELEMENT,
                     D3D11_INPUT_PER_VERTEX_DATA,
                     0};
    layoutDesc[1] = {GLSL_a_mirroredVertexData,
                     0,
                     DXGI_FORMAT_R32G32B32A32_FLOAT,
                     PATCH_VERTEX_DATA_SLOT,
                     D3D11_APPEND_ALIGNED_ELEMENT,
                     D3D11_INPUT_PER_VERTEX_DATA,
                     0};
    VERIFY_OK(this->device()->CreateInputLayout(
        layoutDesc,
        2,
        shader::atlas::vert::g_main,
        std::size(shader::atlas::vert::g_main),
        &m_atlasLayout));
    VERIFY_OK(this->device()->CreateVertexShader(
        shader::atlas::vert::g_main,
        std::size(shader::atlas::vert::g_main),
        nullptr,
        &m_atlasVertexShader));

    VERIFY_OK(this->device()->CreatePixelShader(
        shader::atlas::fill::g_main,
        std::size(shader::atlas::fill::g_main),
        nullptr,
        &m_atlasFillPixelShader));

    VERIFY_OK(this->device()->CreatePixelShader(
        shader::atlas::stroke::g_main,
        std::size(shader::atlas::stroke::g_main),
        nullptr,
        &m_atlasStrokePixelShader));
}

bool D3D11PipelineManager::setPipelineState(
    DrawType drawType,
    ShaderFeatures features,
    InterlockMode interlockMode,
    ShaderMiscFlags miscFlags,
    const PlatformFeatures& platformFeatures
#ifdef WITH_RIVE_TOOLS
    ,
    SynthesizedFailureType synthesizedFailureType
#endif
)
{
    auto* pipeline = tryGetPipeline(
        {
            .drawType = drawType,
            .shaderFeatures = features,
            .interlockMode = interlockMode,
            .shaderMiscFlags = miscFlags,
#ifdef WITH_RIVE_TOOLS
            .synthesizedFailureType = synthesizedFailureType,
#endif
        },
        platformFeatures);

    if (pipeline == nullptr)
    {
        return false;
    }

    m_context->IASetInputLayout(pipeline->m_vertexShader.layout.Get());
    m_context->VSSetShader(pipeline->m_vertexShader.shader.Get(), nullptr, 0);
    m_context->PSSetShader(pipeline->m_pixelShader.shader.Get(), nullptr, 0);
    return true;
}

std::unique_ptr<D3D11DrawVertexShader> D3D11PipelineManager::
    compileVertexShaderBlobToFinalType(DrawType drawType, ComPtr<ID3DBlob> blob)
{
    D3D11_INPUT_ELEMENT_DESC layoutDesc[2];
    uint32_t vertexAttribCount;
    switch (drawType)
    {
        case DrawType::midpointFanPatches:
        case DrawType::midpointFanCenterAAPatches:
        case DrawType::outerCurvePatches:
            layoutDesc[0] = {GLSL_a_patchVertexData,
                             0,
                             DXGI_FORMAT_R32G32B32A32_FLOAT,
                             PATCH_VERTEX_DATA_SLOT,
                             D3D11_APPEND_ALIGNED_ELEMENT,
                             D3D11_INPUT_PER_VERTEX_DATA,
                             0};
            layoutDesc[1] = {GLSL_a_mirroredVertexData,
                             0,
                             DXGI_FORMAT_R32G32B32A32_FLOAT,
                             PATCH_VERTEX_DATA_SLOT,
                             D3D11_APPEND_ALIGNED_ELEMENT,
                             D3D11_INPUT_PER_VERTEX_DATA,
                             0};
            vertexAttribCount = 2;
            break;
        case DrawType::interiorTriangulation:
        case DrawType::atlasBlit:
            layoutDesc[0] = {GLSL_a_triangleVertex,
                             0,
                             DXGI_FORMAT_R32G32B32_FLOAT,
                             TRIANGLE_VERTEX_DATA_SLOT,
                             0,
                             D3D11_INPUT_PER_VERTEX_DATA,
                             0};
            vertexAttribCount = 1;
            break;
        case DrawType::imageRect:
            layoutDesc[0] = {GLSL_a_imageRectVertex,
                             0,
                             DXGI_FORMAT_R32G32B32A32_FLOAT,
                             IMAGE_RECT_VERTEX_DATA_SLOT,
                             0,
                             D3D11_INPUT_PER_VERTEX_DATA,
                             0};
            vertexAttribCount = 1;
            break;
        case DrawType::imageMesh:
            layoutDesc[0] = {GLSL_a_position,
                             0,
                             DXGI_FORMAT_R32G32_FLOAT,
                             IMAGE_MESH_VERTEX_DATA_SLOT,
                             D3D11_APPEND_ALIGNED_ELEMENT,
                             D3D11_INPUT_PER_VERTEX_DATA,
                             0};
            layoutDesc[1] = {GLSL_a_texCoord,
                             0,
                             DXGI_FORMAT_R32G32_FLOAT,
                             IMAGE_MESH_UV_DATA_SLOT,
                             D3D11_APPEND_ALIGNED_ELEMENT,
                             D3D11_INPUT_PER_VERTEX_DATA,
                             0};
            vertexAttribCount = 2;
            break;
        case DrawType::renderPassResolve:
            vertexAttribCount = 0;
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

    auto result = std::make_unique<D3D11DrawVertexShader>();

    VERIFY_OK(device()->CreateInputLayout(layoutDesc,
                                          vertexAttribCount,
                                          blob->GetBufferPointer(),
                                          blob->GetBufferSize(),
                                          &result->layout));
    VERIFY_OK(device()->CreateVertexShader(blob->GetBufferPointer(),
                                           blob->GetBufferSize(),
                                           nullptr,
                                           &result->shader));
    return result;
}

std::unique_ptr<D3D11DrawPixelShader> D3D11PipelineManager ::
    compilePixelShaderBlobToFinalType(ComPtr<ID3DBlob> blob)
{
    auto result = std::make_unique<D3D11DrawPixelShader>();

    VERIFY_OK(device()->CreatePixelShader(blob->GetBufferPointer(),
                                          blob->GetBufferSize(),
                                          nullptr,
                                          &result->shader));

    return result;
}

std::unique_ptr<D3D11DrawPipeline> D3D11PipelineManager::linkPipeline(
    const PipelineProps& props,
    const D3D11DrawVertexShader& vs,
    const D3D11DrawPixelShader& ps)
{
    // For D3D11 this just puts the vs and ps into a single structure together.
    auto pipeline = std::make_unique<D3D11DrawPipeline>();
#ifdef WITH_RIVE_TOOLS
    if (props.synthesizedFailureType ==
            SynthesizedFailureType::pipelineCreation ||
        props.synthesizedFailureType ==
            SynthesizedFailureType::shaderCompilation)
    {
        // An empty result is what counts as "failed"
        return pipeline;
    }
#else
    std::ignore = props;
#endif

    pipeline->m_vertexShader = vs;
    pipeline->m_pixelShader = ps;
    return pipeline;
}

static D3D11_FILTER filter_for_sampler_filter_options(ImageFilter option)
{
    switch (option)
    {
        case ImageFilter::nearest:
            return D3D11_FILTER::D3D11_FILTER_MIN_MAG_MIP_POINT;
        case ImageFilter::bilinear:
            return D3D11_FILTER::D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    }

    RIVE_UNREACHABLE();
    return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
}

static D3D11_TEXTURE_ADDRESS_MODE address_mode_for_sampler_filter_options(
    ImageWrap option)
{
    switch (option)
    {
        case ImageWrap::clamp:
            return D3D11_TEXTURE_ADDRESS_CLAMP;
        case ImageWrap::repeat:
            return D3D11_TEXTURE_ADDRESS_WRAP;
        case ImageWrap::mirror:
            return D3D11_TEXTURE_ADDRESS_MIRROR;
    }

    RIVE_UNREACHABLE();
    return D3D11_TEXTURE_ADDRESS_CLAMP;
}

std::unique_ptr<RenderContext> RenderContextD3DImpl::MakeContext(
    ComPtr<ID3D11Device> gpu,
    ComPtr<ID3D11DeviceContext> gpuContext,
    const D3DContextOptions& contextOptions)
{
    D3DCapabilities d3dCapabilities;
    D3D11_FEATURE_DATA_D3D11_OPTIONS2 d3d11Options2;

    if (gpu->GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_1)
    {
        if (SUCCEEDED(gpu->CheckFeatureSupport(
                D3D11_FEATURE_D3D11_OPTIONS2,
                &d3d11Options2,
                sizeof(D3D11_FEATURE_DATA_D3D11_OPTIONS2))))
        {
            d3dCapabilities.supportsRasterizerOrderedViews =
                d3d11Options2.ROVsSupported;
            if (d3d11Options2.TypedUAVLoadAdditionalFormats)
            {
                // TypedUAVLoadAdditionalFormats is true. Now check if we can
                // both load and store all formats used by Rive (currently only
                // RGBA8 and BGRA8):
                // https://learn.microsoft.com/en-us/windows/win32/direct3d11/typed-unordered-access-view-loads.
                auto check_typed_uav_load = [gpu](DXGI_FORMAT format) {
                    D3D11_FEATURE_DATA_FORMAT_SUPPORT2 d3d11Format2{};
                    d3d11Format2.InFormat = format;
                    if (SUCCEEDED(gpu->CheckFeatureSupport(
                            D3D11_FEATURE_FORMAT_SUPPORT2,
                            &d3d11Format2,
                            sizeof(d3d11Format2))))
                    {
                        constexpr UINT loadStoreFlags =
                            D3D11_FORMAT_SUPPORT2_UAV_TYPED_LOAD |
                            D3D11_FORMAT_SUPPORT2_UAV_TYPED_STORE;
                        return (d3d11Format2.OutFormatSupport2 &
                                loadStoreFlags) == loadStoreFlags;
                    }
                    return false;
                };
                d3dCapabilities.supportsTypedUAVLoadStore =
                    check_typed_uav_load(DXGI_FORMAT_R8G8B8A8_UNORM) &&
                    check_typed_uav_load(DXGI_FORMAT_B8G8R8A8_UNORM);
            }
        }

        // Check if we can use HLSL minimum precision types (e.g. min16int)
        D3D11_FEATURE_DATA_SHADER_MIN_PRECISION_SUPPORT
        d3d11MinPrecisionSupport;
        if (SUCCEEDED(gpu->CheckFeatureSupport(
                D3D11_FEATURE_SHADER_MIN_PRECISION_SUPPORT,
                &d3d11MinPrecisionSupport,
                sizeof(d3d11MinPrecisionSupport))))
        {
            const UINT allStageMinPrecision =
                (d3d11MinPrecisionSupport.AllOtherShaderStagesMinPrecision &
                 d3d11MinPrecisionSupport.PixelShaderMinPrecision);

            d3dCapabilities.supportsMin16Precision =
                (allStageMinPrecision & D3D11_SHADER_MIN_PRECISION_16_BIT) != 0;
        }
    }

    if (contextOptions.disableRasterizerOrderedViews)
    {
        d3dCapabilities.supportsRasterizerOrderedViews = false;
    }
    if (contextOptions.disableTypedUAVLoadStore)
    {
        d3dCapabilities.supportsTypedUAVLoadStore = false;
    }

    d3dCapabilities.isIntel = contextOptions.isIntel;
    d3dCapabilities.allowsUAVSlot0WithColorOutput = false;

    auto renderContextImpl = std::unique_ptr<RenderContextD3DImpl>(
        new RenderContextD3DImpl(std::move(gpu),
                                 std::move(gpuContext),
                                 d3dCapabilities,
                                 contextOptions));
    return std::make_unique<RenderContext>(std::move(renderContextImpl));
}

RenderContextD3DImpl::RenderContextD3DImpl(
    ComPtr<ID3D11Device> gpu,
    ComPtr<ID3D11DeviceContext> gpuContext,
    const D3DCapabilities& d3dCapabilities,
    const D3DContextOptions& d3dContextOptions) :
    m_pipelineManager(gpuContext,
                      gpu,
                      d3dCapabilities,
                      d3dContextOptions.shaderCompilationMode),
    m_d3dCapabilities(d3dCapabilities),
    m_gpu(std::move(gpu)),
    m_gpuContext(std::move(gpuContext))
{
    m_platformFeatures.clipSpaceBottomUp = true;
    m_platformFeatures.framebufferBottomUp = false;
    m_platformFeatures.supportsRasterOrdering =
        d3dCapabilities.supportsRasterizerOrderedViews;
    m_platformFeatures.supportsFragmentShaderAtomics = true;
    m_platformFeatures.maxTextureSize = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;

    // Create a default raster state for path and offscreen draws.
    D3D11_RASTERIZER_DESC rasterDesc;
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.CullMode = D3D11_CULL_BACK;
    rasterDesc.FrontCounterClockwise =
        FALSE; // FrontCounterClockwise must be FALSE in order to
               // match the winding sense of interior triangulations.

    rasterDesc.DepthBias = 0;
    rasterDesc.SlopeScaledDepthBias = 0;
    rasterDesc.DepthBiasClamp = 0;
    rasterDesc.DepthClipEnable =
        TRUE; // This is the default state which reset to before flushing.
              // Details on default state here:
    // https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_rasterizer_desc

    rasterDesc.ScissorEnable = FALSE;
    rasterDesc.MultisampleEnable = FALSE;
    rasterDesc.AntialiasedLineEnable = FALSE;
    VERIFY_OK(m_gpu->CreateRasterizerState(
        &rasterDesc,
        m_backCulledRasterState[0].ReleaseAndGetAddressOf()));

    // ...And with scissor for the atlas.
    rasterDesc.ScissorEnable = TRUE;
    VERIFY_OK(m_gpu->CreateRasterizerState(
        &rasterDesc,
        m_atlasRasterState.ReleaseAndGetAddressOf()));

    // ...And with wireframe for debugging.
    rasterDesc.ScissorEnable = FALSE;
    rasterDesc.FillMode = D3D11_FILL_WIREFRAME;
    VERIFY_OK(m_gpu->CreateRasterizerState(
        &rasterDesc,
        m_backCulledRasterState[1].ReleaseAndGetAddressOf()));

    // Create a raster state without face culling for drawing image meshes.
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.CullMode = D3D11_CULL_NONE;
    VERIFY_OK(m_gpu->CreateRasterizerState(
        &rasterDesc,
        m_doubleSidedRasterState[0].ReleaseAndGetAddressOf()));

    // ...And once more with wireframe for debugging.
    rasterDesc.FillMode = D3D11_FILL_WIREFRAME;
    VERIFY_OK(m_gpu->CreateRasterizerState(
        &rasterDesc,
        m_doubleSidedRasterState[1].ReleaseAndGetAddressOf()));

    // Create the feather texture.
    D3D11_TEXTURE1D_DESC featherTextureDesc{};
    featherTextureDesc.Format = DXGI_FORMAT_R16_FLOAT;
    featherTextureDesc.Width = gpu::GAUSSIAN_TABLE_SIZE;
    featherTextureDesc.MipLevels = 1;
    featherTextureDesc.ArraySize = FEATHER_TEXTURE_1D_ARRAY_LENGTH;
    featherTextureDesc.Usage = D3D11_USAGE_DEFAULT;
    featherTextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    featherTextureDesc.CPUAccessFlags = 0;
    featherTextureDesc.MiscFlags = 0;
    VERIFY_OK(
        m_gpu->CreateTexture1D(&featherTextureDesc,
                               NULL,
                               m_featherTexture.ReleaseAndGetAddressOf()));

    D3D11_BOX box;
    box.left = 0;
    box.right = gpu::GAUSSIAN_TABLE_SIZE;
    box.top = 0;
    box.bottom = 1;
    box.front = 0;
    box.back = 1;
    m_gpuContext->UpdateSubresource(m_featherTexture.Get(),
                                    FEATHER_FUNCTION_ARRAY_INDEX,
                                    &box,
                                    gpu::g_gaussianIntegralTableF16,
                                    sizeof(gpu::g_gaussianIntegralTableF16),
                                    sizeof(gpu::g_gaussianIntegralTableF16));
    m_gpuContext->UpdateSubresource(m_featherTexture.Get(),
                                    FEATHER_INVERSE_FUNCTION_ARRAY_INDEX,
                                    &box,
                                    gpu::g_inverseGaussianIntegralTableF16,
                                    sizeof(gpu::g_gaussianIntegralTableF16),
                                    0);
    VERIFY_OK(m_gpu->CreateShaderResourceView(
        m_featherTexture.Get(),
        NULL,
        m_featherTextureSRV.ReleaseAndGetAddressOf()));

    // Compile the tessellation shaders.
    {
        m_tessSpanIndexBuffer =
            makeSimpleImmutableBuffer(sizeof(gpu::kTessSpanIndices),
                                      D3D11_BIND_INDEX_BUFFER,
                                      gpu::kTessSpanIndices);
    }

    // Set up the path patch rendering buffers.
    PatchVertex patchVertices[kPatchVertexBufferCount];
    uint16_t patchIndices[kPatchIndexBufferCount];
    GeneratePatchBufferData(patchVertices, patchIndices);
    m_patchVertexBuffer = makeSimpleImmutableBuffer(sizeof(patchVertices),
                                                    D3D11_BIND_VERTEX_BUFFER,
                                                    patchVertices);
    m_patchIndexBuffer = makeSimpleImmutableBuffer(sizeof(patchIndices),
                                                   D3D11_BIND_INDEX_BUFFER,
                                                   patchIndices);

    // Set up the imageRect rendering buffers. (gpu::InterlockMode::atomics
    // only.)
    m_imageRectVertexBuffer =
        makeSimpleImmutableBuffer(sizeof(gpu::kImageRectVertices),
                                  D3D11_BIND_VERTEX_BUFFER,
                                  gpu::kImageRectVertices);
    m_imageRectIndexBuffer =
        makeSimpleImmutableBuffer(sizeof(gpu::kImageRectIndices),
                                  D3D11_BIND_INDEX_BUFFER,
                                  gpu::kImageRectIndices);

    // Create buffers for uniforms.
    {
        D3D11_BUFFER_DESC desc{};
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

        desc.ByteWidth = sizeof(gpu::FlushUniforms);
        desc.StructureByteStride = sizeof(gpu::FlushUniforms);
        VERIFY_OK(
            m_gpu->CreateBuffer(&desc,
                                nullptr,
                                m_flushUniforms.ReleaseAndGetAddressOf()));

        desc.ByteWidth = sizeof(DrawUniforms);
        desc.StructureByteStride = sizeof(DrawUniforms);
        VERIFY_OK(m_gpu->CreateBuffer(&desc,
                                      nullptr,
                                      m_drawUniforms.ReleaseAndGetAddressOf()));

        desc.ByteWidth = sizeof(gpu::ImageDrawUniforms);
        desc.StructureByteStride = sizeof(gpu::ImageDrawUniforms);
        VERIFY_OK(
            m_gpu->CreateBuffer(&desc,
                                nullptr,
                                m_imageDrawUniforms.ReleaseAndGetAddressOf()));
    }

    // Create a linear sampler for the gradient & feather textures.
    D3D11_SAMPLER_DESC linearSamplerDesc;
    linearSamplerDesc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    linearSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    linearSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    linearSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    linearSamplerDesc.MipLODBias = 0.0f;
    linearSamplerDesc.MaxAnisotropy = 1;
    linearSamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    linearSamplerDesc.MinLOD = 0;
    linearSamplerDesc.MaxLOD = 0;
    VERIFY_OK(
        m_gpu->CreateSamplerState(&linearSamplerDesc,
                                  m_linearSampler.ReleaseAndGetAddressOf()));

    // Create a mipmap sampler for each sampler permutation option.
    for (int samplerKey = 0;
         samplerKey < ImageSampler::MAX_SAMPLER_PERMUTATIONS;
         ++samplerKey)
    {
        auto xWrap = ImageSampler::GetWrapXOptionFromKey(samplerKey);
        auto yWrap = ImageSampler::GetWrapYOptionFromKey(samplerKey);
        D3D11_SAMPLER_DESC mipmapSamplerDesc;
        mipmapSamplerDesc.Filter = filter_for_sampler_filter_options(
            ImageSampler::GetFilterOptionFromKey(samplerKey));
        mipmapSamplerDesc.AddressU =
            address_mode_for_sampler_filter_options(xWrap);
        mipmapSamplerDesc.AddressV =
            address_mode_for_sampler_filter_options(yWrap);
        mipmapSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        mipmapSamplerDesc.MaxAnisotropy = 1;
        mipmapSamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        mipmapSamplerDesc.MipLODBias = 0.0f;
        mipmapSamplerDesc.MinLOD = 0;
        mipmapSamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
        VERIFY_OK(m_gpu->CreateSamplerState(
            &mipmapSamplerDesc,
            m_samplerStates[samplerKey].ReleaseAndGetAddressOf()));
    }

    m_gpuContext->VSSetSamplers(FEATHER_TEXTURE_IDX,
                                1,
                                m_linearSampler.GetAddressOf());

    D3D11_BLEND_DESC srcOverDesc{};
    srcOverDesc.RenderTarget[0].BlendEnable = TRUE;
    srcOverDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    srcOverDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    srcOverDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    srcOverDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    srcOverDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    srcOverDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    srcOverDesc.RenderTarget[0].RenderTargetWriteMask =
        D3D11_COLOR_WRITE_ENABLE_ALL;
    VERIFY_OK(
        m_gpu->CreateBlendState(&srcOverDesc,
                                m_srcOverBlendState.ReleaseAndGetAddressOf()));

    D3D11_BLEND_DESC plusDesc{};
    plusDesc.RenderTarget[0].BlendEnable = TRUE;
    plusDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    plusDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    plusDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    plusDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    plusDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    plusDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    plusDesc.RenderTarget[0].RenderTargetWriteMask =
        D3D11_COLOR_WRITE_ENABLE_ALL;
    VERIFY_OK(
        m_gpu->CreateBlendState(&plusDesc,
                                m_plusBlendState.ReleaseAndGetAddressOf()));

    D3D11_BLEND_DESC maxDesc = plusDesc;
    maxDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_MAX;
    maxDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_MAX;
    VERIFY_OK(
        m_gpu->CreateBlendState(&maxDesc,
                                m_maxBlendState.ReleaseAndGetAddressOf()));
}

ComPtr<ID3D11Texture2D> RenderContextD3DImpl::makeSimple2DTexture(
    DXGI_FORMAT format,
    UINT width,
    UINT height,
    UINT mipLevelCount,
    UINT bindFlags,
    UINT miscFlags)
{
    return make_simple_2d_texture(m_gpu.Get(),
                                  format,
                                  width,
                                  height,
                                  mipLevelCount,
                                  bindFlags,
                                  miscFlags);
}

ComPtr<ID3D11UnorderedAccessView> RenderContextD3DImpl::makeSimple2DUAV(
    ID3D11Texture2D* tex,
    DXGI_FORMAT format)
{
    return make_simple_2d_uav(m_gpu.Get(), tex, format);
}

ComPtr<ID3D11Buffer> RenderContextD3DImpl::makeSimpleImmutableBuffer(
    size_t sizeInBytes,
    UINT bindFlags,
    const void* data)
{
    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = math::lossless_numeric_cast<UINT>(sizeInBytes);
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = bindFlags;
    desc.StructureByteStride = sizeof(PatchVertex);

    D3D11_SUBRESOURCE_DATA dataDesc{};
    dataDesc.pSysMem = data;

    ComPtr<ID3D11Buffer> buffer;
    VERIFY_OK(
        m_gpu->CreateBuffer(&desc, &dataDesc, buffer.ReleaseAndGetAddressOf()));
    return buffer;
}

class RenderBufferD3DImpl
    : public LITE_RTTI_OVERRIDE(RenderBuffer, RenderBufferD3DImpl)
{
public:
    RenderBufferD3DImpl(RenderBufferType renderBufferType,
                        RenderBufferFlags renderBufferFlags,
                        size_t sizeInBytes,
                        ComPtr<ID3D11Device> gpu,
                        ComPtr<ID3D11DeviceContext> gpuContext) :
        lite_rtti_override(renderBufferType, renderBufferFlags, sizeInBytes),
        m_gpu(std::move(gpu)),
        m_gpuContext(std::move(gpuContext))
    {
        m_desc.ByteWidth = math::lossless_numeric_cast<UINT>(sizeInBytes);
        m_desc.BindFlags = type() == RenderBufferType::vertex
                               ? D3D11_BIND_VERTEX_BUFFER
                               : D3D11_BIND_INDEX_BUFFER;
        if (flags() & RenderBufferFlags::mappedOnceAtInitialization)
        {
            m_desc.Usage = D3D11_USAGE_IMMUTABLE;
            m_desc.CPUAccessFlags = 0;
            m_mappedMemoryForImmutableBuffer.reset(new char[sizeInBytes]);
        }
        else
        {
            m_desc.Usage = D3D11_USAGE_DYNAMIC;
            m_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            VERIFY_OK(m_gpu->CreateBuffer(&m_desc,
                                          nullptr,
                                          m_buffer.ReleaseAndGetAddressOf()));
        }
    }

    ID3D11Buffer* buffer() const { return m_buffer.Get(); }

protected:
    void* onMap() override
    {
        if (flags() & RenderBufferFlags::mappedOnceAtInitialization)
        {
            assert(m_mappedMemoryForImmutableBuffer);
            return m_mappedMemoryForImmutableBuffer.get();
        }
        else
        {
            D3D11_MAPPED_SUBRESOURCE mappedSubresource;
            m_gpuContext->Map(m_buffer.Get(),
                              0,
                              D3D11_MAP_WRITE_DISCARD,
                              0,
                              &mappedSubresource);
            return mappedSubresource.pData;
        }
    }

    void onUnmap() override
    {
        if (flags() & RenderBufferFlags::mappedOnceAtInitialization)
        {
            assert(!m_buffer);
            D3D11_SUBRESOURCE_DATA bufferDataDesc{};
            bufferDataDesc.pSysMem = m_mappedMemoryForImmutableBuffer.get();
            VERIFY_OK(m_gpu->CreateBuffer(&m_desc,
                                          &bufferDataDesc,
                                          m_buffer.ReleaseAndGetAddressOf()));
            m_mappedMemoryForImmutableBuffer
                .reset(); // This buffer will only be mapped once.
        }
        else
        {
            m_gpuContext->Unmap(m_buffer.Get(), 0);
        }
    }

private:
    const ComPtr<ID3D11Device> m_gpu;
    const ComPtr<ID3D11DeviceContext> m_gpuContext;
    D3D11_BUFFER_DESC m_desc{};
    ComPtr<ID3D11Buffer> m_buffer;
    std::unique_ptr<char[]> m_mappedMemoryForImmutableBuffer;
};

rcp<RenderBuffer> RenderContextD3DImpl::makeRenderBuffer(
    RenderBufferType type,
    RenderBufferFlags flags,
    size_t sizeInBytes)
{
    return make_rcp<RenderBufferD3DImpl>(type,
                                         flags,
                                         sizeInBytes,
                                         m_gpu,
                                         m_gpuContext);
}

class TextureD3DImpl : public Texture
{
public:
    TextureD3DImpl(RenderContextD3DImpl* renderContextImpl,
                   UINT width,
                   UINT height,
                   UINT mipLevelCount,
                   const uint8_t imageDataRGBAPremul[]) :
        Texture(width, height)
    {
        m_texture = renderContextImpl->makeSimple2DTexture(
            DXGI_FORMAT_R8G8B8A8_UNORM,
            width,
            height,
            mipLevelCount,
            D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
            D3D11_RESOURCE_MISC_GENERATE_MIPS);

        // Specify the top-level image in the mipmap chain.
        D3D11_BOX box;
        box.left = 0;
        box.right = width;
        box.top = 0;
        box.bottom = height;
        box.front = 0;
        box.back = 1;
        renderContextImpl->gpuContext()->UpdateSubresource(m_texture.Get(),
                                                           0,
                                                           &box,
                                                           imageDataRGBAPremul,
                                                           width * 4,
                                                           0);

        // Create a view and generate mipmaps.
        VERIFY_OK(renderContextImpl->gpu()->CreateShaderResourceView(
            m_texture.Get(),
            NULL,
            m_srv.ReleaseAndGetAddressOf()));
        renderContextImpl->gpuContext()->GenerateMips(m_srv.Get());
    }

    ID3D11ShaderResourceView* srv() const { return m_srv.Get(); }
    ID3D11ShaderResourceView* const* srvAddressOf() const
    {
        return m_srv.GetAddressOf();
    }

private:
    ComPtr<ID3D11Texture2D> m_texture;
    ComPtr<ID3D11ShaderResourceView> m_srv;
};

rcp<Texture> RenderContextD3DImpl::makeImageTexture(
    uint32_t width,
    uint32_t height,
    uint32_t mipLevelCount,
    const uint8_t imageDataRGBAPremul[])
{
    return make_rcp<TextureD3DImpl>(this,
                                    width,
                                    height,
                                    mipLevelCount,
                                    imageDataRGBAPremul);
}

class BufferRingD3D : public BufferRing
{
public:
    BufferRingD3D(RenderContextD3DImpl* renderContextImpl,
                  size_t capacityInBytes,
                  UINT bindFlags) :
        BufferRingD3D(renderContextImpl, capacityInBytes, bindFlags, 0, 0)
    {}

    ID3D11Buffer* flush(ID3D11DeviceContext* gpuContext)
    {
        updateBuffer(gpuContext, 0, m_dirtySizeInBytes);
        m_dirtySizeInBytes = 0;
        return m_buffer.Get();
    }

protected:
    BufferRingD3D(RenderContextD3DImpl* renderContextImpl,
                  size_t capacityInBytes,
                  UINT bindFlags,
                  UINT elementSizeInBytes,
                  UINT miscFlags) :
        BufferRing(capacityInBytes)
    {
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = math::lossless_numeric_cast<UINT>(capacityInBytes);
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = bindFlags;
        desc.CPUAccessFlags = 0;
        desc.StructureByteStride = elementSizeInBytes;
        desc.MiscFlags = miscFlags;
        VERIFY_OK(renderContextImpl->gpu()->CreateBuffer(
            &desc,
            nullptr,
            m_buffer.ReleaseAndGetAddressOf()));
    }

    void* onMapBuffer(int bufferIdx, size_t mapSizeInBytes) override
    {
        // Use a CPU-side shadow buffer since D3D11 doesn't have an API to map a
        // sub-range.
        return shadowBuffer();
    }

    void onUnmapAndSubmitBuffer(int bufferIdx, size_t mapSizeInBytes) override
    {
        m_dirtySizeInBytes = math::lossless_numeric_cast<UINT>(mapSizeInBytes);
    }

    void updateBuffer(ID3D11DeviceContext* gpuContext,
                      UINT offsetInBytes,
                      UINT sizeInBytes) const
    {
        assert(offsetInBytes + sizeInBytes <= m_dirtySizeInBytes);
        if (sizeInBytes != 0)
        {
            D3D11_BOX box;
            box.left = 0;
            box.right = sizeInBytes;
            box.top = 0;
            box.bottom = 1;
            box.front = 0;
            box.back = 1;
            gpuContext->UpdateSubresource(m_buffer.Get(),
                                          0,
                                          &box,
                                          shadowBuffer() + offsetInBytes,
                                          0,
                                          0);
        }
    }

    ComPtr<ID3D11Buffer> m_buffer;
    UINT m_dirtySizeInBytes = 0;
};

class StructuredBufferRingD3D : public BufferRingD3D
{
public:
    StructuredBufferRingD3D(RenderContextD3DImpl* renderContextImpl,
                            size_t capacityInBytes,
                            UINT elementSizeInBytes) :
        BufferRingD3D(renderContextImpl,
                      capacityInBytes,
                      D3D11_BIND_SHADER_RESOURCE,
                      elementSizeInBytes,
                      D3D11_RESOURCE_MISC_BUFFER_STRUCTURED)
    {
        assert(capacityInBytes % elementSizeInBytes == 0);
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = math::lossless_numeric_cast<UINT>(
            capacityInBytes / elementSizeInBytes);
        VERIFY_OK(renderContextImpl->gpu()->CreateShaderResourceView(
            m_buffer.Get(),
            &srvDesc,
            m_srv.ReleaseAndGetAddressOf()));
    }

    ID3D11ShaderResourceView* flush(ID3D11DeviceContext* gpuContext,
                                    UINT offsetInBytes,
                                    UINT sizeInBytes) const
    {
        updateBuffer(gpuContext, offsetInBytes, sizeInBytes);
        return m_srv.Get();
    }

protected:
    ComPtr<ID3D11ShaderResourceView> m_srv;
};

std::unique_ptr<BufferRing> RenderContextD3DImpl::makeUniformBufferRing(
    size_t capacityInBytes)
{
    // In D3D we update uniform data inline with commands, rather than filling a
    // buffer up front.
    return std::make_unique<HeapBufferRing>(capacityInBytes);
}

std::unique_ptr<BufferRing> RenderContextD3DImpl::makeStorageBufferRing(
    size_t capacityInBytes,
    gpu::StorageBufferStructure bufferStructure)
{
    return capacityInBytes != 0
               ? std::make_unique<StructuredBufferRingD3D>(
                     this,
                     capacityInBytes,
                     gpu::StorageBufferElementSizeInBytes(bufferStructure))
               : nullptr;
}

std::unique_ptr<BufferRing> RenderContextD3DImpl::makeVertexBufferRing(
    size_t capacityInBytes)
{
    return capacityInBytes != 0
               ? std::make_unique<BufferRingD3D>(this,
                                                 capacityInBytes,
                                                 D3D11_BIND_VERTEX_BUFFER)
               : nullptr;
}

RenderTargetD3D::RenderTargetD3D(RenderContextD3DImpl* renderContextImpl,
                                 uint32_t width,
                                 uint32_t height) :
    RenderTarget(width, height),
    m_gpu(renderContextImpl->gpu()),
    m_gpuSupportsTypedUAVLoadStore(
        renderContextImpl->d3dCapabilities().supportsTypedUAVLoadStore)
{}

void RenderTargetD3D::setTargetTexture(ComPtr<ID3D11Texture2D> tex)
{
    if (tex != nullptr)
    {
        D3D11_TEXTURE2D_DESC desc;
        tex->GetDesc(&desc);
#ifdef DEBUG
        assert(desc.Width == width());
        assert(desc.Height == height());
        assert(desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM ||
               desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM ||
               desc.Format == DXGI_FORMAT_R8G8B8A8_TYPELESS ||
               desc.Format == DXGI_FORMAT_B8G8R8A8_TYPELESS);
#endif
        m_targetTextureSupportsUAV =
            (desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS) &&
            m_gpuSupportsTypedUAVLoadStore;
        m_targetFormat = desc.Format;
    }
    else
    {
        m_targetTextureSupportsUAV = false;
    }
    m_targetTexture = std::move(tex);
    m_targetRTV = nullptr;
    m_targetUAV = nullptr;
}

ID3D11RenderTargetView* RenderTargetD3D::targetRTV()
{
    if (m_targetRTV == nullptr && m_targetTexture != nullptr)
    {
        D3D11_RENDER_TARGET_VIEW_DESC desc{};
        desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

        switch (m_targetFormat)
        {
            case DXGI_FORMAT_R8G8B8A8_UNORM:
            case DXGI_FORMAT_R8G8B8A8_TYPELESS:
                desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                break;
            case DXGI_FORMAT_B8G8R8A8_UNORM:
            case DXGI_FORMAT_B8G8R8A8_TYPELESS:
                desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
                break;

            default:
                RIVE_UNREACHABLE();
        }

        VERIFY_OK(m_gpu->CreateRenderTargetView(
            m_targetTexture.Get(),
            &desc,
            m_targetRTV.ReleaseAndGetAddressOf()));
    }
    return m_targetRTV.Get();
}

ID3D11Texture2D* RenderTargetD3D::offscreenTexture()
{
    assert(!m_targetTextureSupportsUAV);
    if (m_offscreenTexture == nullptr)
    {
        m_offscreenTexture =
            make_simple_2d_texture(m_gpu.Get(),
                                   DXGI_FORMAT_R8G8B8A8_TYPELESS,
                                   width(),
                                   height(),
                                   1,
                                   D3D11_BIND_UNORDERED_ACCESS);
    }
    return m_offscreenTexture.Get();
}

ID3D11UnorderedAccessView* RenderTargetD3D::targetUAV()
{
    if (m_targetUAV == nullptr)
    {
        if (auto* uavTexture = m_targetTextureSupportsUAV
                                   ? m_targetTexture.Get()
                                   : offscreenTexture())
        {
            DXGI_FORMAT targetUavFormat;
            switch (m_targetFormat)
            {
                case DXGI_FORMAT_R8G8B8A8_UNORM:
                case DXGI_FORMAT_R8G8B8A8_TYPELESS:
                    targetUavFormat = m_gpuSupportsTypedUAVLoadStore
                                          ? DXGI_FORMAT_R8G8B8A8_UNORM
                                          : DXGI_FORMAT_R32_UINT;
                    break;
                case DXGI_FORMAT_B8G8R8A8_UNORM:
                case DXGI_FORMAT_B8G8R8A8_TYPELESS:
                    targetUavFormat = m_gpuSupportsTypedUAVLoadStore
                                          ? DXGI_FORMAT_B8G8R8A8_UNORM
                                          : DXGI_FORMAT_R32_UINT;
                    break;
                default:
                    RIVE_UNREACHABLE();
            }
            m_targetUAV =
                make_simple_2d_uav(m_gpu.Get(), uavTexture, targetUavFormat);
        }
    }
    return m_targetUAV.Get();
}

ID3D11UnorderedAccessView* RenderTargetD3D::clipUAV()
{
    if (m_clipTexture == nullptr)
    {
        m_clipTexture = make_simple_2d_texture(m_gpu.Get(),
                                               DXGI_FORMAT_R32_UINT,
                                               width(),
                                               height(),
                                               1,
                                               D3D11_BIND_UNORDERED_ACCESS);
    }
    if (m_clipUAV == nullptr)
    {
        m_clipUAV = make_simple_2d_uav(m_gpu.Get(),
                                       m_clipTexture.Get(),
                                       DXGI_FORMAT_R32_UINT);
    }
    return m_clipUAV.Get();
}

ID3D11UnorderedAccessView* RenderTargetD3D::scratchColorUAV()
{
    if (m_scratchColorTexture == nullptr)
    {
        m_scratchColorTexture =
            make_simple_2d_texture(m_gpu.Get(),
                                   DXGI_FORMAT_R8G8B8A8_TYPELESS,
                                   width(),
                                   height(),
                                   1,
                                   D3D11_BIND_UNORDERED_ACCESS);
    }
    if (m_scratchColorUAV == nullptr)
    {
        m_scratchColorUAV = make_simple_2d_uav(m_gpu.Get(),
                                               m_scratchColorTexture.Get(),
                                               m_gpuSupportsTypedUAVLoadStore
                                                   ? DXGI_FORMAT_R8G8B8A8_UNORM
                                                   : DXGI_FORMAT_R32_UINT);
    }
    return m_scratchColorUAV.Get();
}

ID3D11UnorderedAccessView* RenderTargetD3D::coverageUAV()
{
    if (m_coverageTexture == nullptr)
    {
        m_coverageTexture = make_simple_2d_texture(m_gpu.Get(),
                                                   DXGI_FORMAT_R32_UINT,
                                                   width(),
                                                   height(),
                                                   1,
                                                   D3D11_BIND_UNORDERED_ACCESS);
    }
    if (m_coverageUAV == nullptr)
    {
        m_coverageUAV = make_simple_2d_uav(m_gpu.Get(),
                                           m_coverageTexture.Get(),
                                           DXGI_FORMAT_R32_UINT);
    }
    return m_coverageUAV.Get();
}

void RenderContextD3DImpl::resizeGradientTexture(uint32_t width,
                                                 uint32_t height)
{
    if (width == 0 || height == 0)
    {
        m_gradTexture = nullptr;
        m_gradTextureSRV = nullptr;
        m_gradTextureRTV = nullptr;
    }
    else
    {
        m_gradTexture = makeSimple2DTexture(DXGI_FORMAT_R8G8B8A8_UNORM,
                                            width,
                                            height,
                                            1,
                                            D3D11_BIND_RENDER_TARGET |
                                                D3D11_BIND_SHADER_RESOURCE);
        VERIFY_OK(m_gpu->CreateShaderResourceView(
            m_gradTexture.Get(),
            NULL,
            m_gradTextureSRV.ReleaseAndGetAddressOf()));
        VERIFY_OK(m_gpu->CreateRenderTargetView(
            m_gradTexture.Get(),
            NULL,
            m_gradTextureRTV.ReleaseAndGetAddressOf()));
    }
}

void RenderContextD3DImpl::resizeTessellationTexture(uint32_t width,
                                                     uint32_t height)
{
    if (width == 0 || height == 0)
    {
        m_tessTexture = nullptr;
        m_tessTextureSRV = nullptr;
        m_tessTextureRTV = nullptr;
    }
    else
    {
        m_tessTexture = makeSimple2DTexture(DXGI_FORMAT_R32G32B32A32_UINT,
                                            width,
                                            height,
                                            1,
                                            D3D11_BIND_RENDER_TARGET |
                                                D3D11_BIND_SHADER_RESOURCE);
        VERIFY_OK(m_gpu->CreateShaderResourceView(
            m_tessTexture.Get(),
            NULL,
            m_tessTextureSRV.ReleaseAndGetAddressOf()));
        VERIFY_OK(m_gpu->CreateRenderTargetView(
            m_tessTexture.Get(),
            NULL,
            m_tessTextureRTV.ReleaseAndGetAddressOf()));
    }
}

void RenderContextD3DImpl::resizeAtlasTexture(uint32_t width, uint32_t height)
{

    if (width == 0 || height == 0)
    {
        m_atlasTexture = nullptr;
        m_atlasTextureSRV = nullptr;
        m_atlasTextureRTV = nullptr;
    }
    else
    {
        m_atlasTexture = makeSimple2DTexture(DXGI_FORMAT_R32_FLOAT,
                                             width,
                                             height,
                                             1,
                                             D3D11_BIND_RENDER_TARGET |
                                                 D3D11_BIND_SHADER_RESOURCE);
        VERIFY_OK(m_gpu->CreateShaderResourceView(
            m_atlasTexture.Get(),
            NULL,
            m_atlasTextureSRV.ReleaseAndGetAddressOf()));
        VERIFY_OK(m_gpu->CreateRenderTargetView(
            m_atlasTexture.Get(),
            NULL,
            m_atlasTextureRTV.ReleaseAndGetAddressOf()));
    }
}

static const char* heap_buffer_contents(const BufferRing* bufferRing)
{
    assert(bufferRing != nullptr);
    auto heapBuffer = static_cast<const HeapBufferRing*>(bufferRing);
    return reinterpret_cast<const char*>(heapBuffer->contents());
}

static ID3D11Buffer* flush_buffer(ID3D11DeviceContext* gpuContext,
                                  BufferRing* bufferRing)
{
    assert(bufferRing != nullptr);
    return static_cast<BufferRingD3D*>(bufferRing)->flush(gpuContext);
}

template <typename HighLevelStruct>
ID3D11ShaderResourceView* flush_structured_buffer(
    ID3D11DeviceContext* gpuContext,
    BufferRing* bufferRing,
    UINT highLevelStructCount,
    UINT firstHighLevelStruct)
{
    return static_cast<const StructuredBufferRingD3D*>(bufferRing)
        ->flush(gpuContext,
                firstHighLevelStruct * sizeof(HighLevelStruct),
                highLevelStructCount * sizeof(HighLevelStruct));
}

static void blit_sub_rect(ID3D11DeviceContext* gpuContext,
                          ID3D11Texture2D* dst,
                          ID3D11Texture2D* src,
                          const IAABB& rect)
{
    D3D11_BOX updateBox = {
        static_cast<UINT>(rect.left),
        static_cast<UINT>(rect.top),
        0,
        static_cast<UINT>(rect.right),
        static_cast<UINT>(rect.bottom),
        1,
    };
    gpuContext->CopySubresourceRegion(dst,
                                      0,
                                      updateBox.left,
                                      updateBox.top,
                                      0,
                                      src,
                                      0,
                                      &updateBox);
}

static D3D11_RECT make_scissor(const TAABB<uint16_t> scissor)
{
    D3D11_RECT rect;
    rect.left = scissor.left;
    rect.top = scissor.top;
    rect.right = scissor.right;
    rect.bottom = scissor.bottom;
    return rect;
}

void RenderContextD3DImpl::flush(const FlushDescriptor& desc)
{
    assert(desc.interlockMode != gpu::InterlockMode::clockwiseAtomic);
    auto renderTarget = static_cast<RenderTargetD3D*>(desc.renderTarget);

    m_gpuContext->ClearState();

    // All programs use the same set of per-flush uniforms.
    m_gpuContext->UpdateSubresource(
        m_flushUniforms.Get(),
        0,
        NULL,
        heap_buffer_contents(flushUniformBufferRing()) +
            desc.flushUniformDataOffsetInBytes,
        0,
        0);

    ID3D11Buffer* uniformBuffers[] = {m_flushUniforms.Get(),
                                      m_drawUniforms.Get(),
                                      m_imageDrawUniforms.Get()};
    static_assert(PATH_BASE_INSTANCE_UNIFORM_BUFFER_IDX ==
                  FLUSH_UNIFORM_BUFFER_IDX + 1);
    static_assert(IMAGE_DRAW_UNIFORM_BUFFER_IDX ==
                  PATH_BASE_INSTANCE_UNIFORM_BUFFER_IDX + 1);
    m_gpuContext->VSSetConstantBuffers(FLUSH_UNIFORM_BUFFER_IDX,
                                       std::size(uniformBuffers),
                                       uniformBuffers);
    m_gpuContext->PSSetConstantBuffers(FLUSH_UNIFORM_BUFFER_IDX,
                                       std::size(uniformBuffers),
                                       uniformBuffers);

    // All programs use the same storage buffers.
    ID3D11ShaderResourceView* storageBufferBufferSRVs[] = {
        desc.pathCount > 0
            ? flush_structured_buffer<gpu::PathData>(
                  m_gpuContext.Get(),
                  pathBufferRing(),
                  desc.pathCount,
                  math::lossless_numeric_cast<UINT>(desc.firstPath))
            : nullptr,
        desc.pathCount > 0
            ? flush_structured_buffer<gpu::PaintData>(
                  m_gpuContext.Get(),
                  paintBufferRing(),
                  desc.pathCount,
                  math::lossless_numeric_cast<UINT>(desc.firstPaint))
            : nullptr,
        desc.pathCount > 0
            ? flush_structured_buffer<gpu::PaintAuxData>(
                  m_gpuContext.Get(),
                  paintAuxBufferRing(),
                  desc.pathCount,
                  math::lossless_numeric_cast<UINT>(desc.firstPaintAux))
            : nullptr,
        desc.contourCount > 0
            ? flush_structured_buffer<gpu::ContourData>(
                  m_gpuContext.Get(),
                  contourBufferRing(),
                  desc.contourCount,
                  math::lossless_numeric_cast<UINT>(desc.firstContour))
            : nullptr,
    };
    static_assert(PAINT_BUFFER_IDX == PATH_BUFFER_IDX + 1);
    static_assert(PAINT_AUX_BUFFER_IDX == PAINT_BUFFER_IDX + 1);
    static_assert(CONTOUR_BUFFER_IDX == PAINT_AUX_BUFFER_IDX + 1);
    m_gpuContext->VSSetShaderResources(PATH_BUFFER_IDX,
                                       std::size(storageBufferBufferSRVs),
                                       storageBufferBufferSRVs);
    if (desc.interlockMode == gpu::InterlockMode::atomics)
    {
        // Atomic mode accesses the paint buffers from the pixel shader.
        m_gpuContext->PSSetShaderResources(PAINT_BUFFER_IDX,
                                           2,
                                           storageBufferBufferSRVs + 1);
    }

    // All programs use the same feather texture.
    m_gpuContext->VSSetShaderResources(FEATHER_TEXTURE_IDX,
                                       1,
                                       m_featherTextureSRV.GetAddressOf());
    m_gpuContext->PSSetShaderResources(FEATHER_TEXTURE_IDX,
                                       1,
                                       m_featherTextureSRV.GetAddressOf());

    // All programs use the same samplers.
    ID3D11SamplerState* samplers[4] = {
        m_linearSampler.Get(),
        m_linearSampler.Get(),
        m_linearSampler.Get(),
        // 0 is the default which is MIN_MAG_MIP_LINEAR
        m_samplerStates[ImageSampler::LINEAR_CLAMP_SAMPLER_KEY].Get(),
    };

    static_assert(FEATHER_TEXTURE_IDX == GRAD_TEXTURE_IDX + 1);
    static_assert(ATLAS_TEXTURE_IDX == FEATHER_TEXTURE_IDX + 1);
    static_assert(IMAGE_TEXTURE_IDX == ATLAS_TEXTURE_IDX + 1);
    m_gpuContext->PSSetSamplers(GRAD_TEXTURE_IDX, 4, samplers);
    m_gpuContext->VSSetSamplers(GRAD_TEXTURE_IDX, 4, samplers);

    // Render the complex color ramps to the gradient texture.
    if (desc.gradSpanCount > 0)
    {
        ID3D11Buffer* gradSpanBuffer =
            flush_buffer(m_gpuContext.Get(), gradSpanBufferRing());
        UINT gradStride = sizeof(GradientSpan);
        UINT gradOffset = 0;
        m_gpuContext->IASetVertexBuffers(0,
                                         1,
                                         &gradSpanBuffer,
                                         &gradStride,
                                         &gradOffset);

        m_gpuContext->IASetPrimitiveTopology(
            D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

        m_pipelineManager.setColorRampState();

        D3D11_VIEWPORT viewport = {0,
                                   0,
                                   static_cast<float>(kGradTextureWidth),
                                   static_cast<float>(desc.gradDataHeight),
                                   0,
                                   1};
        m_gpuContext->RSSetViewports(1, &viewport);

        // Unbind the gradient texture before rendering it.
        ID3D11ShaderResourceView* nullTextureView = nullptr;
        m_gpuContext->PSSetShaderResources(GRAD_TEXTURE_IDX,
                                           1,
                                           &nullTextureView);

        m_gpuContext->OMSetRenderTargets(1,
                                         m_gradTextureRTV.GetAddressOf(),
                                         NULL);

        m_gpuContext->DrawInstanced(
            gpu::GRAD_SPAN_TRI_STRIP_VERTEX_COUNT,
            desc.gradSpanCount,
            0,
            math::lossless_numeric_cast<UINT>(desc.firstGradSpan));
    }

    // Tessellate all curves into vertices in the tessellation texture.
    if (desc.tessVertexSpanCount > 0)
    {
        ID3D11Buffer* tessSpanBuffer =
            flush_buffer(m_gpuContext.Get(), tessSpanBufferRing());
        UINT tessStride = sizeof(TessVertexSpan);
        UINT tessOffset = 0;
        m_gpuContext->IASetVertexBuffers(0,
                                         1,
                                         &tessSpanBuffer,
                                         &tessStride,
                                         &tessOffset);
        m_gpuContext->IASetIndexBuffer(m_tessSpanIndexBuffer.Get(),
                                       DXGI_FORMAT_R16_UINT,
                                       0);

        m_pipelineManager.setTesselationState();
        m_gpuContext->IASetPrimitiveTopology(
            D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // Unbind the tessellation texture before rendering it.
        ID3D11ShaderResourceView* nullTessTextureView = NULL;
        m_gpuContext->VSSetShaderResources(TESS_VERTEX_TEXTURE_IDX,
                                           1,
                                           &nullTessTextureView);

        D3D11_VIEWPORT viewport = {0,
                                   0,
                                   static_cast<float>(kTessTextureWidth),
                                   static_cast<float>(desc.tessDataHeight),
                                   0,
                                   1};
        m_gpuContext->RSSetViewports(1, &viewport);

        m_gpuContext->OMSetRenderTargets(1,
                                         m_tessTextureRTV.GetAddressOf(),
                                         NULL);

        m_gpuContext->DrawIndexedInstanced(
            std::size(gpu::kTessSpanIndices),
            desc.tessVertexSpanCount,
            0,
            0,
            math::lossless_numeric_cast<UINT>(desc.firstTessVertexSpan));

        if (m_d3dCapabilities.isIntel)
        {
            // FIXME! Intel needs this flush! Driver bug? Find a lighter
            // workaround?
            m_gpuContext->Flush();
        }
    }

    ID3D11Buffer* vertexBuffers[3] = {
        m_patchVertexBuffer.Get(),
        desc.hasTriangleVertices
            ? flush_buffer(m_gpuContext.Get(), triangleBufferRing())
            : NULL,
        m_imageRectVertexBuffer.Get()};
    UINT vertexStrides[3] = {sizeof(gpu::PatchVertex),
                             sizeof(gpu::TriangleVertex),
                             sizeof(gpu::ImageRectVertex)};
    UINT vertexOffsets[3] = {0, 0, 0};
    static_assert(PATCH_VERTEX_DATA_SLOT == 0);
    static_assert(TRIANGLE_VERTEX_DATA_SLOT == 1);
    static_assert(IMAGE_RECT_VERTEX_DATA_SLOT == 2);
    m_gpuContext->IASetVertexBuffers(0,
                                     3,
                                     vertexBuffers,
                                     vertexStrides,
                                     vertexOffsets);

    // Unbind the tess and grad texture as a render target before binding them
    // as images.
    m_gpuContext->OMSetRenderTargets(0, NULL, NULL);
    m_gpuContext->VSSetShaderResources(TESS_VERTEX_TEXTURE_IDX,
                                       1,
                                       m_tessTextureSRV.GetAddressOf());
    m_gpuContext->PSSetShaderResources(GRAD_TEXTURE_IDX,
                                       1,
                                       m_gradTextureSRV.GetAddressOf());

    // Render the atlas if we have any offscreen feathers.
    if ((desc.atlasFillBatchCount | desc.atlasStrokeBatchCount) != 0)
    {
        float clearZero[4]{};
        m_gpuContext->ClearRenderTargetView(m_atlasTextureRTV.Get(), clearZero);

        m_pipelineManager.setAtlasVertexState();
        m_gpuContext->IASetPrimitiveTopology(
            D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_gpuContext->IASetIndexBuffer(m_patchIndexBuffer.Get(),
                                       DXGI_FORMAT_R16_UINT,
                                       0);
        m_gpuContext->RSSetState(m_atlasRasterState.Get());

        D3D11_VIEWPORT viewport = {0,
                                   0,
                                   static_cast<float>(desc.atlasContentWidth),
                                   static_cast<float>(desc.atlasContentHeight),
                                   0,
                                   1};
        m_gpuContext->RSSetViewports(1, &viewport);

        m_gpuContext->OMSetRenderTargets(1,
                                         m_atlasTextureRTV.GetAddressOf(),
                                         NULL);

        if (desc.atlasFillBatchCount != 0)
        {
            m_pipelineManager.setAtlasFillState();
            m_gpuContext->OMSetBlendState(m_plusBlendState.Get(),
                                          NULL,
                                          0xffffffff);
            for (size_t i = 0; i < desc.atlasFillBatchCount; ++i)
            {
                const gpu::AtlasDrawBatch& fillBatch = desc.atlasFillBatches[i];
                D3D11_RECT scissor = make_scissor(fillBatch.scissor);
                m_gpuContext->RSSetScissorRects(1, &scissor);
                DrawUniforms drawUniforms(fillBatch.basePatch);
                m_gpuContext->UpdateSubresource(m_drawUniforms.Get(),
                                                0,
                                                NULL,
                                                &drawUniforms,
                                                0,
                                                0);
                m_gpuContext->DrawIndexedInstanced(
                    gpu::kMidpointFanCenterAAPatchIndexCount,
                    fillBatch.patchCount,
                    gpu::kMidpointFanCenterAAPatchBaseIndex,
                    0,
                    fillBatch.basePatch);
            }
        }

        if (desc.atlasStrokeBatchCount != 0)
        {
            m_pipelineManager.setAtlasStrokeState();
            m_gpuContext->OMSetBlendState(m_maxBlendState.Get(),
                                          NULL,
                                          0xffffffff);
            for (size_t i = 0; i < desc.atlasStrokeBatchCount; ++i)
            {
                const gpu::AtlasDrawBatch& strokeBatch =
                    desc.atlasStrokeBatches[i];
                D3D11_RECT scissor = make_scissor(strokeBatch.scissor);
                m_gpuContext->RSSetScissorRects(1, &scissor);
                DrawUniforms drawUniforms(strokeBatch.basePatch);
                m_gpuContext->UpdateSubresource(m_drawUniforms.Get(),
                                                0,
                                                NULL,
                                                &drawUniforms,
                                                0,
                                                0);
                m_gpuContext->DrawIndexedInstanced(
                    gpu::kMidpointFanPatchBorderIndexCount,
                    strokeBatch.patchCount,
                    gpu::kMidpointFanPatchBaseIndex,
                    0,
                    strokeBatch.basePatch);
            }
        }

        m_gpuContext->OMSetBlendState(NULL, NULL, 0xffffffff);
    }

    // Setup and clear the PLS textures.
    switch (desc.colorLoadAction)
    {
        case gpu::LoadAction::clear:
            if (desc.fixedFunctionColorOutput)
            {
                float clearColor4f[4];
                UnpackColorToRGBA32FPremul(desc.colorClearValue, clearColor4f);
                m_gpuContext->ClearRenderTargetView(renderTarget->targetRTV(),
                                                    clearColor4f);
            }
            else if (m_d3dCapabilities.supportsTypedUAVLoadStore)
            {
                float clearColor4f[4];
                UnpackColorToRGBA32FPremul(desc.colorClearValue, clearColor4f);
                m_gpuContext->ClearUnorderedAccessViewFloat(
                    renderTarget->targetUAV(),
                    clearColor4f);
            }
            else
            {
                UINT clearColorui[4] = {
                    gpu::SwizzleRiveColorToRGBAPremul(desc.colorClearValue)};
                m_gpuContext->ClearUnorderedAccessViewUint(
                    renderTarget->targetUAV(),
                    clearColorui);
            }
            break;
        case gpu::LoadAction::preserveRenderTarget:
            if (!desc.fixedFunctionColorOutput &&
                !renderTarget->targetTextureSupportsUAV())
            {
                // We're rendering to an offscreen UAV and preserving the
                // target. Copy the target texture over.
                blit_sub_rect(m_gpuContext.Get(),
                              renderTarget->offscreenTexture(),
                              renderTarget->targetTexture(),
                              desc.renderTargetUpdateBounds);
            }
            break;
        case gpu::LoadAction::dontCare:
            break;
    }
    if (desc.combinedShaderFeatures & gpu::ShaderFeatures::ENABLE_CLIPPING)
    {
        constexpr static UINT kZero[4]{};
        m_gpuContext->ClearUnorderedAccessViewUint(renderTarget->clipUAV(),
                                                   kZero);
    }
    {
        UINT coverageClear[4]{desc.coverageClearValue};
        m_gpuContext->ClearUnorderedAccessViewUint(renderTarget->coverageUAV(),
                                                   coverageClear);
    }

    // Execute the DrawList.
    ID3D11RenderTargetView* targetRTV =
        desc.fixedFunctionColorOutput ? renderTarget->targetRTV() : NULL;
    ID3D11UnorderedAccessView* plsUAVs[] = {
        desc.fixedFunctionColorOutput ? NULL : renderTarget->targetUAV(),
        renderTarget->clipUAV(),
        desc.interlockMode == gpu::InterlockMode::rasterOrdering
            ? renderTarget->scratchColorUAV()
            : NULL, // Atomic mode doesn't use the scratchColor.
        renderTarget->coverageUAV(),
    };
    static_assert(COLOR_PLANE_IDX == 0);
    static_assert(CLIP_PLANE_IDX == 1);
    static_assert(SCRATCH_COLOR_PLANE_IDX == 2);
    static_assert(COVERAGE_PLANE_IDX == 3);
    m_gpuContext->OMSetRenderTargetsAndUnorderedAccessViews(
        desc.fixedFunctionColorOutput ? 1 : 0,
        &targetRTV,
        NULL,
        desc.fixedFunctionColorOutput ? 1 : 0,
        desc.fixedFunctionColorOutput ? std::size(plsUAVs) - 1
                                      : std::size(plsUAVs),
        desc.fixedFunctionColorOutput ? plsUAVs + 1 : plsUAVs,
        NULL);

    if (desc.fixedFunctionColorOutput)
    {
        // When rendering directly to the target RTV, we use the built-in blend
        // hardware for opacity and antialiasing.
        m_gpuContext->OMSetBlendState(m_srcOverBlendState.Get(),
                                      NULL,
                                      0xffffffff);
    }

    D3D11_VIEWPORT viewport = {0,
                               0,
                               static_cast<float>(renderTarget->width()),
                               static_cast<float>(renderTarget->height()),
                               0,
                               1};
    m_gpuContext->RSSetViewports(1, &viewport);

    // Set this last, when the atlas texture is no longer bound as a render
    // target.
    m_gpuContext->PSSetShaderResources(ATLAS_TEXTURE_IDX,
                                       1,
                                       m_atlasTextureSRV.GetAddressOf());

    m_gpuContext->PSSetConstantBuffers(IMAGE_DRAW_UNIFORM_BUFFER_IDX,
                                       1,
                                       m_imageDrawUniforms.GetAddressOf());

    const char* const imageDrawUniformData =
        heap_buffer_contents(imageDrawUniformBufferRing());

    bool renderPassHasCoalescedResolveAndTransfer =
        desc.interlockMode == gpu::InterlockMode::atomics &&
        !desc.fixedFunctionColorOutput &&
        !renderTarget->targetTextureSupportsUAV();

    for (const DrawBatch& batch : *desc.drawList)
    {
        DrawType drawType = batch.drawType;

        auto shaderFeatures = desc.interlockMode == gpu::InterlockMode::atomics
                                  ? desc.combinedShaderFeatures
                                  : batch.shaderFeatures;
        auto shaderMiscFlags = batch.shaderMiscFlags;
        if (drawType == gpu::DrawType::renderPassResolve &&
            renderPassHasCoalescedResolveAndTransfer)
        {
            assert(desc.interlockMode == gpu::InterlockMode::atomics);
            shaderMiscFlags |=
                gpu::ShaderMiscFlags::coalescedResolveAndTransfer;
        }
        if (desc.fixedFunctionColorOutput)
        {
            shaderMiscFlags |= gpu::ShaderMiscFlags::fixedFunctionColorOutput;
        }
        if (desc.interlockMode == gpu::InterlockMode::rasterOrdering &&
            (batch.drawContents & gpu::DrawContents::clockwiseFill))
        {
            shaderMiscFlags |= gpu::ShaderMiscFlags::clockwiseFill;
        }

        if (!m_pipelineManager.setPipelineState(drawType,
                                                shaderFeatures,
                                                desc.interlockMode,
                                                shaderMiscFlags,
                                                m_platformFeatures
#ifdef WITH_RIVE_TOOLS
                                                ,
                                                desc.synthesizedFailureType
#endif
                                                ))
        {
            // There was an issue getting either the requested pipeline state or
            // its ubershader counterpart so we cannot draw anything.
            continue;
        }

        if (auto imageTextureD3D =
                static_cast<const TextureD3DImpl*>(batch.imageTexture))
        {
            m_gpuContext->PSSetShaderResources(IMAGE_TEXTURE_IDX,
                                               1,
                                               imageTextureD3D->srvAddressOf());
        }

        {
            // we should never get a sampler option that is greater then our
            // array size
            assert(batch.imageSampler.asKey() <
                   ImageSampler::MAX_SAMPLER_PERMUTATIONS);
            ID3D11SamplerState* samplers[1] = {
                m_samplerStates[batch.imageSampler.asKey()].Get()};
            m_gpuContext->PSSetSamplers(IMAGE_SAMPLER_IDX, 1, samplers);
        }

        switch (drawType)
        {
            case DrawType::midpointFanPatches:
            case DrawType::midpointFanCenterAAPatches:
            case DrawType::outerCurvePatches:
            {
                m_gpuContext->IASetPrimitiveTopology(
                    D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                m_gpuContext->IASetIndexBuffer(m_patchIndexBuffer.Get(),
                                               DXGI_FORMAT_R16_UINT,
                                               0);
                m_gpuContext->RSSetState(
                    m_backCulledRasterState[desc.wireframe].Get());
                DrawUniforms drawUniforms(batch.baseElement);
                m_gpuContext->UpdateSubresource(m_drawUniforms.Get(),
                                                0,
                                                NULL,
                                                &drawUniforms,
                                                0,
                                                0);
                m_gpuContext->DrawIndexedInstanced(PatchIndexCount(drawType),
                                                   batch.elementCount,
                                                   PatchBaseIndex(drawType),
                                                   0,
                                                   batch.baseElement);
                break;
            }
            case DrawType::interiorTriangulation:
            case DrawType::atlasBlit:
            {
                m_gpuContext->IASetPrimitiveTopology(
                    D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                m_gpuContext->RSSetState(
                    m_backCulledRasterState[desc.wireframe].Get());
                m_gpuContext->Draw(batch.elementCount, batch.baseElement);
                break;
            }
            case DrawType::imageRect:
            {
                m_gpuContext->IASetPrimitiveTopology(
                    D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                m_gpuContext->IASetIndexBuffer(m_imageRectIndexBuffer.Get(),
                                               DXGI_FORMAT_R16_UINT,
                                               0);
                m_gpuContext->RSSetState(
                    m_doubleSidedRasterState[desc.wireframe].Get());
                m_gpuContext->UpdateSubresource(m_imageDrawUniforms.Get(),
                                                0,
                                                NULL,
                                                imageDrawUniformData +
                                                    batch.imageDrawDataOffset,
                                                0,
                                                0);
                m_gpuContext->DrawIndexed(std::size(gpu::kImageRectIndices),
                                          0,
                                          0);
                break;
            }
            case DrawType::imageMesh:
            {
                LITE_RTTI_CAST_OR_BREAK(vertexBuffer,
                                        RenderBufferD3DImpl*,
                                        batch.vertexBuffer);
                LITE_RTTI_CAST_OR_BREAK(uvBuffer,
                                        RenderBufferD3DImpl*,
                                        batch.uvBuffer);
                LITE_RTTI_CAST_OR_BREAK(indexBuffer,
                                        RenderBufferD3DImpl*,
                                        batch.indexBuffer);
                ID3D11Buffer* imageMeshBuffers[] = {vertexBuffer->buffer(),
                                                    uvBuffer->buffer()};
                UINT imageMeshStrides[] = {sizeof(Vec2D), sizeof(Vec2D)};
                UINT imageMeshOffsets[] = {0, 0};
                m_gpuContext->IASetVertexBuffers(IMAGE_MESH_VERTEX_DATA_SLOT,
                                                 2,
                                                 imageMeshBuffers,
                                                 imageMeshStrides,
                                                 imageMeshOffsets);
                static_assert(IMAGE_MESH_UV_DATA_SLOT ==
                              IMAGE_MESH_VERTEX_DATA_SLOT + 1);
                m_gpuContext->IASetPrimitiveTopology(
                    D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                m_gpuContext->IASetIndexBuffer(indexBuffer->buffer(),
                                               DXGI_FORMAT_R16_UINT,
                                               0);
                m_gpuContext->RSSetState(
                    m_doubleSidedRasterState[desc.wireframe].Get());
                m_gpuContext->UpdateSubresource(m_imageDrawUniforms.Get(),
                                                0,
                                                NULL,
                                                imageDrawUniformData +
                                                    batch.imageDrawDataOffset,
                                                0,
                                                0);
                m_gpuContext->DrawIndexed(batch.elementCount,
                                          batch.baseElement,
                                          0);
                break;
            }
            case DrawType::renderPassResolve:
                assert(desc.interlockMode == gpu::InterlockMode::atomics);
                m_gpuContext->IASetPrimitiveTopology(
                    D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
                m_gpuContext->RSSetState(m_backCulledRasterState[0].Get());
                if (renderPassHasCoalescedResolveAndTransfer)
                {
                    // Bind the actual target texture as the render target for
                    // the PLS resolve, so we don't have to copy to it after the
                    // render pass. (And ince we're changing the render target,
                    // this also better be the final batch of the render pass.)
                    assert(&batch == &desc.drawList->tail());
                    assert(!desc.fixedFunctionColorOutput);
                    assert(!renderTarget->targetTextureSupportsUAV());
                    ID3D11RenderTargetView* resolveRTV =
                        renderTarget->targetRTV();
                    ID3D11UnorderedAccessView* resolveUAVs[] = {
                        renderTarget->clipUAV(),
                        renderTarget
                            ->targetUAV(), // Bind the target UAV (for reading)
                                           // to a different slot for the
                                           // resolve because D3D doesn't let us
                                           // use slot 0 when there's a render
                                           // target.
                        renderTarget->coverageUAV(),
                    };
                    static_assert(CLIP_PLANE_IDX == 1);
                    static_assert(COALESCED_OFFSCREEN_COLOR_PLANE_IDX == 2);
                    static_assert(COVERAGE_PLANE_IDX == 3);
                    m_gpuContext->OMSetRenderTargetsAndUnorderedAccessViews(
                        1,
                        &resolveRTV,
                        NULL,
                        1,
                        std::size(resolveUAVs),
                        resolveUAVs,
                        NULL);
                }
                m_gpuContext->Draw(4, 0);
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
    }

    if (desc.interlockMode == gpu::InterlockMode::rasterOrdering &&
        !renderTarget->targetTextureSupportsUAV())
    {
        // We rendered to an offscreen UAV and did not resolve to the
        // renderTarget. Copy back to the main target.
        assert(!desc.fixedFunctionColorOutput);
        assert(!renderPassHasCoalescedResolveAndTransfer);
        blit_sub_rect(m_gpuContext.Get(),
                      renderTarget->targetTexture(),
                      renderTarget->offscreenTexture(),
                      desc.renderTargetUpdateBounds);
    }
}
} // namespace rive::gpu
