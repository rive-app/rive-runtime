/*
 * Copyright 2023 Rive
 */

#include "rive/pls/d3d/pls_render_context_d3d.hpp"

#include <D3DCompiler.h>
#include <sstream>

#include "../out/obj/generated/advanced_blend.glsl.hpp"
#include "../out/obj/generated/color_ramp.glsl.hpp"
#include "../out/obj/generated/common.glsl.hpp"
#include "../out/obj/generated/draw.glsl.hpp"
#include "../out/obj/generated/hlsl.glsl.hpp"
#include "../out/obj/generated/tessellate.glsl.hpp"

constexpr static UINT kPatchVertexDataSlot = 0;
constexpr static UINT kTriangleVertexDataSlot = 1;

namespace rive::pls
{
static DXGI_FORMAT d3d_format(TexelBufferRing::Format format)
{

    switch (format)
    {
        case TexelBufferRing::Format::rgba32f:
            return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case TexelBufferRing::Format::rgba32ui:
            return DXGI_FORMAT_R32G32B32A32_UINT;
        case TexelBufferRing::Format::rgba8:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
    }
    RIVE_UNREACHABLE();
}

static ComPtr<ID3D11Texture2D> make_simple_2d_texture(ID3D11Device* gpu,
                                                      DXGI_FORMAT format,
                                                      size_t width,
                                                      size_t height,
                                                      UINT bindFlags)
{
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = bindFlags;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;

    ComPtr<ID3D11Texture2D> tex;
    VERIFY_OK(gpu->CreateTexture2D(&desc, NULL, tex.GetAddressOf()));
    return tex;
}

static ComPtr<ID3D11ShaderResourceView> make_simple_2d_srv(ID3D11Device* gpu,
                                                           ID3D11Texture2D* tex2D,
                                                           DXGI_FORMAT format)
{
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.Format = format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

    ComPtr<ID3D11ShaderResourceView> srv;
    VERIFY_OK(gpu->CreateShaderResourceView(tex2D, &srvDesc, srv.GetAddressOf()));
    return srv;
}

static ComPtr<ID3D11RenderTargetView> make_simple_2d_rtv(ID3D11Device* gpu,
                                                         ID3D11Texture2D* tex2D,
                                                         DXGI_FORMAT format)
{
    D3D11_RENDER_TARGET_VIEW_DESC rtDesc;
    rtDesc.Format = format;
    rtDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    rtDesc.Texture2D.MipSlice = 0;

    ComPtr<ID3D11RenderTargetView> rtv;
    VERIFY_OK(gpu->CreateRenderTargetView(tex2D, &rtDesc, &rtv));
    return rtv;
}

static ComPtr<ID3D11UnorderedAccessView> make_simple_2d_uav(ID3D11Device* gpu,
                                                            ID3D11Texture2D* tex,
                                                            DXGI_FORMAT format)
{
    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.Format = format;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;

    ComPtr<ID3D11UnorderedAccessView> uav;
    VERIFY_OK(gpu->CreateUnorderedAccessView(tex, &uavDesc, uav.GetAddressOf()));
    return uav;
}

static ComPtr<ID3DBlob> compile_source_to_blob(ID3D11Device* gpu,
                                               const char* shaderTypeDefineName,
                                               const std::string& commonSource,
                                               const char* entrypoint,
                                               const char* target)
{
    std::ostringstream source;
    source << "#define " << shaderTypeDefineName << '\n';
    source << commonSource;

    const std::string& sourceStr = source.str();
    ComPtr<ID3DBlob> blob;
    ComPtr<ID3DBlob> errors;
    HRESULT hr = D3DCompile(sourceStr.c_str(),
                            sourceStr.length(),
                            nullptr,
                            nullptr,
                            nullptr,
                            entrypoint,
                            target,
                            D3DCOMPILE_ENABLE_STRICTNESS,
                            0,
                            &blob,
                            &errors);
    if (errors && errors->GetBufferPointer())
    {
        fprintf(stderr, "Errors or warnings compiling shader.\n");
        int l = 1;
        std::stringstream stream(sourceStr);
        std::string lineStr;
        while (std::getline(stream, lineStr, '\n'))
        {
            fprintf(stderr, "%4i| %s\n", l++, lineStr.c_str());
        }
        fprintf(stderr, "%s\n", reinterpret_cast<char*>(errors->GetBufferPointer()));
        exit(-1);
    }
    if (FAILED(hr))
    {
        fprintf(stderr, "Failed to compile shader.\n");
        exit(-1);
    }
    return blob;
}

static PlatformFeatures platform_features_d3d()
{
    PlatformFeatures platformFeatures;
    platformFeatures.invertOffscreenY = true;
    return platformFeatures;
}

PLSRenderContextD3D::PLSRenderContextD3D(ComPtr<ID3D11Device> gpu,
                                         ComPtr<ID3D11DeviceContext> gpuContext,
                                         bool isIntel) :
    PLSRenderContext(platform_features_d3d()),
    m_isIntel(isIntel),
    m_gpu(gpu),
    m_gpuContext(gpuContext)
{
    D3D11_RASTERIZER_DESC rasterDesc;
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.CullMode = D3D11_CULL_BACK;
    rasterDesc.FrontCounterClockwise = FALSE; // FrontCounterClockwise must be FALSE in order to
                                              // match the winding sense of interior triangulations.
    rasterDesc.DepthBias = 0;
    rasterDesc.SlopeScaledDepthBias = 0;
    rasterDesc.DepthBiasClamp = 0;
    rasterDesc.DepthClipEnable = FALSE;
    rasterDesc.ScissorEnable = FALSE;
    rasterDesc.MultisampleEnable = FALSE;
    rasterDesc.AntialiasedLineEnable = FALSE;
    VERIFY_OK(m_gpu->CreateRasterizerState(&rasterDesc, m_rasterState.GetAddressOf()));
    m_gpuContext->RSSetState(m_rasterState.Get());

    // Make one more raster state for wireframe, for debugging purposes.
    rasterDesc.FillMode = D3D11_FILL_WIREFRAME;
    VERIFY_OK(m_gpu->CreateRasterizerState(&rasterDesc, m_debugWireframeState.GetAddressOf()));

    // Compile the shaders that render gradient color ramps.
    {
        std::ostringstream s;
        s << glsl::hlsl << '\n';
        s << glsl::common << '\n';
        s << glsl::color_ramp << '\n';
        ComPtr<ID3DBlob> vertexBlob = compile_source_to_blob(m_gpu.Get(),
                                                             GLSL_VERTEX,
                                                             s.str().c_str(),
                                                             GLSL_colorRampVertexMain,
                                                             "vs_5_0");
        ComPtr<ID3DBlob> pixelBlob = compile_source_to_blob(m_gpu.Get(),
                                                            GLSL_FRAGMENT,
                                                            s.str().c_str(),
                                                            GLSL_colorRampFragmentMain,
                                                            "ps_5_0");
        D3D11_INPUT_ELEMENT_DESC spanDesc =
            {GLSL_a_span, 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1};
        VERIFY_OK(m_gpu->CreateInputLayout(&spanDesc,
                                           1,
                                           vertexBlob->GetBufferPointer(),
                                           vertexBlob->GetBufferSize(),
                                           &m_colorRampLayout));
        VERIFY_OK(m_gpu->CreateVertexShader(vertexBlob->GetBufferPointer(),
                                            vertexBlob->GetBufferSize(),
                                            nullptr,
                                            &m_colorRampVertexShader));
        VERIFY_OK(m_gpu->CreatePixelShader(pixelBlob->GetBufferPointer(),
                                           pixelBlob->GetBufferSize(),
                                           nullptr,
                                           &m_colorRampPixelShader));
    }

    // Compile the tessellation shaders.
    {
        std::ostringstream s;
        s << glsl::hlsl << '\n';
        s << glsl::common << '\n';
        s << glsl::tessellate << '\n';
        ComPtr<ID3DBlob> vertexBlob = compile_source_to_blob(m_gpu.Get(),
                                                             GLSL_VERTEX,
                                                             s.str().c_str(),
                                                             GLSL_tessellateVertexMain,
                                                             "vs_5_0");
        ComPtr<ID3DBlob> pixelBlob = compile_source_to_blob(m_gpu.Get(),
                                                            GLSL_FRAGMENT,
                                                            s.str().c_str(),
                                                            GLSL_tessellateFragmentMain,
                                                            "ps_5_0");
        // Draw two instances per TessVertexSpan: one normal and one optional reflection.
        constexpr static UINT kTessAttribsStepRate = 2;
        D3D11_INPUT_ELEMENT_DESC attribsDesc[] = {{GLSL_a_p0p1_,
                                                   0,
                                                   DXGI_FORMAT_R32G32B32A32_FLOAT,
                                                   0,
                                                   D3D11_APPEND_ALIGNED_ELEMENT,
                                                   D3D11_INPUT_PER_INSTANCE_DATA,
                                                   kTessAttribsStepRate},
                                                  {GLSL_a_p2p3_,
                                                   0,
                                                   DXGI_FORMAT_R32G32B32A32_FLOAT,
                                                   0,
                                                   D3D11_APPEND_ALIGNED_ELEMENT,
                                                   D3D11_INPUT_PER_INSTANCE_DATA,
                                                   kTessAttribsStepRate},
                                                  {GLSL_a_joinTan_and_ys,
                                                   0,
                                                   DXGI_FORMAT_R32G32B32A32_FLOAT,
                                                   0,
                                                   D3D11_APPEND_ALIGNED_ELEMENT,
                                                   D3D11_INPUT_PER_INSTANCE_DATA,
                                                   kTessAttribsStepRate},
                                                  {GLSL_a_args,
                                                   0,
                                                   DXGI_FORMAT_R32G32B32A32_UINT,
                                                   0,
                                                   D3D11_APPEND_ALIGNED_ELEMENT,
                                                   D3D11_INPUT_PER_INSTANCE_DATA,
                                                   kTessAttribsStepRate}};
        VERIFY_OK(m_gpu->CreateInputLayout(attribsDesc,
                                           std::size(attribsDesc),
                                           vertexBlob->GetBufferPointer(),
                                           vertexBlob->GetBufferSize(),
                                           &m_tessellateLayout));
        VERIFY_OK(m_gpu->CreateVertexShader(vertexBlob->GetBufferPointer(),
                                            vertexBlob->GetBufferSize(),
                                            nullptr,
                                            &m_tessellateVertexShader));
        VERIFY_OK(m_gpu->CreatePixelShader(pixelBlob->GetBufferPointer(),
                                           pixelBlob->GetBufferSize(),
                                           nullptr,
                                           &m_tessellatePixelShader));
    }

    // Set up the path patch rendering buffers.
    PatchVertex patchVertices[kPatchVertexBufferCount];
    uint16_t patchIndices[kPatchIndexBufferCount];
    GeneratePatchBufferData(patchVertices, patchIndices);

    {
        D3D11_BUFFER_DESC vertexBufferDesc{};
        vertexBufferDesc.ByteWidth = sizeof(patchVertices);
        vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
        vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vertexBufferDesc.StructureByteStride = sizeof(PatchVertex);

        D3D11_SUBRESOURCE_DATA vertexDataDesc{};
        vertexDataDesc.pSysMem = patchVertices;

        VERIFY_OK(gpu->CreateBuffer(&vertexBufferDesc,
                                    &vertexDataDesc,
                                    m_patchVertexBuffer.GetAddressOf()));
    }

    {
        D3D11_BUFFER_DESC indexBufferDesc{};
        indexBufferDesc.ByteWidth = sizeof(patchIndices);
        indexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
        indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

        D3D11_SUBRESOURCE_DATA indexDataDesc{};
        indexDataDesc.pSysMem = patchIndices;

        VERIFY_OK(
            gpu->CreateBuffer(&indexBufferDesc, &indexDataDesc, m_patchIndexBuffer.GetAddressOf()));
    }

    // Create a buffer for the uniforms that get updated every draw.
    {
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = sizeof(PerDrawUniforms);
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.StructureByteStride = sizeof(PerDrawUniforms);
        VERIFY_OK(gpu->CreateBuffer(&desc, nullptr, m_perDrawUniforms.GetAddressOf()));
    }

    // Create a sampler for the gradient texture.
    D3D11_SAMPLER_DESC gradSamplerDesc;
    gradSamplerDesc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    gradSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    gradSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    gradSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    gradSamplerDesc.MipLODBias = 0.0f;
    gradSamplerDesc.MaxAnisotropy = 1;
    gradSamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    gradSamplerDesc.MinLOD = 0;
    gradSamplerDesc.MaxLOD = 0;
    VERIFY_OK(m_gpu->CreateSamplerState(&gradSamplerDesc, m_gradSampler.GetAddressOf()));
    m_gpuContext->PSSetSamplers(0, 1, m_gradSampler.GetAddressOf());
}

class BufferRingD3D : public BufferRingShadowImpl
{
public:
    BufferRingD3D(ID3D11Device* gpu,
                  ComPtr<ID3D11DeviceContext> gpuContext,
                  size_t capacity,
                  size_t itemSizeInBytes,
                  UINT bindFlags) :
        BufferRingShadowImpl(capacity, itemSizeInBytes), m_gpuContext(gpuContext)
    {
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = itemSizeInBytes * capacity;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = bindFlags;
        desc.CPUAccessFlags = 0;
        desc.StructureByteStride = itemSizeInBytes;

        for (size_t i = 0; i < kBufferRingSize; ++i)
        {
            VERIFY_OK(gpu->CreateBuffer(&desc, nullptr, m_buffers[i].GetAddressOf()));
        }
    }

