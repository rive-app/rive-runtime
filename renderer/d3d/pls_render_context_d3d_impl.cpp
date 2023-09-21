/*
 * Copyright 2023 Rive
 */

#include "rive/pls/d3d/pls_render_context_d3d_impl.hpp"

#include "rive/pls/pls_image.hpp"
#include "shaders/constants.glsl"

#include <D3DCompiler.h>
#include <sstream>

#include "../out/obj/generated/advanced_blend.glsl.hpp"
#include "../out/obj/generated/color_ramp.glsl.hpp"
#include "../out/obj/generated/constants.glsl.hpp"
#include "../out/obj/generated/common.glsl.hpp"
#include "../out/obj/generated/draw_image_mesh.glsl.hpp"
#include "../out/obj/generated/draw_path.glsl.hpp"
#include "../out/obj/generated/hlsl.glsl.hpp"
#include "../out/obj/generated/tessellate.glsl.hpp"

constexpr static UINT kPatchVertexDataSlot = 0;
constexpr static UINT kTriangleVertexDataSlot = 1;
constexpr static UINT kImageMeshVertexDataSlot = 2;
constexpr static UINT kImageMeshUVDataSlot = 3;

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
    VERIFY_OK(gpu->CreateTexture2D(&desc, NULL, tex.GetAddressOf()));
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

std::unique_ptr<PLSRenderContext> PLSRenderContextD3DImpl::MakeContext(
    ComPtr<ID3D11Device> gpu,
    ComPtr<ID3D11DeviceContext> gpuContext,
    bool isIntel)
{
    auto plsContextImpl = std::unique_ptr<PLSRenderContextD3DImpl>(
        new PLSRenderContextD3DImpl(std::move(gpu), std::move(gpuContext), isIntel));
    return std::make_unique<PLSRenderContext>(std::move(plsContextImpl));
}

