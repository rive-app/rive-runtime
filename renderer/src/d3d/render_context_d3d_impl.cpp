/*
 * Copyright 2023 Rive
 */

#include "rive/renderer/d3d/render_context_d3d_impl.hpp"

#include "rive/renderer/image.hpp"
#include "shaders/constants.glsl"

#include <D3DCompiler.h>
#include <sstream>

#include "generated/shaders/advanced_blend.glsl.hpp"
#include "generated/shaders/atomic_draw.glsl.hpp"
#include "generated/shaders/color_ramp.glsl.hpp"
#include "generated/shaders/constants.glsl.hpp"
#include "generated/shaders/common.glsl.hpp"
#include "generated/shaders/draw_image_mesh.glsl.hpp"
#include "generated/shaders/draw_path_common.glsl.hpp"
#include "generated/shaders/draw_path.glsl.hpp"
#include "generated/shaders/hlsl.glsl.hpp"
#include "generated/shaders/tessellate.glsl.hpp"

// D3D11 doesn't let us bind the framebuffer UAV to slot 0 when there is a color output. Use the
// (unused in this case) SCRATCH_COLOR_PLANE_IDX instead when we are doing a coalesced resolve
// and transfer.
#define COALESCED_OFFSCREEN_COLOR_PLANE_IDX SCRATCH_COLOR_PLANE_IDX

constexpr static UINT kPatchVertexDataSlot = 0;
constexpr static UINT kTriangleVertexDataSlot = 1;
constexpr static UINT kImageRectVertexDataSlot = 2;
constexpr static UINT kImageMeshVertexDataSlot = 3;
constexpr static UINT kImageMeshUVDataSlot = 4;

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

static ComPtr<ID3D11UnorderedAccessView> make_simple_2d_uav(ID3D11Device* gpu,
                                                            ID3D11Texture2D* tex,
                                                            DXGI_FORMAT format)
{
    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.Format = format;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;

    ComPtr<ID3D11UnorderedAccessView> uav;
    VERIFY_OK(gpu->CreateUnorderedAccessView(tex, &uavDesc, uav.ReleaseAndGetAddressOf()));
    return uav;
}