    ID3D11Buffer* submittedBuffer() const { return m_buffers[submittedBufferIdx()].Get(); }

protected:
    void onUnmapAndSubmitBuffer(int bufferIdx, size_t bytesWritten)
    {
        if (bytesWritten == capacity() * itemSizeInBytes())
        {
            // Constant buffers don't allow partial updates, so special-case the event where we
            // update the entire buffer.
            m_gpuContext
                ->UpdateSubresource(m_buffers[bufferIdx].Get(), 0, NULL, shadowBuffer(), 0, 0);
        }
        else
        {
            D3D11_BOX box;
            box.left = 0;
            box.right = bytesWritten;
            box.top = 0;
            box.bottom = 1;
            box.front = 0;
            box.back = 1;
            m_gpuContext
                ->UpdateSubresource(m_buffers[bufferIdx].Get(), 0, &box, shadowBuffer(), 0, 0);
        }
    }

    ComPtr<ID3D11DeviceContext> m_gpuContext;
    ComPtr<ID3D11Buffer> m_buffers[kBufferRingSize];
};

std::unique_ptr<BufferRingImpl> PLSRenderContextD3D::makeVertexBufferRing(size_t capacity,
                                                                          size_t itemSizeInBytes)
{
    return std::make_unique<BufferRingD3D>(m_gpu.Get(),
                                           m_gpuContext,
                                           capacity,
                                           itemSizeInBytes,
                                           D3D11_BIND_VERTEX_BUFFER);
}

std::unique_ptr<BufferRingImpl> PLSRenderContextD3D::makePixelUnpackBufferRing(
    size_t capacity,
    size_t itemSizeInBytes)
{
    // It appears impossible to update a D3D texture from a GPU buffer.
    return std::make_unique<CPUOnlyBufferRing>(capacity, itemSizeInBytes);
}

std::unique_ptr<BufferRingImpl> PLSRenderContextD3D::makeUniformBufferRing(size_t itemSizeInBytes)
{
    return std::make_unique<BufferRingD3D>(m_gpu.Get(),
                                           m_gpuContext,
                                           1,
                                           itemSizeInBytes,
                                           D3D11_BIND_CONSTANT_BUFFER);
}

class TexelBufferD3D : public TexelBufferRing
{
public:
    TexelBufferD3D(ID3D11Device* gpu,
                   ComPtr<ID3D11DeviceContext> gpuContext,
                   Format format,
                   size_t widthInItems,
                   size_t height,
                   size_t texelsPerItem) :
        TexelBufferRing(format, widthInItems, height, texelsPerItem), m_gpuContext(gpuContext)
    {
        DXGI_FORMAT formatD3D = d3d_format(format);
        UINT bindFlags = D3D11_BIND_SHADER_RESOURCE;
        for (size_t i = 0; i < kBufferRingSize; ++i)
        {
            m_textures[i] =
                make_simple_2d_texture(gpu, formatD3D, widthInTexels(), height, bindFlags);
            m_srvs[i] = make_simple_2d_srv(gpu, m_textures[i].Get(), formatD3D);
        }
    }