PLSRenderContextD3DImpl::PLSRenderContextD3DImpl(ComPtr<ID3D11Device> gpu,
                                                 ComPtr<ID3D11DeviceContext> gpuContext,
                                                 bool isIntel) :
    m_isIntel(isIntel), m_gpu(std::move(gpu)), m_gpuContext(std::move(gpuContext))
{
    m_platformFeatures.invertOffscreenY = true;

    // Create a default raster state for path and offscreen draws.
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
    VERIFY_OK(m_gpu->CreateRasterizerState(&rasterDesc, m_pathRasterState[0].GetAddressOf()));

    // ...And with wireframe for debugging.
    rasterDesc.FillMode = D3D11_FILL_WIREFRAME;
    VERIFY_OK(m_gpu->CreateRasterizerState(&rasterDesc, m_pathRasterState[1].GetAddressOf()));

    // Create a raster state without face culling for drawing image meshes.
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.CullMode = D3D11_CULL_NONE;
    VERIFY_OK(m_gpu->CreateRasterizerState(&rasterDesc, m_imageMeshRasterState[0].GetAddressOf()));

    // ...And once more with wireframe for debugging.
    rasterDesc.FillMode = D3D11_FILL_WIREFRAME;
    VERIFY_OK(m_gpu->CreateRasterizerState(&rasterDesc, m_imageMeshRasterState[1].GetAddressOf()));

    // Compile the shaders that render gradient color ramps.
    {
        std::ostringstream s;
        s << glsl::hlsl << '\n';
        s << glsl::constants << '\n';
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
        s << glsl::constants << '\n';
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

        D3D11_BUFFER_DESC tessIndexBufferDesc{};
        tessIndexBufferDesc.ByteWidth = sizeof(pls::kTessSpanIndices);
        tessIndexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
        tessIndexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        tessIndexBufferDesc.StructureByteStride = sizeof(pls::kTessSpanIndices);

        D3D11_SUBRESOURCE_DATA tessIndexDataDesc{};
        tessIndexDataDesc.pSysMem = pls::kTessSpanIndices;

        VERIFY_OK(m_gpu->CreateBuffer(&tessIndexBufferDesc,
                                      &tessIndexDataDesc,
                                      m_tessSpanIndexBuffer.GetAddressOf()));
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

        VERIFY_OK(m_gpu->CreateBuffer(&vertexBufferDesc,
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

        VERIFY_OK(m_gpu->CreateBuffer(&indexBufferDesc,
                                      &indexDataDesc,
                                      m_patchIndexBuffer.GetAddressOf()));
    }

    // Create buffers for uniforms.
    {
        D3D11_BUFFER_DESC desc{};
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

        desc.ByteWidth = sizeof(pls::FlushUniforms);
        desc.StructureByteStride = sizeof(pls::FlushUniforms);
        VERIFY_OK(m_gpu->CreateBuffer(&desc, nullptr, m_flushUniforms.GetAddressOf()));

        desc.ByteWidth = sizeof(DrawUniforms);
        desc.StructureByteStride = sizeof(DrawUniforms);
        VERIFY_OK(m_gpu->CreateBuffer(&desc, nullptr, m_drawUniforms.GetAddressOf()));

        desc.ByteWidth = sizeof(pls::ImageMeshUniforms);
        desc.StructureByteStride = sizeof(pls::ImageMeshUniforms);
        VERIFY_OK(m_gpu->CreateBuffer(&desc, nullptr, m_imageMeshUniforms.GetAddressOf()));
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
    VERIFY_OK(m_gpu->CreateSamplerState(&linearSamplerDesc, m_linearSampler.GetAddressOf()));

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
    VERIFY_OK(m_gpu->CreateSamplerState(&mipmapSamplerDesc, m_mipmapSampler.GetAddressOf()));

    ID3D11SamplerState* samplers[2] = {m_linearSampler.Get(), m_mipmapSampler.Get()};
    static_assert(IMAGE_TEXTURE_IDX == GRAD_TEXTURE_IDX + 1);
    m_gpuContext->PSSetSamplers(GRAD_TEXTURE_IDX, 2, samplers);
}

class RenderBufferD3DImpl : public RenderBuffer
{
public:
    RenderBufferD3DImpl(RenderBufferType renderBufferType,
                        RenderBufferFlags renderBufferFlags,
                        size_t sizeInBytes,
                        ComPtr<ID3D11Device> gpu,
                        ComPtr<ID3D11DeviceContext> gpuContext) :
        RenderBuffer(renderBufferType, renderBufferFlags, sizeInBytes),
        m_gpu(std::move(gpu)),
        m_gpuContext(std::move(gpuContext))
    {
        m_desc.ByteWidth = sizeInBytes;
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
            VERIFY_OK(m_gpu->CreateBuffer(&m_desc, nullptr, m_buffer.GetAddressOf()));
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
            VERIFY_OK(m_gpu->CreateBuffer(&m_desc, &bufferDataDesc, m_buffer.GetAddressOf()));
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
    PLSTextureD3DImpl(ID3D11Device* gpu,
                      ID3D11DeviceContext* gpuContext,
                      UINT width,
                      UINT height,
                      UINT mipLevelCount,
                      const uint8_t imageDataRGBA[]) :
        PLSTexture(width, height)
    {
        m_texture = make_simple_2d_texture(gpu,
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
        gpuContext->UpdateSubresource(m_texture.Get(), 0, &box, imageDataRGBA, width * 4, 0);

        // Create a view and generate mipmaps.
        VERIFY_OK(gpu->CreateShaderResourceView(m_texture.Get(), NULL, m_srv.GetAddressOf()));
        gpuContext->GenerateMips(m_srv.Get());
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
    return make_rcp<PLSTextureD3DImpl>(m_gpu.Get(),
                                       m_gpuContext.Get(),
                                       width,
                                       height,
                                       mipLevelCount,
                                       imageDataRGBA);
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
                make_simple_2d_texture(gpu, formatD3D, widthInTexels(), height, 1, bindFlags);
            VERIFY_OK(
                gpu->CreateShaderResourceView(m_textures[i].Get(), NULL, m_srvs[i].GetAddressOf()));
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

std::unique_ptr<TexelBufferRing> PLSRenderContextD3DImpl::makeTexelBufferRing(
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

std::unique_ptr<BufferRing> PLSRenderContextD3DImpl::makeVertexBufferRing(size_t capacity,
                                                                          size_t itemSizeInBytes)
{
    return std::make_unique<BufferRingD3D>(m_gpu.Get(),
                                           m_gpuContext,
                                           capacity,
                                           itemSizeInBytes,
                                           D3D11_BIND_VERTEX_BUFFER);
}

std::unique_ptr<BufferRing> PLSRenderContextD3DImpl::makePixelUnpackBufferRing(
    size_t capacity,
    size_t itemSizeInBytes)
{
    // It appears impossible to update a D3D texture from a GPU buffer; store this data on the heap
    // and upload it to the texture at flush time.
    return std::make_unique<HeapBufferRing>(capacity, itemSizeInBytes);
}

std::unique_ptr<BufferRing> PLSRenderContextD3DImpl::makeUniformBufferRing(size_t capacity,
                                                                           size_t itemSizeInBytes)
{
    // In D3D we update uniform data inline with commands, rather than filling a buffer up front.
    return std::make_unique<HeapBufferRing>(capacity, itemSizeInBytes);
}

PLSRenderTargetD3D::PLSRenderTargetD3D(ID3D11Device* gpu, size_t width, size_t height) :
    PLSRenderTarget(width, height)
{
    m_coverageTexture = make_simple_2d_texture(gpu,
                                               DXGI_FORMAT_R32_UINT,
                                               width,
                                               height,
                                               1,
                                               D3D11_BIND_UNORDERED_ACCESS);
    m_originalDstColorTexture = make_simple_2d_texture(gpu,
                                                       DXGI_FORMAT_R8G8B8A8_UNORM,
                                                       width,
                                                       height,
                                                       1,
                                                       D3D11_BIND_UNORDERED_ACCESS);
    m_clipTexture = make_simple_2d_texture(gpu,
                                           DXGI_FORMAT_R32_UINT,
                                           width,
                                           height,
                                           1,
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

rcp<PLSRenderTargetD3D> PLSRenderContextD3DImpl::makeRenderTarget(size_t width, size_t height)
{
    return rcp(new PLSRenderTargetD3D(m_gpu.Get(), width, height));
}

void PLSRenderContextD3DImpl::resizeGradientTexture(size_t height)
{
    m_gradTexture = make_simple_2d_texture(m_gpu.Get(),
                                           DXGI_FORMAT_R8G8B8A8_UNORM,
                                           kGradTextureWidth,
                                           height,
                                           1,
                                           D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
    VERIFY_OK(m_gpu->CreateShaderResourceView(m_gradTexture.Get(),
                                              NULL,
                                              m_gradTextureSRV.GetAddressOf()));
    VERIFY_OK(
        m_gpu->CreateRenderTargetView(m_gradTexture.Get(), NULL, m_gradTextureRTV.GetAddressOf()));
}

void PLSRenderContextD3DImpl::resizeTessellationTexture(size_t height)
{
    m_tessTexture = make_simple_2d_texture(m_gpu.Get(),
                                           DXGI_FORMAT_R32G32B32A32_UINT,
                                           kTessTextureWidth,
                                           height,
                                           1,
                                           D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
    VERIFY_OK(m_gpu->CreateShaderResourceView(m_tessTexture.Get(),
                                              NULL,
                                              m_tessTextureSRV.GetAddressOf()));
    VERIFY_OK(
        m_gpu->CreateRenderTargetView(m_tessTexture.Get(), NULL, m_tessTextureRTV.GetAddressOf()));
}

void PLSRenderContextD3DImpl::setPipelineLayoutAndShaders(DrawType drawType,
                                                          ShaderFeatures shaderFeatures)
{
    uint32_t vertexShaderKey =
        ShaderUniqueKey(drawType, shaderFeatures & kVertexShaderFeaturesMask);
    auto vertexEntry = m_drawVertexShaders.find(vertexShaderKey);

    uint32_t pixelShaderKey = ShaderUniqueKey(drawType, shaderFeatures);
    auto pixelEntry = m_drawPixelShaders.find(pixelShaderKey);

    if (vertexEntry == m_drawVertexShaders.end() || pixelEntry == m_drawPixelShaders.end())
    {
        std::ostringstream s;
        if (drawType == DrawType::interiorTriangulation)
        {
            s << "#define " << GLSL_DRAW_INTERIOR_TRIANGLES << '\n';
        }
        for (size_t i = 0; i < kShaderFeatureCount; ++i)
        {
            ShaderFeatures feature = static_cast<ShaderFeatures>(1 << i);
            if (shaderFeatures & feature)
            {
                s << "#define " << GetShaderFeatureGLSLName(feature) << '\n';
            }
        }
        s << "#define " << GLSL_ENABLE_BASE_INSTANCE_POLYFILL << '\n';
        s << glsl::hlsl << '\n';
        s << glsl::constants << '\n';
        s << glsl::common << '\n';
        if (shaderFeatures & ShaderFeatures::ENABLE_ADVANCED_BLEND)
        {
            s << glsl::advanced_blend << '\n';
        }
        switch (drawType)
        {
            case DrawType::midpointFanPatches:
            case DrawType::outerCurvePatches:
            case DrawType::interiorTriangulation:
                s << pls::glsl::draw_path << '\n';
                break;
            case DrawType::imageMesh:
                s << pls::glsl::draw_image_mesh << '\n';
                break;
        }

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

static ID3D11Buffer* submitted_buffer(const BufferRing* bufferRing)
{
    return static_cast<const BufferRingD3D*>(bufferRing)->submittedBuffer();
}

static ID3D11ShaderResourceView* submitted_srv(const TexelBufferRing* texelBufferRing)
{
    return static_cast<const TexelBufferD3D*>(texelBufferRing)->submittedSRV();
}

static const void* heap_buffer_contents(const BufferRing* bufferRing)
{
    return static_cast<const HeapBufferRing*>(bufferRing)->contents();
}

void PLSRenderContextD3DImpl::flush(const PLSRenderContext::FlushDescriptor& desc)
{
    auto renderTarget = static_cast<const PLSRenderTargetD3D*>(desc.renderTarget);

    constexpr static UINT kZero[4]{};
    if (desc.loadAction == LoadAction::clear)
    {
        float clearColor4f[4];
        UnpackColorToRGBA32F(desc.clearColor, clearColor4f);
        m_gpuContext->ClearUnorderedAccessViewFloat(renderTarget->m_targetUAV.Get(), clearColor4f);
    }
    m_gpuContext->ClearUnorderedAccessViewUint(renderTarget->m_coverageUAV.Get(), kZero);
    if (desc.needsClipBuffer)
    {
        m_gpuContext->ClearUnorderedAccessViewUint(renderTarget->m_clipUAV.Get(), kZero);
    }

    ID3D11Buffer* cbuffers[] = {m_flushUniforms.Get(),
                                m_drawUniforms.Get(),
                                m_imageMeshUniforms.Get()};
    static_assert(DRAW_UNIFORM_BUFFER_IDX == FLUSH_UNIFORM_BUFFER_IDX + 1);
    static_assert(IMAGE_MESH_UNIFORM_BUFFER_IDX == DRAW_UNIFORM_BUFFER_IDX + 1);
    m_gpuContext->VSSetConstantBuffers(FLUSH_UNIFORM_BUFFER_IDX, std::size(cbuffers), cbuffers);

    m_gpuContext->RSSetState(m_pathRasterState[0].Get());

    // All programs use the same set of per-flush uniforms.
    m_gpuContext->UpdateSubresource(m_flushUniforms.Get(),
                                    0,
                                    NULL,
                                    heap_buffer_contents(flushUniformBufferRing()),
                                    0,
                                    0);

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

        m_gpuContext->DrawInstanced(4, desc.complexGradSpanCount, 0, 0);
    }

    // Copy the simple color ramps to the gradient texture.
    if (desc.simpleGradTexelsHeight > 0)
    {
        assert(desc.simpleGradTexelsHeight * desc.simpleGradTexelsWidth <=
               simpleColorRampsBufferRing()->capacity() * 2);
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
                                        heap_buffer_contents(simpleColorRampsBufferRing()),
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
        ID3D11ShaderResourceView* vsTextureViews[] = {NULL,
                                                      submitted_srv(pathBufferRing()),
                                                      submitted_srv(contourBufferRing())};
        static_assert(TESS_VERTEX_TEXTURE_IDX == 0);
        static_assert(PATH_TEXTURE_IDX == 1);
        static_assert(CONTOUR_TEXTURE_IDX == 2);
        m_gpuContext->VSSetShaderResources(0, std::size(vsTextureViews), vsTextureViews);

        D3D11_VIEWPORT viewport = {0,
                                   0,
                                   static_cast<float>(kTessTextureWidth),
                                   static_cast<float>(desc.tessDataHeight),
                                   0,
                                   1};
        m_gpuContext->RSSetViewports(1, &viewport);

        m_gpuContext->PSSetShader(m_tessellatePixelShader.Get(), NULL, 0);

        m_gpuContext->OMSetRenderTargets(1, m_tessTextureRTV.GetAddressOf(), NULL);

        m_gpuContext->DrawIndexedInstanced(std::size(pls::kTessSpanIndices),
                                           desc.tessVertexSpanCount,
                                           0,
                                           0,
                                           0);

        if (m_isIntel)
        {
            // FIXME! Intel needs this flush! Driver bug? Find a lighter workaround?
            m_gpuContext->Flush();
        }
    }

    auto imageMeshUniformData = static_cast<const pls::ImageMeshUniforms*>(
        heap_buffer_contents(imageMeshUniformBufferRing()));

    // Execute the DrawList.
    ID3D11UnorderedAccessView* plsUAVs[] = {renderTarget->m_targetUAV.Get(),
                                            renderTarget->m_coverageUAV.Get(),
                                            renderTarget->m_originalDstColorUAV.Get(),
                                            renderTarget->m_clipUAV.Get()};
    static_assert(FRAMEBUFFER_PLANE_IDX == 0);
    static_assert(COVERAGE_PLANE_IDX == 1);
    static_assert(ORIGINAL_DST_COLOR_PLANE_IDX == 2);
    static_assert(CLIP_PLANE_IDX == 3);
    m_gpuContext->OMSetRenderTargetsAndUnorderedAccessViews(0,
                                                            NULL,
                                                            NULL,
                                                            0,
                                                            std::size(plsUAVs),
                                                            plsUAVs,
                                                            NULL);

    m_gpuContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ID3D11Buffer* vertexBuffers[] = {m_patchVertexBuffer.Get(),
                                     submitted_buffer(triangleBufferRing())};
    static_assert(kPatchVertexDataSlot == 0);
    static_assert(kTriangleVertexDataSlot == 1);
    UINT vertexStrides[] = {sizeof(PatchVertex), sizeof(TriangleVertex)};
    UINT vertexOffsets[] = {0, 0};
    m_gpuContext->IASetVertexBuffers(0,
                                     desc.hasTriangleVertices ? 2 : 1,
                                     vertexBuffers,
                                     vertexStrides,
                                     vertexOffsets);

    ID3D11ShaderResourceView* vertexTextureViews[] = {m_tessTextureSRV.Get(),
                                                      submitted_srv(pathBufferRing()),
                                                      submitted_srv(contourBufferRing())};
    static_assert(TESS_VERTEX_TEXTURE_IDX == 0);
    static_assert(PATH_TEXTURE_IDX == 1);
    static_assert(CONTOUR_TEXTURE_IDX == 2);
    m_gpuContext->VSSetShaderResources(0, std::size(vertexTextureViews), vertexTextureViews);

    D3D11_VIEWPORT viewport = {0,
                               0,
                               static_cast<float>(renderTarget->width()),
                               static_cast<float>(renderTarget->height()),
                               0,
                               1};
    m_gpuContext->RSSetViewports(1, &viewport);

    m_gpuContext->PSSetConstantBuffers(IMAGE_MESH_UNIFORM_BUFFER_IDX,
                                       1,
                                       m_imageMeshUniforms.GetAddressOf());
    m_gpuContext->PSSetShaderResources(GRAD_TEXTURE_IDX, 1, m_gradTextureSRV.GetAddressOf());

    for (const Draw& draw : *desc.drawList)
    {
        if (draw.elementCount == 0)
        {
            continue;
        }

        DrawType drawType = draw.drawType;
        setPipelineLayoutAndShaders(drawType, draw.shaderFeatures);

        if (auto imageTextureD3D = static_cast<const PLSTextureD3DImpl*>(draw.imageTextureRef))
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
                m_gpuContext->IASetIndexBuffer(m_patchIndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
                m_gpuContext->RSSetState(m_pathRasterState[desc.wireframe].Get());
                DrawUniforms drawUniforms(draw.baseElement);
                m_gpuContext->UpdateSubresource(m_drawUniforms.Get(), 0, NULL, &drawUniforms, 0, 0);
                m_gpuContext->DrawIndexedInstanced(PatchIndexCount(drawType),
                                                   draw.elementCount,
                                                   PatchBaseIndex(drawType),
                                                   0,
                                                   draw.baseElement);
                break;
            }
            case DrawType::interiorTriangulation:
            {
                m_gpuContext->RSSetState(m_pathRasterState[desc.wireframe].Get());
                m_gpuContext->Draw(draw.elementCount, draw.baseElement);
                break;
            }
            case DrawType::imageMesh:
            {
                auto vertexBuffer = static_cast<const RenderBufferD3DImpl*>(draw.vertexBufferRef);
                auto uvBuffer = static_cast<const RenderBufferD3DImpl*>(draw.uvBufferRef);
                auto indexBuffer = static_cast<const RenderBufferD3DImpl*>(draw.indexBufferRef);
                ID3D11Buffer* imageMeshBuffers[] = {vertexBuffer->buffer(), uvBuffer->buffer()};
                UINT imageMeshStrides[] = {sizeof(Vec2D), sizeof(Vec2D)};
                UINT imageMeshOffsets[] = {0, 0};
                m_gpuContext->IASetVertexBuffers(kImageMeshVertexDataSlot,
                                                 2,
                                                 imageMeshBuffers,
                                                 imageMeshStrides,
                                                 imageMeshOffsets);
                static_assert(kImageMeshUVDataSlot == kImageMeshVertexDataSlot + 1);
                m_gpuContext->IASetIndexBuffer(indexBuffer->buffer(), DXGI_FORMAT_R16_UINT, 0);
                m_gpuContext->RSSetState(m_imageMeshRasterState[desc.wireframe].Get());
                m_gpuContext->UpdateSubresource(m_imageMeshUniforms.Get(),
                                                0,
                                                NULL,
                                                imageMeshUniformData++,
                                                0,
                                                0);
                m_gpuContext->DrawIndexed(draw.elementCount, draw.baseElement, 0);
                break;
            }
        }
    }
}
} // namespace rive::pls