std::unique_ptr<PLSRenderContext> PLSRenderContextD3DImpl::MakeContext(
    ComPtr<ID3D11Device> gpu,
    ComPtr<ID3D11DeviceContext> gpuContext,
    const ContextOptions& contextOptions)
{
    D3DCapabilities d3dCapabilities;
    D3D11_FEATURE_DATA_D3D11_OPTIONS2 d3d11Options2;

    if (gpu->GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_1)
    {
        if (SUCCEEDED(gpu->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS2,
                                               &d3d11Options2,
                                               sizeof(D3D11_FEATURE_DATA_D3D11_OPTIONS2))))
        {
            d3dCapabilities.supportsRasterizerOrderedViews = d3d11Options2.ROVsSupported;
            if (d3d11Options2.TypedUAVLoadAdditionalFormats)
            {
                // TypedUAVLoadAdditionalFormats is true. Now check if we can both load and
                // store all formats used by Rive (currently only RGBA8):
                // https://learn.microsoft.com/en-us/windows/win32/direct3d11/typed-unordered-access-view-loads.
                D3D11_FEATURE_DATA_FORMAT_SUPPORT2 d3d11Format2{};
                d3d11Format2.InFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
                if (SUCCEEDED(gpu->CheckFeatureSupport(D3D11_FEATURE_FORMAT_SUPPORT2,
                                                       &d3d11Format2,
                                                       sizeof(d3d11Format2))))
                {
                    constexpr UINT loadStoreFlags = D3D11_FORMAT_SUPPORT2_UAV_TYPED_LOAD |
                                                    D3D11_FORMAT_SUPPORT2_UAV_TYPED_STORE;
                    d3dCapabilities.supportsTypedUAVLoadStore =
                        (d3d11Format2.OutFormatSupport2 & loadStoreFlags) == loadStoreFlags;
                }
            }
        }

        // Check if we can use HLSL minimum precision types (e.g. min16int)
        D3D11_FEATURE_DATA_SHADER_MIN_PRECISION_SUPPORT d3d11MinPrecisionSupport;
        if (SUCCEEDED(gpu->CheckFeatureSupport(D3D11_FEATURE_SHADER_MIN_PRECISION_SUPPORT,
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

    auto plsContextImpl = std::unique_ptr<PLSRenderContextD3DImpl>(
        new PLSRenderContextD3DImpl(std::move(gpu), std::move(gpuContext), d3dCapabilities));
    return std::make_unique<PLSRenderContext>(std::move(plsContextImpl));
}

PLSRenderContextD3DImpl::PLSRenderContextD3DImpl(ComPtr<ID3D11Device> gpu,
                                                 ComPtr<ID3D11DeviceContext> gpuContext,
                                                 const D3DCapabilities& d3dCapabilities) :
    m_d3dCapabilities(d3dCapabilities), m_gpu(std::move(gpu)), m_gpuContext(std::move(gpuContext))
{
    m_platformFeatures.invertOffscreenY = true;
    m_platformFeatures.supportsRasterOrdering = d3dCapabilities.supportsRasterizerOrderedViews;

    // Create a default raster state for path and offscreen draws.
    D3D11_RASTERIZER_DESC rasterDesc;
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.CullMode = D3D11_CULL_BACK;
    rasterDesc.FrontCounterClockwise = FALSE; // FrontCounterClockwise must be FALSE in order to
                                              // match the winding sense of interior triangulations.

    rasterDesc.DepthBias = 0;
    rasterDesc.SlopeScaledDepthBias = 0;
    rasterDesc.DepthBiasClamp = 0;
    rasterDesc.DepthClipEnable = TRUE; // This is the default state which reset to before flushing.
                                       // Details on default state here:
    // https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_rasterizer_desc

    rasterDesc.ScissorEnable = FALSE;
    rasterDesc.MultisampleEnable = FALSE;
    rasterDesc.AntialiasedLineEnable = FALSE;
    VERIFY_OK(m_gpu->CreateRasterizerState(&rasterDesc,
                                           m_backCulledRasterState[0].ReleaseAndGetAddressOf()));

    // ...And with wireframe for debugging.
    rasterDesc.FillMode = D3D11_FILL_WIREFRAME;
    VERIFY_OK(m_gpu->CreateRasterizerState(&rasterDesc,
                                           m_backCulledRasterState[1].ReleaseAndGetAddressOf()));

    // Create a raster state without face culling for drawing image meshes.
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.CullMode = D3D11_CULL_NONE;
    VERIFY_OK(m_gpu->CreateRasterizerState(&rasterDesc,
                                           m_doubleSidedRasterState[0].ReleaseAndGetAddressOf()));

    // ...And once more with wireframe for debugging.
    rasterDesc.FillMode = D3D11_FILL_WIREFRAME;
    VERIFY_OK(m_gpu->CreateRasterizerState(&rasterDesc,
                                           m_doubleSidedRasterState[1].ReleaseAndGetAddressOf()));

    // Compile the shaders that render gradient color ramps.
    {
        std::ostringstream s;
        s << glsl::hlsl << '\n';
        s << glsl::constants << '\n';
        s << glsl::common << '\n';
        s << glsl::color_ramp << '\n';
        ComPtr<ID3DBlob> vertexBlob =
            compileSourceToBlob(GLSL_VERTEX, s.str().c_str(), GLSL_colorRampVertexMain, "vs_5_0");
        ComPtr<ID3DBlob> pixelBlob = compileSourceToBlob(GLSL_FRAGMENT,
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
        s << glsl::constants << '\n';
        s << glsl::common << '\n';
        s << glsl::tessellate << '\n';
        ComPtr<ID3DBlob> vertexBlob =
            compileSourceToBlob(GLSL_VERTEX, s.str().c_str(), GLSL_tessellateVertexMain, "vs_5_0");
        ComPtr<ID3DBlob> pixelBlob = compileSourceToBlob(GLSL_FRAGMENT,
                                                         s.str().c_str(),
                                                         GLSL_tessellateFragmentMain,
                                                         "ps_5_0");
        // Draw two instances per TessVertexSpan: one normal and one optional reflection.
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

        m_tessSpanIndexBuffer = makeSimpleImmutableBuffer(sizeof(gpu::kTessSpanIndices),
                                                          D3D11_BIND_INDEX_BUFFER,
                                                          gpu::kTessSpanIndices);
    }

    // Set up the path patch rendering buffers.
    PatchVertex patchVertices[kPatchVertexBufferCount];
    uint16_t patchIndices[kPatchIndexBufferCount];
    GeneratePatchBufferData(patchVertices, patchIndices);
    m_patchVertexBuffer =
        makeSimpleImmutableBuffer(sizeof(patchVertices), D3D11_BIND_VERTEX_BUFFER, patchVertices);
    m_patchIndexBuffer =
        makeSimpleImmutableBuffer(sizeof(patchIndices), D3D11_BIND_INDEX_BUFFER, patchIndices);

    // Set up the imageRect rendering buffers. (gpu::InterlockMode::atomics only.)
    m_imageRectVertexBuffer = makeSimpleImmutableBuffer(sizeof(gpu::kImageRectVertices),
                                                        D3D11_BIND_VERTEX_BUFFER,
                                                        gpu::kImageRectVertices);
    m_imageRectIndexBuffer = makeSimpleImmutableBuffer(sizeof(gpu::kImageRectIndices),
                                                       D3D11_BIND_INDEX_BUFFER,
                                                       gpu::kImageRectIndices);

    // Create buffers for uniforms.
    {
        D3D11_BUFFER_DESC desc{};
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

        desc.ByteWidth = sizeof(gpu::FlushUniforms);
        desc.StructureByteStride = sizeof(gpu::FlushUniforms);
        VERIFY_OK(m_gpu->CreateBuffer(&desc, nullptr, m_flushUniforms.ReleaseAndGetAddressOf()));

        desc.ByteWidth = sizeof(DrawUniforms);
        desc.StructureByteStride = sizeof(DrawUniforms);
        VERIFY_OK(m_gpu->CreateBuffer(&desc, nullptr, m_drawUniforms.ReleaseAndGetAddressOf()));

        desc.ByteWidth = sizeof(gpu::ImageDrawUniforms);
        desc.StructureByteStride = sizeof(gpu::ImageDrawUniforms);
        VERIFY_OK(
            m_gpu->CreateBuffer(&desc, nullptr, m_imageDrawUniforms.ReleaseAndGetAddressOf()));
    }

    // Create a linear sampler for the gradient texture.
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
        m_gpu->CreateSamplerState(&linearSamplerDesc, m_linearSampler.ReleaseAndGetAddressOf()));

    // Create a mipmap sampler for the image textures.
    D3D11_SAMPLER_DESC mipmapSamplerDesc;
    mipmapSamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    mipmapSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    mipmapSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    mipmapSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    mipmapSamplerDesc.MipLODBias = 0.0f;
    mipmapSamplerDesc.MaxAnisotropy = 1;
    mipmapSamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    mipmapSamplerDesc.MinLOD = 0;
    mipmapSamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    VERIFY_OK(
        m_gpu->CreateSamplerState(&mipmapSamplerDesc, m_mipmapSampler.ReleaseAndGetAddressOf()));

    ID3D11SamplerState* samplers[2] = {m_linearSampler.Get(), m_mipmapSampler.Get()};
    static_assert(IMAGE_TEXTURE_IDX == GRAD_TEXTURE_IDX + 1);
    m_gpuContext->PSSetSamplers(GRAD_TEXTURE_IDX, 2, samplers);

    D3D11_BLEND_DESC srcOverDesc{};
    srcOverDesc.RenderTarget[0].BlendEnable = TRUE;
    srcOverDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    srcOverDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    srcOverDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    srcOverDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    srcOverDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    srcOverDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    srcOverDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    VERIFY_OK(m_gpu->CreateBlendState(&srcOverDesc, m_srcOverBlendState.ReleaseAndGetAddressOf()));
}

ComPtr<ID3D11Texture2D> PLSRenderContextD3DImpl::makeSimple2DTexture(DXGI_FORMAT format,
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

ComPtr<ID3D11UnorderedAccessView> PLSRenderContextD3DImpl::makeSimple2DUAV(ID3D11Texture2D* tex,
                                                                           DXGI_FORMAT format)
{
    return make_simple_2d_uav(m_gpu.Get(), tex, format);
}

ComPtr<ID3D11Buffer> PLSRenderContextD3DImpl::makeSimpleImmutableBuffer(size_t sizeInBytes,
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
    VERIFY_OK(m_gpu->CreateBuffer(&desc, &dataDesc, buffer.ReleaseAndGetAddressOf()));
    return buffer;
}

ComPtr<ID3DBlob> PLSRenderContextD3DImpl::compileSourceToBlob(const char* shaderTypeDefineName,
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
        abort();
    }
    if (FAILED(hr))
    {
        fprintf(stderr, "Failed to compile shader.\n");
        abort();
    }
    return blob;
}

class RenderBufferD3DImpl : public lite_rtti_override<RenderBuffer, RenderBufferD3DImpl>
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
        m_desc.BindFlags =
            type() == RenderBufferType::vertex ? D3D11_BIND_VERTEX_BUFFER : D3D11_BIND_INDEX_BUFFER;
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
            VERIFY_OK(m_gpu->CreateBuffer(&m_desc, nullptr, m_buffer.ReleaseAndGetAddressOf()));
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
            m_gpuContext->Map(m_buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubresource);
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
            VERIFY_OK(
                m_gpu->CreateBuffer(&m_desc, &bufferDataDesc, m_buffer.ReleaseAndGetAddressOf()));
            m_mappedMemoryForImmutableBuffer.reset(); // This buffer will only be mapped once.
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

rcp<RenderBuffer> PLSRenderContextD3DImpl::makeRenderBuffer(RenderBufferType type,
                                                            RenderBufferFlags flags,
                                                            size_t sizeInBytes)
{
    return make_rcp<RenderBufferD3DImpl>(type, flags, sizeInBytes, m_gpu, m_gpuContext);
}

class PLSTextureD3DImpl : public PLSTexture
{
public:
    PLSTextureD3DImpl(PLSRenderContextD3DImpl* plsImpl,
                      UINT width,
                      UINT height,
                      UINT mipLevelCount,
                      const uint8_t imageDataRGBA[]) :
        PLSTexture(width, height)
    {
        m_texture =
            plsImpl->makeSimple2DTexture(DXGI_FORMAT_R8G8B8A8_UNORM,
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
        plsImpl->gpuContext()
            ->UpdateSubresource(m_texture.Get(), 0, &box, imageDataRGBA, width * 4, 0);

        // Create a view and generate mipmaps.
        VERIFY_OK(plsImpl->gpu()->CreateShaderResourceView(m_texture.Get(),
                                                           NULL,
                                                           m_srv.ReleaseAndGetAddressOf()));
        plsImpl->gpuContext()->GenerateMips(m_srv.Get());
    }

    ID3D11ShaderResourceView* srv() const { return m_srv.Get(); }
    ID3D11ShaderResourceView* const* srvAddressOf() const { return m_srv.GetAddressOf(); }

private:
    ComPtr<ID3D11Texture2D> m_texture;
    ComPtr<ID3D11ShaderResourceView> m_srv;
};

rcp<PLSTexture> PLSRenderContextD3DImpl::makeImageTexture(uint32_t width,
                                                          uint32_t height,
                                                          uint32_t mipLevelCount,
                                                          const uint8_t imageDataRGBA[])
{
    return make_rcp<PLSTextureD3DImpl>(this, width, height, mipLevelCount, imageDataRGBA);
}

class BufferRingD3D : public BufferRing
{
public:
    BufferRingD3D(PLSRenderContextD3DImpl* plsImpl, size_t capacityInBytes, UINT bindFlags) :
        BufferRingD3D(plsImpl, capacityInBytes, bindFlags, 0, 0)
    {}

    ID3D11Buffer* submittedBuffer() const { return m_buffers[submittedBufferIdx()].Get(); }

protected:
    BufferRingD3D(PLSRenderContextD3DImpl* plsImpl,
                  size_t capacityInBytes,
                  UINT bindFlags,
                  UINT elementSizeInBytes,
                  UINT miscFlags) :
        BufferRing(capacityInBytes), m_gpuContext(plsImpl->gpuContext())
    {
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = math::lossless_numeric_cast<UINT>(capacityInBytes);
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = bindFlags;
        desc.CPUAccessFlags = 0;
        desc.StructureByteStride = elementSizeInBytes;
        desc.MiscFlags = miscFlags;

        for (size_t i = 0; i < kBufferRingSize; ++i)
        {
            VERIFY_OK(plsImpl->gpu()->CreateBuffer(&desc,
                                                   nullptr,
                                                   m_buffers[i].ReleaseAndGetAddressOf()));
        }
    }

    void* onMapBuffer(int bufferIdx, size_t mapSizeInBytes) override
    {
        // Use a CPU-side shadow buffer since D3D11 doesn't have an API to map a sub-range.
        return shadowBuffer();
    }

    void onUnmapAndSubmitBuffer(int bufferIdx, size_t mapSizeInBytes) override
    {
        if (mapSizeInBytes == capacityInBytes())
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
            box.right = math::lossless_numeric_cast<UINT>(mapSizeInBytes);
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

class StructuredBufferRingD3D : public BufferRingD3D
{
public:
    StructuredBufferRingD3D(PLSRenderContextD3DImpl* plsImpl,
                            size_t capacityInBytes,
                            UINT elementSizeInBytes) :
        BufferRingD3D(plsImpl,
                      capacityInBytes,
                      D3D11_BIND_SHADER_RESOURCE,
                      elementSizeInBytes,
                      D3D11_RESOURCE_MISC_BUFFER_STRUCTURED)
    {
        assert(capacityInBytes % elementSizeInBytes == 0);
    }

    ID3D11ShaderResourceView* replaceSRV(ID3D11Device* gpu,
                                         UINT elementCount,
                                         UINT firstElement) const
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        srvDesc.Buffer.FirstElement = firstElement;
        srvDesc.Buffer.NumElements = elementCount;

        VERIFY_OK(gpu->CreateShaderResourceView(m_buffers[submittedBufferIdx()].Get(),
                                                &srvDesc,
                                                m_currentSRV.ReleaseAndGetAddressOf()));
        return m_currentSRV.Get();
    }

protected:
    mutable ComPtr<ID3D11ShaderResourceView> m_currentSRV;
};

std::unique_ptr<BufferRing> PLSRenderContextD3DImpl::makeUniformBufferRing(size_t capacityInBytes)
{
    // In D3D we update uniform data inline with commands, rather than filling a buffer up front.
    return std::make_unique<HeapBufferRing>(capacityInBytes);
}

std::unique_ptr<BufferRing> PLSRenderContextD3DImpl::makeStorageBufferRing(
    size_t capacityInBytes,
    gpu::StorageBufferStructure bufferStructure)
{
    return capacityInBytes != 0 ? std::make_unique<StructuredBufferRingD3D>(
                                      this,
                                      capacityInBytes,
                                      gpu::StorageBufferElementSizeInBytes(bufferStructure))
                                : nullptr;
}

std::unique_ptr<BufferRing> PLSRenderContextD3DImpl::makeVertexBufferRing(size_t capacityInBytes)
{
    return capacityInBytes != 0
               ? std::make_unique<BufferRingD3D>(this, capacityInBytes, D3D11_BIND_VERTEX_BUFFER)
               : nullptr;
}

std::unique_ptr<BufferRing> PLSRenderContextD3DImpl::makeTextureTransferBufferRing(
    size_t capacityInBytes)
{
    // It appears impossible to update a D3D texture from a GPU buffer; store this data on the heap
    // and upload it to the texture at flush time.
    return std::make_unique<HeapBufferRing>(capacityInBytes);
}

PLSRenderTargetD3D::PLSRenderTargetD3D(PLSRenderContextD3DImpl* plsImpl,
                                       uint32_t width,
                                       uint32_t height) :
    PLSRenderTarget(width, height),
    m_gpu(plsImpl->gpu()),
    m_gpuSupportsTypedUAVLoadStore(plsImpl->d3dCapabilities().supportsTypedUAVLoadStore)
{}

void PLSRenderTargetD3D::setTargetTexture(ComPtr<ID3D11Texture2D> tex)
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
               desc.Format == DXGI_FORMAT_R8G8B8A8_TYPELESS);
#endif
        m_targetTextureSupportsUAV =
            (desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS) &&
            (m_gpuSupportsTypedUAVLoadStore || desc.Format == DXGI_FORMAT_R8G8B8A8_TYPELESS);
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

ID3D11RenderTargetView* PLSRenderTargetD3D::targetRTV()
{
    if (m_targetRTV == nullptr && m_targetTexture != nullptr)
    {
        D3D11_RENDER_TARGET_VIEW_DESC desc{};
        desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

        switch (m_targetFormat)
        {
            case DXGI_FORMAT_R8G8B8A8_UNORM:
            case DXGI_FORMAT_B8G8R8A8_UNORM:
                desc.Format = m_targetFormat;
                break;
            case DXGI_FORMAT_R8G8B8A8_TYPELESS:
                desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                break;

            default:
                RIVE_UNREACHABLE();
        }

        VERIFY_OK(m_gpu->CreateRenderTargetView(m_targetTexture.Get(),
                                                &desc,
                                                m_targetRTV.ReleaseAndGetAddressOf()));
    }
    return m_targetRTV.Get();
}

ID3D11Texture2D* PLSRenderTargetD3D::offscreenTexture()
{
    assert(!m_targetTextureSupportsUAV);
    if (m_offscreenTexture == nullptr)
    {
        m_offscreenTexture = make_simple_2d_texture(m_gpu.Get(),
                                                    DXGI_FORMAT_R8G8B8A8_TYPELESS,
                                                    width(),
                                                    height(),
                                                    1,
                                                    D3D11_BIND_UNORDERED_ACCESS);
    }
    return m_offscreenTexture.Get();
}

ID3D11UnorderedAccessView* PLSRenderTargetD3D::targetUAV()
{
    if (m_targetUAV == nullptr)
    {
        if (auto* uavTexture =
                m_targetTextureSupportsUAV ? m_targetTexture.Get() : offscreenTexture())
        {
            DXGI_FORMAT targetUavFormat;
            if (m_gpuSupportsTypedUAVLoadStore)
            {
                switch (m_targetFormat)
                {
                    case DXGI_FORMAT_R8G8B8A8_UNORM:
                    case DXGI_FORMAT_B8G8R8A8_UNORM:
                        targetUavFormat = m_targetFormat;
                        break;
                    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
                        targetUavFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
                        break;
                    default:
                        RIVE_UNREACHABLE();
                }
            }
            else
            {
                targetUavFormat = DXGI_FORMAT_R32_UINT;
            }
            m_targetUAV = make_simple_2d_uav(m_gpu.Get(), uavTexture, targetUavFormat);
        }
    }
    return m_targetUAV.Get();
}

ID3D11UnorderedAccessView* PLSRenderTargetD3D::clipUAV()
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
        m_clipUAV = make_simple_2d_uav(m_gpu.Get(), m_clipTexture.Get(), DXGI_FORMAT_R32_UINT);
    }
    return m_clipUAV.Get();
}

ID3D11UnorderedAccessView* PLSRenderTargetD3D::scratchColorUAV()
{
    if (m_scratchColorTexture == nullptr)
    {
        m_scratchColorTexture = make_simple_2d_texture(m_gpu.Get(),
                                                       DXGI_FORMAT_R8G8B8A8_TYPELESS,
                                                       width(),
                                                       height(),
                                                       1,
                                                       D3D11_BIND_UNORDERED_ACCESS);
    }
    if (m_scratchColorUAV == nullptr)
    {
        m_scratchColorUAV = make_simple_2d_uav(
            m_gpu.Get(),
            m_scratchColorTexture.Get(),
            m_gpuSupportsTypedUAVLoadStore ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_R32_UINT);
    }
    return m_scratchColorUAV.Get();
}

ID3D11UnorderedAccessView* PLSRenderTargetD3D::coverageUAV()
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
        m_coverageUAV =
            make_simple_2d_uav(m_gpu.Get(), m_coverageTexture.Get(), DXGI_FORMAT_R32_UINT);
    }
    return m_coverageUAV.Get();
}