    ID3D11ShaderResourceView* submittedSRV() const { return m_srvs[submittedBufferIdx()].Get(); }

private:
    void submitTexels(int textureIdx, size_t updateWidthInTexels, size_t updateHeight) override
    {
        D3D11_BOX box;
        box.left = 0;
        box.right = updateWidthInTexels;
        box.top = 0;
        box.bottom = updateHeight;
        box.front = 0;
        box.back = 1;
        m_gpuContext->UpdateSubresource(m_textures[textureIdx].Get(),
                                        0,
                                        &box,
                                        shadowBuffer(),
                                        widthInItems() * itemSizeInBytes(),
                                        0);
    }

    ComPtr<ID3D11DeviceContext> m_gpuContext;
    ComPtr<ID3D11Texture2D> m_textures[kBufferRingSize];
    ComPtr<ID3D11ShaderResourceView> m_srvs[kBufferRingSize];
};

std::unique_ptr<TexelBufferRing> PLSRenderContextD3D::makeTexelBufferRing(
    TexelBufferRing::Format format,
    size_t widthInItems,
    size_t height,
    size_t texelsPerItem,
    int textureIdx,
    TexelBufferRing::Filter)
{
    return std::make_unique<TexelBufferD3D>(m_gpu.Get(),
                                            m_gpuContext,
                                            format,
                                            widthInItems,
                                            height,
                                            texelsPerItem);
}

PLSRenderTargetD3D::PLSRenderTargetD3D(ID3D11Device* gpu, size_t width, size_t height) :
    PLSRenderTarget(width, height)
{
    m_coverageTexture = make_simple_2d_texture(gpu,
                                               DXGI_FORMAT_R32_UINT,
                                               width,
                                               height,
                                               D3D11_BIND_UNORDERED_ACCESS);
    m_originalDstColorTexture = make_simple_2d_texture(gpu,
                                                       DXGI_FORMAT_R8G8B8A8_UNORM,
                                                       width,
                                                       height,
                                                       D3D11_BIND_UNORDERED_ACCESS);
    m_clipTexture = make_simple_2d_texture(gpu,
                                           DXGI_FORMAT_R32_UINT,
                                           width,
                                           height,
                                           D3D11_BIND_UNORDERED_ACCESS);

    m_coverageUAV = make_simple_2d_uav(gpu, m_coverageTexture.Get(), DXGI_FORMAT_R32_UINT);
    m_originalDstColorUAV =
        make_simple_2d_uav(gpu, m_originalDstColorTexture.Get(), DXGI_FORMAT_R8G8B8A8_UNORM);
    m_clipUAV = make_simple_2d_uav(gpu, m_clipTexture.Get(), DXGI_FORMAT_R32_UINT);
}

void PLSRenderTargetD3D::setTargetTexture(ID3D11Device* gpu, ComPtr<ID3D11Texture2D> tex)
{
#ifdef DEBUG
    D3D11_TEXTURE2D_DESC desc;
    tex->GetDesc(&desc);
    assert(desc.Width == width());
    assert(desc.Height == height());
    assert(desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM);
#endif
    m_targetTexture = std::move(tex);
    m_targetUAV = make_simple_2d_uav(gpu, m_targetTexture.Get(), DXGI_FORMAT_R8G8B8A8_UNORM);
}

rcp<PLSRenderTargetD3D> PLSRenderContextD3D::makeRenderTarget(size_t width, size_t height)
{
    return rcp(new PLSRenderTargetD3D(m_gpu.Get(), width, height));
}

void PLSRenderContextD3D::allocateGradientTexture(size_t height)
{
    m_gradTexture = make_simple_2d_texture(m_gpu.Get(),
                                           DXGI_FORMAT_R8G8B8A8_UNORM,
                                           kGradTextureWidth,
                                           height,
                                           D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
    m_gradTextureSRV =
        make_simple_2d_srv(m_gpu.Get(), m_gradTexture.Get(), DXGI_FORMAT_R8G8B8A8_UNORM);
    m_gradTextureRTV =
        make_simple_2d_rtv(m_gpu.Get(), m_gradTexture.Get(), DXGI_FORMAT_R8G8B8A8_UNORM);
}

void PLSRenderContextD3D::allocateTessellationTexture(size_t height)
{
    m_tessTexture = make_simple_2d_texture(m_gpu.Get(),
                                           DXGI_FORMAT_R32G32B32A32_UINT,
                                           kTessTextureWidth,
                                           height,
                                           D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
    m_tessTextureSRV =
        make_simple_2d_srv(m_gpu.Get(), m_tessTexture.Get(), DXGI_FORMAT_R32G32B32A32_UINT);
    m_tessTextureRTV =
        make_simple_2d_rtv(m_gpu.Get(), m_tessTexture.Get(), DXGI_FORMAT_R32G32B32A32_UINT);
}

void PLSRenderContextD3D::setPipelineLayoutAndShaders(DrawType drawType,
                                                      const ShaderFeatures& shaderFeatures)
{
    uint32_t vertexShaderKey = ShaderUniqueKey(SourceType::vertexOnly, drawType, shaderFeatures);
    auto vertexEntry = m_drawVertexShaders.find(vertexShaderKey);

    uint32_t pixelShaderKey = ShaderUniqueKey(SourceType::wholeProgram, drawType, shaderFeatures);
    auto pixelEntry = m_drawPixelShaders.find(pixelShaderKey);

    if (vertexEntry == m_drawVertexShaders.end() || pixelEntry == m_drawPixelShaders.end())
    {
        std::ostringstream s;
        uint64_t shaderFeatureDefines =
            shaderFeatures.getPreprocessorDefines(SourceType::wholeProgram);
        if (drawType == DrawType::interiorTriangulation)
        {
            s << "#define " << GLSL_DRAW_INTERIOR_TRIANGLES << '\n';
        }
        if (shaderFeatureDefines & ShaderFeatures::PreprocessorDefines::ENABLE_ADVANCED_BLEND)
        {
            s << "#define " << GLSL_ENABLE_ADVANCED_BLEND << '\n';
        }
        if (shaderFeatureDefines & ShaderFeatures::PreprocessorDefines::ENABLE_PATH_CLIPPING)
        {
            s << "#define " << GLSL_ENABLE_PATH_CLIPPING << '\n';
        }
        if (shaderFeatureDefines & ShaderFeatures::PreprocessorDefines::ENABLE_EVEN_ODD)
        {
            s << "#define " << GLSL_ENABLE_EVEN_ODD << '\n';
        }
        if (shaderFeatureDefines & ShaderFeatures::PreprocessorDefines::ENABLE_HSL_BLEND_MODES)
        {
            s << "#define " << GLSL_ENABLE_HSL_BLEND_MODES << '\n';
        }
        s << "#define " << GLSL_ENABLE_BASE_INSTANCE_POLYFILL << '\n';
        s << glsl::hlsl << '\n';
        s << glsl::common << '\n';
        if (shaderFeatures.programFeatures.blendTier != BlendTier::srcOver)
        {
            s << glsl::advanced_blend << '\n';
        }
        s << glsl::draw << '\n';

        const std::string shader = s.str();

        if (vertexEntry == m_drawVertexShaders.end())
        {
            DrawVertexShader drawVertexShader;
            ComPtr<ID3DBlob> blob = compile_source_to_blob(m_gpu.Get(),
                                                           GLSL_VERTEX,
                                                           shader.c_str(),
                                                           GLSL_drawVertexMain,
                                                           "vs_5_0");
            D3D11_INPUT_ELEMENT_DESC layoutDesc[2];
            size_t vertexAttribCount;
            switch (drawType)
            {
                case DrawType::midpointFanPatches:
                case DrawType::outerCurvePatches:
                    layoutDesc[0] = {GLSL_a_patchVertexData,
                                     0,
                                     DXGI_FORMAT_R32G32B32A32_FLOAT,
                                     kPatchVertexDataSlot,
                                     D3D11_APPEND_ALIGNED_ELEMENT,
                                     D3D11_INPUT_PER_VERTEX_DATA,
                                     0};
                    layoutDesc[1] = {GLSL_a_mirroredVertexData,
                                     0,
                                     DXGI_FORMAT_R32G32B32A32_FLOAT,
                                     kPatchVertexDataSlot,
                                     D3D11_APPEND_ALIGNED_ELEMENT,
                                     D3D11_INPUT_PER_VERTEX_DATA,
                                     0};
                    vertexAttribCount = 2;
                    break;
                case DrawType::interiorTriangulation:
                    layoutDesc[0] = {GLSL_a_triangleVertex,
                                     0,
                                     DXGI_FORMAT_R32G32B32_FLOAT,
                                     kTriangleVertexDataSlot,
                                     0,
                                     D3D11_INPUT_PER_VERTEX_DATA,
                                     0};
                    vertexAttribCount = 1;
                    break;
            }
            VERIFY_OK(m_gpu->CreateInputLayout(layoutDesc,
                                               vertexAttribCount,
                                               blob->GetBufferPointer(),
                                               blob->GetBufferSize(),
                                               &drawVertexShader.layout));
            VERIFY_OK(m_gpu->CreateVertexShader(blob->GetBufferPointer(),
                                                blob->GetBufferSize(),
                                                nullptr,
                                                &drawVertexShader.shader));
            vertexEntry = m_drawVertexShaders.insert({vertexShaderKey, drawVertexShader}).first;
        }

        if (pixelEntry == m_drawPixelShaders.end())
        {
            ComPtr<ID3D11PixelShader> pixelShader;
            ComPtr<ID3DBlob> blob = compile_source_to_blob(m_gpu.Get(),
                                                           GLSL_FRAGMENT,
                                                           shader.c_str(),
                                                           GLSL_drawFragmentMain,
                                                           "ps_5_0");
            VERIFY_OK(m_gpu->CreatePixelShader(blob->GetBufferPointer(),
                                               blob->GetBufferSize(),
                                               nullptr,
                                               &pixelShader));
            pixelEntry = m_drawPixelShaders.insert({pixelShaderKey, pixelShader}).first;
        }
    }

    m_gpuContext->IASetInputLayout(vertexEntry->second.layout.Get());
    m_gpuContext->VSSetShader(vertexEntry->second.shader.Get(), NULL, 0);
    m_gpuContext->PSSetShader(pixelEntry->second.Get(), NULL, 0);
}

static ID3D11Buffer* submitted_buffer(const BufferRingImpl* bufferRing)
{
    return static_cast<const BufferRingD3D*>(bufferRing)->submittedBuffer();
}

static ID3D11ShaderResourceView* submitted_srv(const TexelBufferRing* texelBufferRing)
{
    return static_cast<const TexelBufferD3D*>(texelBufferRing)->submittedSRV();
}

void PLSRenderContextD3D::onFlush(const FlushDescriptor& desc)
{
    auto renderTarget =
        static_cast<const PLSRenderTargetD3D*>(frameDescriptor().renderTarget.get());

    constexpr static UINT kZero[4]{};
    if (desc.loadAction == LoadAction::clear)
    {
        float clearColor4f[4];
        UnpackColorToRGBA32F(frameDescriptor().clearColor, clearColor4f);
        m_gpuContext->ClearUnorderedAccessViewFloat(renderTarget->m_targetUAV.Get(), clearColor4f);
    }
    m_gpuContext->ClearUnorderedAccessViewUint(renderTarget->m_coverageUAV.Get(), kZero);
    if (desc.needsClipBuffer)
    {
        m_gpuContext->ClearUnorderedAccessViewUint(renderTarget->m_clipUAV.Get(), kZero);
    }

    ID3D11Buffer* cbuffers[] = {submitted_buffer(uniformBufferRing()), m_perDrawUniforms.Get()};
    m_gpuContext->VSSetConstantBuffers(0, std::size(cbuffers), cbuffers);

    // Render the complex color ramps to the gradient texture.
    if (desc.complexGradSpanCount > 0)
    {
        ID3D11Buffer* gradSpanBuffer = submitted_buffer(gradSpanBufferRing());
        UINT gradStride = sizeof(GradientSpan);
        UINT gradOffset = 0;
        m_gpuContext->IASetVertexBuffers(0, 1, &gradSpanBuffer, &gradStride, &gradOffset);
        m_gpuContext->IASetInputLayout(m_colorRampLayout.Get());
        m_gpuContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

        m_gpuContext->VSSetShader(m_colorRampVertexShader.Get(), NULL, 0);

        D3D11_VIEWPORT viewport = {0,
                                   static_cast<float>(desc.complexGradRowsTop),
                                   static_cast<float>(kGradTextureWidth),
                                   static_cast<float>(desc.complexGradRowsHeight),
                                   0,
                                   1};
        m_gpuContext->RSSetViewports(1, &viewport);

        m_gpuContext->PSSetShaderResources(0, 0, NULL);
        m_gpuContext->PSSetShader(m_colorRampPixelShader.Get(), NULL, 0);

        m_gpuContext->OMSetRenderTargets(1, m_gradTextureRTV.GetAddressOf(), NULL);

        m_gpuContext->DrawInstanced(4, desc.complexGradSpanCount, 0, 0);
    }

    // Copy the simple color ramps to the gradient texture.
    if (desc.simpleGradTexelsHeight > 0)
    {
        D3D11_BOX box;
        box.left = 0;
        box.right = desc.simpleGradTexelsWidth;
        box.top = 0;
        box.bottom = desc.simpleGradTexelsHeight;
        box.front = 0;
        box.back = 1;
        const void* data =
            static_cast<const CPUOnlyBufferRing*>(simpleColorRampsBufferRing())->submittedata();
        m_gpuContext
            ->UpdateSubresource(m_gradTexture.Get(), 0, &box, data, kGradTextureWidth * 4, 0);
    }

    // Tessellate all curves into vertices in the tessellation texture.
    if (desc.tessVertexSpanCount > 0)
    {
        ID3D11Buffer* tessSpanBuffer = submitted_buffer(tessSpanBufferRing());
        UINT tessStride = sizeof(TessVertexSpan);
        UINT tessOffset = 0;
        m_gpuContext->IASetVertexBuffers(0, 1, &tessSpanBuffer, &tessStride, &tessOffset);
        m_gpuContext->IASetInputLayout(m_tessellateLayout.Get());
        m_gpuContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

        m_gpuContext->VSSetShader(m_tessellateVertexShader.Get(), NULL, 0);
        ID3D11ShaderResourceView* vsTextureViews[] = {NULL,
                                                      submitted_srv(pathBufferRing()),
                                                      submitted_srv(contourBufferRing())};
        static_assert(kTessVertexTextureIdx == 0);
        static_assert(kPathTextureIdx == 1);
        static_assert(kContourTextureIdx == 2);
        m_gpuContext->VSSetShaderResources(0, std::size(vsTextureViews), vsTextureViews);

        D3D11_VIEWPORT viewport = {0,
                                   0,
                                   static_cast<float>(kTessTextureWidth),
                                   static_cast<float>(desc.tessDataHeight),
                                   0,
                                   1};
        m_gpuContext->RSSetViewports(1, &viewport);

        m_gpuContext->PSSetShaderResources(0, 0, NULL);
        m_gpuContext->PSSetShader(m_tessellatePixelShader.Get(), NULL, 0);

        m_gpuContext->OMSetRenderTargets(1, m_tessTextureRTV.GetAddressOf(), NULL);

        // Draw two instances per TessVertexSpan: one normal and one optional reflection.
        m_gpuContext->DrawInstanced(4, desc.tessVertexSpanCount * 2, 0, 0);

        if (m_isIntel)
        {
            // FIXME! Intel needs this flush! Driver bug? Find a lighter workaround?
            m_gpuContext->Flush();
        }
    }

    // Execute the DrawList.
    ID3D11UnorderedAccessView* plsUAVs[] = {renderTarget->m_targetUAV.Get(),
                                            renderTarget->m_coverageUAV.Get(),
                                            renderTarget->m_originalDstColorUAV.Get(),
                                            renderTarget->m_clipUAV.Get()};
    static_assert(kFramebufferPlaneIdx == 0);
    static_assert(kCoveragePlaneIdx == 1);
    static_assert(kOriginalDstColorPlaneIdx == 2);
    static_assert(kClipPlaneIdx == 3);
    m_gpuContext->OMSetRenderTargetsAndUnorderedAccessViews(0,
                                                            NULL,
                                                            NULL,
                                                            0,
                                                            std::size(plsUAVs),
                                                            plsUAVs,
                                                            NULL);

    m_gpuContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_gpuContext->IASetIndexBuffer(m_patchIndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);

    ID3D11Buffer* vertexBuffers[] = {m_patchVertexBuffer.Get(),
                                     submitted_buffer(triangleBufferRing())};
    static_assert(kPatchVertexDataSlot == 0);
    static_assert(kTriangleVertexDataSlot == 1);
    UINT vertexStrides[] = {sizeof(PatchVertex), sizeof(TriangleVertex)};
    UINT vertexOffsets[] = {0, 0};
    m_gpuContext->IASetVertexBuffers(0, 2, vertexBuffers, vertexStrides, vertexOffsets);

    ID3D11ShaderResourceView* vertexTextureViews[] = {m_tessTextureSRV.Get(),
                                                      submitted_srv(pathBufferRing()),
                                                      submitted_srv(contourBufferRing())};
    static_assert(kTessVertexTextureIdx == 0);
    static_assert(kPathTextureIdx == 1);
    static_assert(kContourTextureIdx == 2);
    m_gpuContext->VSSetShaderResources(0, std::size(vertexTextureViews), vertexTextureViews);

    D3D11_VIEWPORT viewport = {0,
                               0,
                               static_cast<float>(renderTarget->width()),
                               static_cast<float>(renderTarget->height()),
                               0,
                               1};
    m_gpuContext->RSSetViewports(1, &viewport);
    if (frameDescriptor().wireframe)
    {
        m_gpuContext->RSSetState(m_debugWireframeState.Get());
    }

    m_gpuContext->PSSetShaderResources(kGradTextureIdx, 1, m_gradTextureSRV.GetAddressOf());

    for (const Draw& draw : m_drawList)
    {
        if (draw.vertexOrInstanceCount == 0)
        {
            continue;
        }

        DrawType drawType = draw.drawType;
        setPipelineLayoutAndShaders(drawType, draw.shaderFeatures);

        switch (drawType)
        {
            case DrawType::midpointFanPatches:
            case DrawType::outerCurvePatches:
            {
                PerDrawUniforms uniforms(draw.baseVertexOrInstance);
                m_gpuContext->UpdateSubresource(m_perDrawUniforms.Get(), 0, NULL, &uniforms, 0, 0);
                m_gpuContext->DrawIndexedInstanced(PatchIndexCount(drawType),
                                                   draw.vertexOrInstanceCount,
                                                   PatchBaseIndex(drawType),
                                                   0,
                                                   draw.baseVertexOrInstance);
                break;
            }
            case DrawType::interiorTriangulation:
                m_gpuContext->Draw(draw.vertexOrInstanceCount, draw.baseVertexOrInstance);
                break;
        }
    }

    if (frameDescriptor().wireframe)
    {
        m_gpuContext->RSSetState(m_rasterState.Get());
    }
}
} // namespace rive::pls