void PLSRenderContextD3DImpl::resizeGradientTexture(uint32_t width, uint32_t height)
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
                                            D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
        VERIFY_OK(m_gpu->CreateShaderResourceView(m_gradTexture.Get(),
                                                  NULL,
                                                  m_gradTextureSRV.ReleaseAndGetAddressOf()));
        VERIFY_OK(m_gpu->CreateRenderTargetView(m_gradTexture.Get(),
                                                NULL,
                                                m_gradTextureRTV.ReleaseAndGetAddressOf()));
    }
}

void PLSRenderContextD3DImpl::resizeTessellationTexture(uint32_t width, uint32_t height)
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
                                            D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
        VERIFY_OK(m_gpu->CreateShaderResourceView(m_tessTexture.Get(),
                                                  NULL,
                                                  m_tessTextureSRV.ReleaseAndGetAddressOf()));
        VERIFY_OK(m_gpu->CreateRenderTargetView(m_tessTexture.Get(),
                                                NULL,
                                                m_tessTextureRTV.ReleaseAndGetAddressOf()));
    }
}

template <typename HighLevelStruct>
ID3D11ShaderResourceView* PLSRenderContextD3DImpl::replaceStructuredBufferSRV(
    const BufferRing* bufferRing,
    UINT highLevelStructCount,
    UINT firstHighLevelStruct)
{
    // Shaders access our storage buffers as arrays of basic types, as opposed to structures. Our
    // SRV therefore needs to be indexed by the underlying basic type, not the high level structure.
    constexpr static UINT kUnderlyingTypeSizeInBytes =
        gpu::StorageBufferElementSizeInBytes(HighLevelStruct::kBufferStructure);
    static_assert(sizeof(HighLevelStruct) % kUnderlyingTypeSizeInBytes == 0);
    constexpr static UINT kStructIndexMultiplier =
        sizeof(HighLevelStruct) / kUnderlyingTypeSizeInBytes;
    return static_cast<const StructuredBufferRingD3D*>(bufferRing)
        ->replaceSRV(m_gpu.Get(),
                     highLevelStructCount * kStructIndexMultiplier,
                     firstHighLevelStruct * kStructIndexMultiplier);
}

void PLSRenderContextD3DImpl::setPipelineLayoutAndShaders(DrawType drawType,
                                                          gpu::ShaderFeatures shaderFeatures,
                                                          gpu::InterlockMode interlockMode,
                                                          gpu::ShaderMiscFlags pixelShaderMiscFlags)
{
    uint32_t vertexShaderKey = gpu::ShaderUniqueKey(drawType,
                                                    shaderFeatures & kVertexShaderFeaturesMask,
                                                    interlockMode,
                                                    gpu::ShaderMiscFlags::none);
    auto vertexEntry = m_drawVertexShaders.find(vertexShaderKey);

    uint32_t pixelShaderKey =
        ShaderUniqueKey(drawType, shaderFeatures, interlockMode, pixelShaderMiscFlags);
    auto pixelEntry = m_drawPixelShaders.find(pixelShaderKey);

    if (vertexEntry == m_drawVertexShaders.end() || pixelEntry == m_drawPixelShaders.end())
    {
        std::ostringstream s;
        for (size_t i = 0; i < kShaderFeatureCount; ++i)
        {
            ShaderFeatures feature = static_cast<ShaderFeatures>(1 << i);
            if (shaderFeatures & feature)
            {
                s << "#define " << GetShaderFeatureGLSLName(feature) << " 1\n";
            }
        }
        if (m_d3dCapabilities.supportsRasterizerOrderedViews)
        {
            if ((interlockMode == gpu::InterlockMode::rasterOrdering &&
                 drawType != DrawType::interiorTriangulation) ||
                drawType == DrawType::imageMesh)
            {
                s << "#define " << GLSL_ENABLE_RASTERIZER_ORDERED_VIEWS << '\n';
            }
        }
        if (m_d3dCapabilities.supportsTypedUAVLoadStore)
        {
            s << "#define " << GLSL_ENABLE_TYPED_UAV_LOAD_STORE << '\n';
        }
        if (m_d3dCapabilities.supportsMin16Precision)
        {
            s << "#define " << GLSL_ENABLE_MIN_16_PRECISION << '\n';
        }
        if (pixelShaderMiscFlags & gpu::ShaderMiscFlags::fixedFunctionColorBlend)
        {
            s << "#define " << GLSL_FIXED_FUNCTION_COLOR_BLEND << '\n';
        }
        if (pixelShaderMiscFlags & gpu::ShaderMiscFlags::coalescedResolveAndTransfer)
        {
            s << "#define " << GLSL_COALESCED_PLS_RESOLVE_AND_TRANSFER << '\n';
            s << "#define " << GLSL_COLOR_PLANE_IDX_OVERRIDE << ' '
              << COALESCED_OFFSCREEN_COLOR_PLANE_IDX << '\n';
        }
        switch (drawType)
        {
            case DrawType::midpointFanPatches:
            case DrawType::outerCurvePatches:
                s << "#define " << GLSL_DRAW_PATH << '\n';
                break;
            case DrawType::interiorTriangulation:
                s << "#define " << GLSL_DRAW_INTERIOR_TRIANGLES << '\n';
                break;
            case DrawType::imageRect:
                assert(interlockMode == gpu::InterlockMode::atomics);
                s << "#define " << GLSL_DRAW_IMAGE << '\n';
                s << "#define " << GLSL_DRAW_IMAGE_RECT << '\n';
                break;
            case DrawType::imageMesh:
                s << "#define " << GLSL_DRAW_IMAGE << '\n';
                s << "#define " << GLSL_DRAW_IMAGE_MESH << '\n';
                break;
            case DrawType::gpuAtomicResolve:
                assert(interlockMode == gpu::InterlockMode::atomics);
                s << "#define " << GLSL_DRAW_RENDER_TARGET_UPDATE_BOUNDS << '\n';
                s << "#define " << GLSL_RESOLVE_PLS << '\n';
                break;
            case DrawType::gpuAtomicInitialize:
            case DrawType::stencilClipReset:
                RIVE_UNREACHABLE();
        }
        s << glsl::constants << '\n';
        s << glsl::hlsl << '\n';
        s << glsl::common << '\n';
        if (shaderFeatures & ShaderFeatures::ENABLE_ADVANCED_BLEND)
        {
            s << glsl::advanced_blend << '\n';
        }
        switch (drawType)
        {
            case DrawType::midpointFanPatches:
            case DrawType::outerCurvePatches:
                s << gpu::glsl::draw_path_common << '\n';
                s << (interlockMode == gpu::InterlockMode::rasterOrdering ? gpu::glsl::draw_path
                                                                          : gpu::glsl::atomic_draw)
                  << '\n';
                break;
            case DrawType::interiorTriangulation:
                s << gpu::glsl::draw_path_common << '\n';
                s << (interlockMode == gpu::InterlockMode::rasterOrdering ? gpu::glsl::draw_path
                                                                          : gpu::glsl::atomic_draw)
                  << '\n';
                break;
            case DrawType::imageRect:
                assert(interlockMode == gpu::InterlockMode::atomics);
                s << gpu::glsl::atomic_draw << '\n';
                break;
            case DrawType::imageMesh:
                s << (interlockMode == gpu::InterlockMode::rasterOrdering
                          ? gpu::glsl::draw_image_mesh
                          : gpu::glsl::atomic_draw)
                  << '\n';
                break;
            case DrawType::gpuAtomicResolve:
            case DrawType::stencilClipReset:
                assert(interlockMode == gpu::InterlockMode::atomics);
                s << gpu::glsl::atomic_draw << '\n';
                break;
            case DrawType::gpuAtomicInitialize:
                RIVE_UNREACHABLE();
        }

        const std::string shader = s.str();

        if (vertexEntry == m_drawVertexShaders.end())
        {
            DrawVertexShader drawVertexShader;
            ComPtr<ID3DBlob> blob =
                compileSourceToBlob(GLSL_VERTEX, shader.c_str(), GLSL_drawVertexMain, "vs_5_0");
            D3D11_INPUT_ELEMENT_DESC layoutDesc[2];
            uint32_t vertexAttribCount;
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
                case DrawType::imageRect:
                    layoutDesc[0] = {GLSL_a_imageRectVertex,
                                     0,
                                     DXGI_FORMAT_R32G32B32A32_FLOAT,
                                     kImageRectVertexDataSlot,
                                     0,
                                     D3D11_INPUT_PER_VERTEX_DATA,
                                     0};
                    vertexAttribCount = 1;
                    break;
                case DrawType::imageMesh:
                    layoutDesc[0] = {GLSL_a_position,
                                     0,
                                     DXGI_FORMAT_R32G32_FLOAT,
                                     kImageMeshVertexDataSlot,
                                     D3D11_APPEND_ALIGNED_ELEMENT,
                                     D3D11_INPUT_PER_VERTEX_DATA,
                                     0};
                    layoutDesc[1] = {GLSL_a_texCoord,
                                     0,
                                     DXGI_FORMAT_R32G32_FLOAT,
                                     kImageMeshUVDataSlot,
                                     D3D11_APPEND_ALIGNED_ELEMENT,
                                     D3D11_INPUT_PER_VERTEX_DATA,
                                     0};
                    vertexAttribCount = 2;
                    break;
                case DrawType::gpuAtomicResolve:
                    vertexAttribCount = 0;
                    break;
                case DrawType::gpuAtomicInitialize:
                case DrawType::stencilClipReset:
                    RIVE_UNREACHABLE();
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
            ComPtr<ID3DBlob> blob =
                compileSourceToBlob(GLSL_FRAGMENT, shader.c_str(), GLSL_drawFragmentMain, "ps_5_0");
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

static ID3D11Buffer* submitted_buffer(const BufferRing* bufferRing)
{
    assert(bufferRing != nullptr);
    return static_cast<const BufferRingD3D*>(bufferRing)->submittedBuffer();
}

static const char* heap_buffer_contents(const BufferRing* bufferRing)
{
    assert(bufferRing != nullptr);
    auto heapBuffer = static_cast<const HeapBufferRing*>(bufferRing);
    return reinterpret_cast<const char*>(heapBuffer->contents());
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
    gpuContext->CopySubresourceRegion(dst, 0, updateBox.left, updateBox.top, 0, src, 0, &updateBox);
}

void PLSRenderContextD3DImpl::flush(const FlushDescriptor& desc)
{
    auto renderTarget = static_cast<PLSRenderTargetD3D*>(desc.renderTarget);

    m_gpuContext->ClearState();

    // All programs use the same set of per-flush uniforms.
    m_gpuContext->UpdateSubresource(m_flushUniforms.Get(),
                                    0,
                                    NULL,
                                    heap_buffer_contents(flushUniformBufferRing()) +
                                        desc.flushUniformDataOffsetInBytes,
                                    0,
                                    0);

    ID3D11Buffer* uniformBuffers[] = {m_flushUniforms.Get(),
                                      m_drawUniforms.Get(),
                                      m_imageDrawUniforms.Get()};
    static_assert(PATH_BASE_INSTANCE_UNIFORM_BUFFER_IDX == FLUSH_UNIFORM_BUFFER_IDX + 1);
    static_assert(IMAGE_DRAW_UNIFORM_BUFFER_IDX == PATH_BASE_INSTANCE_UNIFORM_BUFFER_IDX + 1);
    m_gpuContext->VSSetConstantBuffers(FLUSH_UNIFORM_BUFFER_IDX,
                                       std::size(uniformBuffers),
                                       uniformBuffers);

    // All programs use the same storage buffers.
    ID3D11ShaderResourceView* storageBufferBufferSRVs[] = {
        desc.pathCount > 0 ? replaceStructuredBufferSRV<gpu::PathData>(
                                 pathBufferRing(),
                                 desc.pathCount,
                                 math::lossless_numeric_cast<UINT>(desc.firstPath))
                           : nullptr,
        desc.pathCount > 0 ? replaceStructuredBufferSRV<gpu::PaintData>(
                                 paintBufferRing(),
                                 desc.pathCount,
                                 math::lossless_numeric_cast<UINT>(desc.firstPaint))
                           : nullptr,
        desc.pathCount > 0 ? replaceStructuredBufferSRV<gpu::PaintAuxData>(
                                 paintAuxBufferRing(),
                                 desc.pathCount,
                                 math::lossless_numeric_cast<UINT>(desc.firstPaintAux))
                           : nullptr,
        desc.contourCount > 0 ? replaceStructuredBufferSRV<gpu::ContourData>(
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
        m_gpuContext->PSSetShaderResources(PAINT_BUFFER_IDX, 2, storageBufferBufferSRVs + 1);
    }

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

        // Unbind the gradient texture before rendering it.
        ID3D11ShaderResourceView* nullTextureView = nullptr;
        m_gpuContext->PSSetShaderResources(GRAD_TEXTURE_IDX, 1, &nullTextureView);

        m_gpuContext->PSSetShader(m_colorRampPixelShader.Get(), NULL, 0);

        m_gpuContext->OMSetRenderTargets(1, m_gradTextureRTV.GetAddressOf(), NULL);

        m_gpuContext->DrawInstanced(4,
                                    desc.complexGradSpanCount,
                                    0,
                                    math::lossless_numeric_cast<UINT>(desc.firstComplexGradSpan));
    }

    // Copy the simple color ramps to the gradient texture.
    if (desc.simpleGradTexelsHeight > 0)
    {
        assert(desc.simpleGradTexelsHeight * desc.simpleGradTexelsWidth * 4 <=
               simpleColorRampsBufferRing()->capacityInBytes());
        D3D11_BOX box;
        box.left = 0;
        box.right = desc.simpleGradTexelsWidth;
        box.top = 0;
        box.bottom = desc.simpleGradTexelsHeight;
        box.front = 0;
        box.back = 1;
        m_gpuContext->UpdateSubresource(m_gradTexture.Get(),
                                        0,
                                        &box,
                                        heap_buffer_contents(simpleColorRampsBufferRing()) +
                                            desc.simpleGradDataOffsetInBytes,
                                        kGradTextureWidth * 4,
                                        0);
    }

    // Tessellate all curves into vertices in the tessellation texture.
    if (desc.tessVertexSpanCount > 0)
    {
        ID3D11Buffer* tessSpanBuffer = submitted_buffer(tessSpanBufferRing());
        UINT tessStride = sizeof(TessVertexSpan);
        UINT tessOffset = 0;
        m_gpuContext->IASetVertexBuffers(0, 1, &tessSpanBuffer, &tessStride, &tessOffset);
        m_gpuContext->IASetIndexBuffer(m_tessSpanIndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
        m_gpuContext->IASetInputLayout(m_tessellateLayout.Get());
        m_gpuContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        m_gpuContext->VSSetShader(m_tessellateVertexShader.Get(), NULL, 0);

        // Unbind the tessellation texture before rendering it.
        ID3D11ShaderResourceView* nullTessTextureView = NULL;
        m_gpuContext->VSSetShaderResources(TESS_VERTEX_TEXTURE_IDX, 1, &nullTessTextureView);

        D3D11_VIEWPORT viewport = {0,
                                   0,
                                   static_cast<float>(kTessTextureWidth),
                                   static_cast<float>(desc.tessDataHeight),
                                   0,
                                   1};
        m_gpuContext->RSSetViewports(1, &viewport);

        m_gpuContext->PSSetShader(m_tessellatePixelShader.Get(), NULL, 0);

        m_gpuContext->OMSetRenderTargets(1, m_tessTextureRTV.GetAddressOf(), NULL);

        m_gpuContext->DrawIndexedInstanced(
            std::size(gpu::kTessSpanIndices),
            desc.tessVertexSpanCount,
            0,
            0,
            math::lossless_numeric_cast<UINT>(desc.firstTessVertexSpan));

        if (m_d3dCapabilities.isIntel)
        {
            // FIXME! Intel needs this flush! Driver bug? Find a lighter workaround?
            m_gpuContext->Flush();
        }
    }

    // Setup and clear the PLS textures.
    bool renderDirectToRasterPipeline =
        desc.interlockMode == InterlockMode::atomics &&
        !(desc.combinedShaderFeatures & ShaderFeatures::ENABLE_ADVANCED_BLEND);
    switch (desc.colorLoadAction)
    {
        case gpu::LoadAction::clear:
            if (renderDirectToRasterPipeline)
            {
                float clearColor4f[4];
                UnpackColorToRGBA32F(desc.clearColor, clearColor4f);
                m_gpuContext->ClearRenderTargetView(renderTarget->targetRTV(), clearColor4f);
            }
            else if (m_d3dCapabilities.supportsTypedUAVLoadStore)
            {
                float clearColor4f[4];
                UnpackColorToRGBA32F(desc.clearColor, clearColor4f);
                m_gpuContext->ClearUnorderedAccessViewFloat(renderTarget->targetUAV(),
                                                            clearColor4f);
            }
            else
            {
                UINT clearColorui[4] = {gpu::SwizzleRiveColorToRGBA(desc.clearColor)};
                m_gpuContext->ClearUnorderedAccessViewUint(renderTarget->targetUAV(), clearColorui);
            }
            break;
        case gpu::LoadAction::preserveRenderTarget:
            if (!renderDirectToRasterPipeline && !renderTarget->targetTextureSupportsUAV())
            {
                // We're rendering to an offscreen UAV and preserving the target. Copy the target
                // texture over.
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
        m_gpuContext->ClearUnorderedAccessViewUint(renderTarget->clipUAV(), kZero);
    }
    {
        UINT coverageClear[4]{desc.coverageClearValue};
        m_gpuContext->ClearUnorderedAccessViewUint(renderTarget->coverageUAV(), coverageClear);
    }

    // Execute the DrawList.
    ID3D11Buffer* vertexBuffers[3] = {
        m_patchVertexBuffer.Get(),
        desc.hasTriangleVertices ? submitted_buffer(triangleBufferRing()) : NULL,
        m_imageRectVertexBuffer.Get()};
    UINT vertexStrides[3] = {sizeof(gpu::PatchVertex),
                             sizeof(gpu::TriangleVertex),
                             sizeof(gpu::ImageRectVertex)};
    UINT vertexOffsets[3] = {0, 0, 0};
    static_assert(kPatchVertexDataSlot == 0);
    static_assert(kTriangleVertexDataSlot == 1);
    static_assert(kImageRectVertexDataSlot == 2);
    m_gpuContext->IASetVertexBuffers(0, 3, vertexBuffers, vertexStrides, vertexOffsets);

    D3D11_VIEWPORT viewport = {0,
                               0,
                               static_cast<float>(renderTarget->width()),
                               static_cast<float>(renderTarget->height()),
                               0,
                               1};
    m_gpuContext->RSSetViewports(1, &viewport);

    m_gpuContext->PSSetConstantBuffers(IMAGE_DRAW_UNIFORM_BUFFER_IDX,
                                       1,
                                       m_imageDrawUniforms.GetAddressOf());

    ID3D11RenderTargetView* targetRTV =
        renderDirectToRasterPipeline ? renderTarget->targetRTV() : NULL;
    ID3D11UnorderedAccessView* plsUAVs[] = {
        renderDirectToRasterPipeline ? NULL : renderTarget->targetUAV(),
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
        renderDirectToRasterPipeline ? 1 : 0,
        &targetRTV,
        NULL,
        renderDirectToRasterPipeline ? 1 : 0,
        renderDirectToRasterPipeline ? std::size(plsUAVs) - 1 : std::size(plsUAVs),
        renderDirectToRasterPipeline ? plsUAVs + 1 : plsUAVs,
        NULL);

    if (renderDirectToRasterPipeline)
    {
        // When rendering directly to the target RTV, we use the built-in blend hardware for opacity
        // and antialiasing.
        m_gpuContext->OMSetBlendState(m_srcOverBlendState.Get(), NULL, 0xffffffff);
    }

    // Set these last, when the tess and grad textures are no longer bound as render targets.
    m_gpuContext->VSSetShaderResources(TESS_VERTEX_TEXTURE_IDX, 1, m_tessTextureSRV.GetAddressOf());
    m_gpuContext->PSSetShaderResources(GRAD_TEXTURE_IDX, 1, m_gradTextureSRV.GetAddressOf());

    const char* const imageDrawUniformData = heap_buffer_contents(imageDrawUniformBufferRing());

    bool renderPassHasCoalescedResolveAndTransfer =
        desc.interlockMode == gpu::InterlockMode::atomics && !renderDirectToRasterPipeline &&
        !renderTarget->targetTextureSupportsUAV();

    for (const DrawBatch& batch : *desc.drawList)
    {
        if (batch.elementCount == 0)
        {
            continue;
        }

        DrawType drawType = batch.drawType;
        auto shaderFeatures = desc.interlockMode == gpu::InterlockMode::atomics
                                  ? desc.combinedShaderFeatures
                                  : batch.shaderFeatures;
        auto pixelShaderMiscFlags =
            drawType == gpu::DrawType::gpuAtomicResolve && renderPassHasCoalescedResolveAndTransfer
                ? gpu::ShaderMiscFlags::coalescedResolveAndTransfer
                : gpu::ShaderMiscFlags::none;
        if (renderDirectToRasterPipeline)
        {
            pixelShaderMiscFlags |= gpu::ShaderMiscFlags::fixedFunctionColorBlend;
        }
        setPipelineLayoutAndShaders(drawType,
                                    shaderFeatures,
                                    desc.interlockMode,
                                    pixelShaderMiscFlags);

        if (auto imageTextureD3D = static_cast<const PLSTextureD3DImpl*>(batch.imageTexture))
        {
            m_gpuContext->PSSetShaderResources(IMAGE_TEXTURE_IDX,
                                               1,
                                               imageTextureD3D->srvAddressOf());
        }

        switch (drawType)
        {
            case DrawType::midpointFanPatches:
            case DrawType::outerCurvePatches:
            {
                m_gpuContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                m_gpuContext->IASetIndexBuffer(m_patchIndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
                m_gpuContext->RSSetState(m_backCulledRasterState[desc.wireframe].Get());
                DrawUniforms drawUniforms(batch.baseElement);
                m_gpuContext->UpdateSubresource(m_drawUniforms.Get(), 0, NULL, &drawUniforms, 0, 0);
                m_gpuContext->DrawIndexedInstanced(PatchIndexCount(drawType),
                                                   batch.elementCount,
                                                   PatchBaseIndex(drawType),
                                                   0,
                                                   batch.baseElement);
                break;
            }
            case DrawType::interiorTriangulation:
            {
                m_gpuContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                m_gpuContext->RSSetState(m_backCulledRasterState[desc.wireframe].Get());
                m_gpuContext->Draw(batch.elementCount, batch.baseElement);
                break;
            }
            case DrawType::imageRect:
                m_gpuContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                m_gpuContext->IASetIndexBuffer(m_imageRectIndexBuffer.Get(),
                                               DXGI_FORMAT_R16_UINT,
                                               0);
                m_gpuContext->RSSetState(m_doubleSidedRasterState[desc.wireframe].Get());
                m_gpuContext->UpdateSubresource(m_imageDrawUniforms.Get(),
                                                0,
                                                NULL,
                                                imageDrawUniformData + batch.imageDrawDataOffset,
                                                0,
                                                0);
                m_gpuContext->DrawIndexed(std::size(gpu::kImageRectIndices), 0, 0);
                break;
            case DrawType::imageMesh:
            {
                LITE_RTTI_CAST_OR_BREAK(vertexBuffer,
                                        const RenderBufferD3DImpl*,
                                        batch.vertexBuffer);
                LITE_RTTI_CAST_OR_BREAK(uvBuffer, const RenderBufferD3DImpl*, batch.uvBuffer);
                LITE_RTTI_CAST_OR_BREAK(indexBuffer, const RenderBufferD3DImpl*, batch.indexBuffer);
                ID3D11Buffer* imageMeshBuffers[] = {vertexBuffer->buffer(), uvBuffer->buffer()};
                UINT imageMeshStrides[] = {sizeof(Vec2D), sizeof(Vec2D)};
                UINT imageMeshOffsets[] = {0, 0};
                m_gpuContext->IASetVertexBuffers(kImageMeshVertexDataSlot,
                                                 2,
                                                 imageMeshBuffers,
                                                 imageMeshStrides,
                                                 imageMeshOffsets);
                static_assert(kImageMeshUVDataSlot == kImageMeshVertexDataSlot + 1);
                m_gpuContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                m_gpuContext->IASetIndexBuffer(indexBuffer->buffer(), DXGI_FORMAT_R16_UINT, 0);
                m_gpuContext->RSSetState(m_doubleSidedRasterState[desc.wireframe].Get());
                m_gpuContext->UpdateSubresource(m_imageDrawUniforms.Get(),
                                                0,
                                                NULL,
                                                imageDrawUniformData + batch.imageDrawDataOffset,
                                                0,
                                                0);
                m_gpuContext->DrawIndexed(batch.elementCount, batch.baseElement, 0);
                break;
            }
            case DrawType::gpuAtomicResolve:
                assert(desc.interlockMode == gpu::InterlockMode::atomics);
                m_gpuContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
                m_gpuContext->RSSetState(m_backCulledRasterState[0].Get());
                if (renderPassHasCoalescedResolveAndTransfer)
                {
                    // Bind the actual target texture as the render target for the PLS resolve, so
                    // we don't have to copy to it after the render pass.
                    // (And ince we're changing the render target, this also better be the final
                    // batch of the render pass.)
                    assert(&batch == &desc.drawList->tail());
                    assert(!renderDirectToRasterPipeline);
                    assert(!renderTarget->targetTextureSupportsUAV());
                    ID3D11RenderTargetView* resolveRTV = renderTarget->targetRTV();
                    ID3D11UnorderedAccessView* resolveUAVs[] = {
                        renderTarget->clipUAV(),
                        renderTarget->targetUAV(), // Bind the target UAV (for reading) to a
                                                   // different slot for the resolve because D3D
                                                   // doesn't let us use slot 0 when there's a
                                                   // render target.
                        renderTarget->coverageUAV(),
                    };
                    static_assert(CLIP_PLANE_IDX == 1);
                    static_assert(COALESCED_OFFSCREEN_COLOR_PLANE_IDX == 2);
                    static_assert(COVERAGE_PLANE_IDX == 3);
                    m_gpuContext->OMSetRenderTargetsAndUnorderedAccessViews(1,
                                                                            &resolveRTV,
                                                                            NULL,
                                                                            1,
                                                                            std::size(resolveUAVs),
                                                                            resolveUAVs,
                                                                            NULL);
                }
                m_gpuContext->Draw(4, 0);
                break;
            case DrawType::gpuAtomicInitialize:
            case DrawType::stencilClipReset:
                RIVE_UNREACHABLE();
        }
    }

    if (desc.interlockMode == gpu::InterlockMode::rasterOrdering &&
        !renderTarget->targetTextureSupportsUAV())
    {
        // We rendered to an offscreen UAV and did not resolve to the renderTarget. Copy back to the
        // main target.
        assert(!renderDirectToRasterPipeline);
        assert(!renderPassHasCoalescedResolveAndTransfer);
        blit_sub_rect(m_gpuContext.Get(),
                      renderTarget->targetTexture(),
                      renderTarget->offscreenTexture(),
                      desc.renderTargetUpdateBounds);
    }
}
} // namespace rive::gpu
