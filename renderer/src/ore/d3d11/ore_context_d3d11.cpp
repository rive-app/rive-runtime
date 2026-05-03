/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/ore/ore_context.hpp"
#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_texture.hpp"
#include "rive/renderer/ore/ore_sampler.hpp"
#include "rive/renderer/ore/ore_shader_module.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/renderer/ore/ore_render_pass.hpp"
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/d3d11/render_context_d3d_impl.hpp"
#include "rive/rive_types.hpp"

#include <d3d11.h>
#include <wrl/client.h>
#include <string>

using Microsoft::WRL::ComPtr;

namespace rive::ore
{

// ============================================================================
// Enum -> DXGI / D3D11 conversion helpers
// ============================================================================

// Format info bundle: resource format (typeless for depth), SRV, RTV, DSV.
struct D3D11FormatInfo
{
    DXGI_FORMAT resource; // Texture resource format (typeless for depth).
    DXGI_FORMAT srv;      // SRV format for sampling.
    DXGI_FORMAT rtv;      // RTV format (UNKNOWN for depth).
    DXGI_FORMAT dsv;      // DSV format (UNKNOWN for color).
    bool isDepth;
};

static D3D11FormatInfo oreFormatInfo(TextureFormat fmt)
{
    switch (fmt)
    {
        case TextureFormat::r8unorm:
            return {DXGI_FORMAT_R8_UNORM,
                    DXGI_FORMAT_R8_UNORM,
                    DXGI_FORMAT_R8_UNORM,
                    DXGI_FORMAT_UNKNOWN,
                    false};
        case TextureFormat::rg8unorm:
            return {DXGI_FORMAT_R8G8_UNORM,
                    DXGI_FORMAT_R8G8_UNORM,
                    DXGI_FORMAT_R8G8_UNORM,
                    DXGI_FORMAT_UNKNOWN,
                    false};
        case TextureFormat::rgba8unorm:
            return {DXGI_FORMAT_R8G8B8A8_UNORM,
                    DXGI_FORMAT_R8G8B8A8_UNORM,
                    DXGI_FORMAT_R8G8B8A8_UNORM,
                    DXGI_FORMAT_UNKNOWN,
                    false};
        case TextureFormat::rgba8snorm:
            return {DXGI_FORMAT_R8G8B8A8_SNORM,
                    DXGI_FORMAT_R8G8B8A8_SNORM,
                    DXGI_FORMAT_R8G8B8A8_SNORM,
                    DXGI_FORMAT_UNKNOWN,
                    false};
        case TextureFormat::bgra8unorm:
            return {DXGI_FORMAT_B8G8R8A8_UNORM,
                    DXGI_FORMAT_B8G8R8A8_UNORM,
                    DXGI_FORMAT_B8G8R8A8_UNORM,
                    DXGI_FORMAT_UNKNOWN,
                    false};
        case TextureFormat::rgba16float:
            return {DXGI_FORMAT_R16G16B16A16_FLOAT,
                    DXGI_FORMAT_R16G16B16A16_FLOAT,
                    DXGI_FORMAT_R16G16B16A16_FLOAT,
                    DXGI_FORMAT_UNKNOWN,
                    false};
        case TextureFormat::rg16float:
            return {DXGI_FORMAT_R16G16_FLOAT,
                    DXGI_FORMAT_R16G16_FLOAT,
                    DXGI_FORMAT_R16G16_FLOAT,
                    DXGI_FORMAT_UNKNOWN,
                    false};
        case TextureFormat::r16float:
            return {DXGI_FORMAT_R16_FLOAT,
                    DXGI_FORMAT_R16_FLOAT,
                    DXGI_FORMAT_R16_FLOAT,
                    DXGI_FORMAT_UNKNOWN,
                    false};
        case TextureFormat::rgba32float:
            return {DXGI_FORMAT_R32G32B32A32_FLOAT,
                    DXGI_FORMAT_R32G32B32A32_FLOAT,
                    DXGI_FORMAT_R32G32B32A32_FLOAT,
                    DXGI_FORMAT_UNKNOWN,
                    false};
        case TextureFormat::rg32float:
            return {DXGI_FORMAT_R32G32_FLOAT,
                    DXGI_FORMAT_R32G32_FLOAT,
                    DXGI_FORMAT_R32G32_FLOAT,
                    DXGI_FORMAT_UNKNOWN,
                    false};
        case TextureFormat::r32float:
            return {DXGI_FORMAT_R32_FLOAT,
                    DXGI_FORMAT_R32_FLOAT,
                    DXGI_FORMAT_R32_FLOAT,
                    DXGI_FORMAT_UNKNOWN,
                    false};
        case TextureFormat::rgb10a2unorm:
            return {DXGI_FORMAT_R10G10B10A2_UNORM,
                    DXGI_FORMAT_R10G10B10A2_UNORM,
                    DXGI_FORMAT_R10G10B10A2_UNORM,
                    DXGI_FORMAT_UNKNOWN,
                    false};
        case TextureFormat::r11g11b10float:
            return {DXGI_FORMAT_R11G11B10_FLOAT,
                    DXGI_FORMAT_R11G11B10_FLOAT,
                    DXGI_FORMAT_R11G11B10_FLOAT,
                    DXGI_FORMAT_UNKNOWN,
                    false};
        // Depth formats: typeless resource so both DSV and SRV are possible.
        case TextureFormat::depth16unorm:
            return {DXGI_FORMAT_R16_TYPELESS,
                    DXGI_FORMAT_R16_UNORM,
                    DXGI_FORMAT_UNKNOWN,
                    DXGI_FORMAT_D16_UNORM,
                    true};
        case TextureFormat::depth24plusStencil8:
            return {DXGI_FORMAT_R24G8_TYPELESS,
                    DXGI_FORMAT_R24_UNORM_X8_TYPELESS,
                    DXGI_FORMAT_UNKNOWN,
                    DXGI_FORMAT_D24_UNORM_S8_UINT,
                    true};
        case TextureFormat::depth32float:
            return {DXGI_FORMAT_R32_TYPELESS,
                    DXGI_FORMAT_R32_FLOAT,
                    DXGI_FORMAT_UNKNOWN,
                    DXGI_FORMAT_D32_FLOAT,
                    true};
        case TextureFormat::depth32floatStencil8:
            return {DXGI_FORMAT_R32G8X24_TYPELESS,
                    DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS,
                    DXGI_FORMAT_UNKNOWN,
                    DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
                    true};
        // Block-compressed (supported on D3D11 feature level 11.0).
        case TextureFormat::bc1unorm:
            return {DXGI_FORMAT_BC1_UNORM,
                    DXGI_FORMAT_BC1_UNORM,
                    DXGI_FORMAT_UNKNOWN,
                    DXGI_FORMAT_UNKNOWN,
                    false};
        case TextureFormat::bc3unorm:
            return {DXGI_FORMAT_BC3_UNORM,
                    DXGI_FORMAT_BC3_UNORM,
                    DXGI_FORMAT_UNKNOWN,
                    DXGI_FORMAT_UNKNOWN,
                    false};
        case TextureFormat::bc7unorm:
            return {DXGI_FORMAT_BC7_UNORM,
                    DXGI_FORMAT_BC7_UNORM,
                    DXGI_FORMAT_UNKNOWN,
                    DXGI_FORMAT_UNKNOWN,
                    false};
        // ETC2/ASTC are not natively supported by D3D11. These are valid
        // `TextureFormat` enum values that a Lua script can legitimately
        // request, so we must NOT crash here — `d3d11MakeTexture`
        // detects the `DXGI_FORMAT_UNKNOWN` resource sentinel and
        // surfaces a clear `setLastError` instead. Per the
        // `feedback_lua_gpu_misuse_validation` rule.
        case TextureFormat::etc2rgb8:
        case TextureFormat::etc2rgba8:
        case TextureFormat::astc4x4:
        case TextureFormat::astc6x6:
        case TextureFormat::astc8x8:
            return {DXGI_FORMAT_UNKNOWN,
                    DXGI_FORMAT_UNKNOWN,
                    DXGI_FORMAT_UNKNOWN,
                    DXGI_FORMAT_UNKNOWN,
                    false};
    }
    // C++ enum cast-from-int — not reachable from Lua (the bridge
    // validates enum strings). Crash here is a programmer error.
    RIVE_UNREACHABLE();
}

static D3D11_FILTER oreFilterToD3D(Filter min,
                                   Filter mag,
                                   Filter mip,
                                   CompareFunction compare,
                                   uint32_t aniso)
{
    bool isCompare = (compare != CompareFunction::none);
    if (aniso > 1)
        return isCompare ? D3D11_FILTER_COMPARISON_ANISOTROPIC
                         : D3D11_FILTER_ANISOTROPIC;

    // Build the filter from min/mag/mip combinations.
    if (min == Filter::linear && mag == Filter::linear && mip == Filter::linear)
        return isCompare ? D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR
                         : D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    if (min == Filter::linear && mag == Filter::linear &&
        mip == Filter::nearest)
        return isCompare ? D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT
                         : D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    if (min == Filter::linear && mag == Filter::nearest &&
        mip == Filter::linear)
        return isCompare
                   ? D3D11_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR
                   : D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
    if (min == Filter::linear && mag == Filter::nearest &&
        mip == Filter::nearest)
        return isCompare ? D3D11_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT
                         : D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT;
    if (min == Filter::nearest && mag == Filter::linear &&
        mip == Filter::linear)
        return isCompare ? D3D11_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR
                         : D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR;
    if (min == Filter::nearest && mag == Filter::linear &&
        mip == Filter::nearest)
        return isCompare
                   ? D3D11_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT
                   : D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
    if (min == Filter::nearest && mag == Filter::nearest &&
        mip == Filter::linear)
        return isCompare ? D3D11_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR
                         : D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR;
    // nearest/nearest/nearest
    return isCompare ? D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT
                     : D3D11_FILTER_MIN_MAG_MIP_POINT;
}

static D3D11_TEXTURE_ADDRESS_MODE oreWrapToD3D(WrapMode mode)
{
    switch (mode)
    {
        case WrapMode::repeat:
            return D3D11_TEXTURE_ADDRESS_WRAP;
        case WrapMode::mirrorRepeat:
            return D3D11_TEXTURE_ADDRESS_MIRROR;
        case WrapMode::clampToEdge:
            return D3D11_TEXTURE_ADDRESS_CLAMP;
    }
    RIVE_UNREACHABLE();
}

static D3D11_COMPARISON_FUNC oreCompareFunctionToD3D(CompareFunction fn)
{
    switch (fn)
    {
        case CompareFunction::none:
        case CompareFunction::never:
            return D3D11_COMPARISON_NEVER;
        case CompareFunction::less:
            return D3D11_COMPARISON_LESS;
        case CompareFunction::equal:
            return D3D11_COMPARISON_EQUAL;
        case CompareFunction::lessEqual:
            return D3D11_COMPARISON_LESS_EQUAL;
        case CompareFunction::greater:
            return D3D11_COMPARISON_GREATER;
        case CompareFunction::notEqual:
            return D3D11_COMPARISON_NOT_EQUAL;
        case CompareFunction::greaterEqual:
            return D3D11_COMPARISON_GREATER_EQUAL;
        case CompareFunction::always:
            return D3D11_COMPARISON_ALWAYS;
    }
    RIVE_UNREACHABLE();
}

static D3D11_BLEND oreBlendFactorToD3D(BlendFactor f)
{
    switch (f)
    {
        case BlendFactor::zero:
            return D3D11_BLEND_ZERO;
        case BlendFactor::one:
            return D3D11_BLEND_ONE;
        case BlendFactor::srcColor:
            return D3D11_BLEND_SRC_COLOR;
        case BlendFactor::oneMinusSrcColor:
            return D3D11_BLEND_INV_SRC_COLOR;
        case BlendFactor::srcAlpha:
            return D3D11_BLEND_SRC_ALPHA;
        case BlendFactor::oneMinusSrcAlpha:
            return D3D11_BLEND_INV_SRC_ALPHA;
        case BlendFactor::dstColor:
            return D3D11_BLEND_DEST_COLOR;
        case BlendFactor::oneMinusDstColor:
            return D3D11_BLEND_INV_DEST_COLOR;
        case BlendFactor::dstAlpha:
            return D3D11_BLEND_DEST_ALPHA;
        case BlendFactor::oneMinusDstAlpha:
            return D3D11_BLEND_INV_DEST_ALPHA;
        case BlendFactor::srcAlphaSaturated:
            return D3D11_BLEND_SRC_ALPHA_SAT;
        case BlendFactor::blendColor:
            return D3D11_BLEND_BLEND_FACTOR;
        case BlendFactor::oneMinusBlendColor:
            return D3D11_BLEND_INV_BLEND_FACTOR;
    }
    RIVE_UNREACHABLE();
}

static D3D11_BLEND_OP oreBlendOpToD3D(BlendOp op)
{
    switch (op)
    {
        case BlendOp::add:
            return D3D11_BLEND_OP_ADD;
        case BlendOp::subtract:
            return D3D11_BLEND_OP_SUBTRACT;
        case BlendOp::reverseSubtract:
            return D3D11_BLEND_OP_REV_SUBTRACT;
        case BlendOp::min:
            return D3D11_BLEND_OP_MIN;
        case BlendOp::max:
            return D3D11_BLEND_OP_MAX;
    }
    RIVE_UNREACHABLE();
}

static D3D11_STENCIL_OP oreStencilOpToD3D(StencilOp op)
{
    switch (op)
    {
        case StencilOp::keep:
            return D3D11_STENCIL_OP_KEEP;
        case StencilOp::zero:
            return D3D11_STENCIL_OP_ZERO;
        case StencilOp::replace:
            return D3D11_STENCIL_OP_REPLACE;
        case StencilOp::incrementClamp:
            return D3D11_STENCIL_OP_INCR_SAT;
        case StencilOp::decrementClamp:
            return D3D11_STENCIL_OP_DECR_SAT;
        case StencilOp::invert:
            return D3D11_STENCIL_OP_INVERT;
        case StencilOp::incrementWrap:
            return D3D11_STENCIL_OP_INCR;
        case StencilOp::decrementWrap:
            return D3D11_STENCIL_OP_DECR;
    }
    RIVE_UNREACHABLE();
}

static D3D11_CULL_MODE oreCullModeToD3D(CullMode mode)
{
    switch (mode)
    {
        case CullMode::none:
            return D3D11_CULL_NONE;
        case CullMode::front:
            return D3D11_CULL_FRONT;
        case CullMode::back:
            return D3D11_CULL_BACK;
    }
    RIVE_UNREACHABLE();
}

static UINT8 oreColorWriteMaskToD3D(ColorWriteMask mask)
{
    UINT8 d3d = 0;
    if ((mask & ColorWriteMask::red) != ColorWriteMask::none)
        d3d |= D3D11_COLOR_WRITE_ENABLE_RED;
    if ((mask & ColorWriteMask::green) != ColorWriteMask::none)
        d3d |= D3D11_COLOR_WRITE_ENABLE_GREEN;
    if ((mask & ColorWriteMask::blue) != ColorWriteMask::none)
        d3d |= D3D11_COLOR_WRITE_ENABLE_BLUE;
    if ((mask & ColorWriteMask::alpha) != ColorWriteMask::none)
        d3d |= D3D11_COLOR_WRITE_ENABLE_ALPHA;
    return d3d;
}

static DXGI_FORMAT oreVertexFormatToDXGI(VertexFormat fmt)
{
    switch (fmt)
    {
        case VertexFormat::float1:
            return DXGI_FORMAT_R32_FLOAT;
        case VertexFormat::float2:
            return DXGI_FORMAT_R32G32_FLOAT;
        case VertexFormat::float3:
            return DXGI_FORMAT_R32G32B32_FLOAT;
        case VertexFormat::float4:
            return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case VertexFormat::uint8x4:
            return DXGI_FORMAT_R8G8B8A8_UINT;
        case VertexFormat::sint8x4:
            return DXGI_FORMAT_R8G8B8A8_SINT;
        case VertexFormat::unorm8x4:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        case VertexFormat::snorm8x4:
            return DXGI_FORMAT_R8G8B8A8_SNORM;
        case VertexFormat::uint16x2:
            return DXGI_FORMAT_R16G16_UINT;
        case VertexFormat::sint16x2:
            return DXGI_FORMAT_R16G16_SINT;
        case VertexFormat::unorm16x2:
            return DXGI_FORMAT_R16G16_UNORM;
        case VertexFormat::snorm16x2:
            return DXGI_FORMAT_R16G16_SNORM;
        case VertexFormat::uint16x4:
            return DXGI_FORMAT_R16G16B16A16_UINT;
        case VertexFormat::sint16x4:
            return DXGI_FORMAT_R16G16B16A16_SINT;
        case VertexFormat::float16x2:
            return DXGI_FORMAT_R16G16_FLOAT;
        case VertexFormat::float16x4:
            return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case VertexFormat::uint32:
            return DXGI_FORMAT_R32_UINT;
        case VertexFormat::uint32x2:
            return DXGI_FORMAT_R32G32_UINT;
        case VertexFormat::uint32x3:
            return DXGI_FORMAT_R32G32B32_UINT;
        case VertexFormat::uint32x4:
            return DXGI_FORMAT_R32G32B32A32_UINT;
        case VertexFormat::sint32:
            return DXGI_FORMAT_R32_SINT;
        case VertexFormat::sint32x2:
            return DXGI_FORMAT_R32G32_SINT;
        case VertexFormat::sint32x3:
            return DXGI_FORMAT_R32G32B32_SINT;
        case VertexFormat::sint32x4:
            return DXGI_FORMAT_R32G32B32A32_SINT;
    }
    RIVE_UNREACHABLE();
}

// ============================================================================
// D3D11 helper methods — always compiled (used by combined d3d11+d3d12
// dispatch file).
// ============================================================================

// ============================================================================
// d3d11MakeBuffer
// ============================================================================

rcp<Buffer> Context::d3d11MakeBuffer(const BufferDesc& desc)
{
    if (desc.immutable && desc.data == nullptr)
    {
        setLastError(
            "makeBuffer: immutable=true requires `data` (the buffer is "
            "GPU-only after creation)");
        return nullptr;
    }

    auto buffer = rcp<Buffer>(new Buffer(desc.size, desc.usage));
    buffer->m_d3d11Context = m_d3d11Context.Get();

    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth = desc.size;
    if (desc.immutable)
    {
        // GPU-read-only, populated from initData. Fastest path on AMD —
        // no CPU access flag means no shadow copy.
        bd.Usage = D3D11_USAGE_IMMUTABLE;
        bd.CPUAccessFlags = 0;
    }
    else
    {
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    }

    switch (desc.usage)
    {
        case BufferUsage::vertex:
            bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            break;
        case BufferUsage::index:
            bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
            break;
        case BufferUsage::uniform:
            // Constant buffers must be a multiple of 16 bytes.
            bd.ByteWidth = (desc.size + 15u) & ~15u;
            bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            break;
    }

    D3D11_SUBRESOURCE_DATA initData{};
    initData.pSysMem = desc.data;

    HRESULT hr = m_d3d11Device->CreateBuffer(
        &bd,
        desc.data ? &initData : nullptr,
        buffer->m_d3d11Buffer.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        setLastError("makeBuffer: CreateBuffer failed (hr=0x%08x, size=%u)",
                     static_cast<unsigned>(hr),
                     desc.size);
        return nullptr;
    }

    // Tag with the user's label so RenderDoc / PIX captures show it.
    if (desc.label != nullptr)
    {
        buffer->m_d3d11Buffer->SetPrivateData(
            WKPDID_D3DDebugObjectName,
            static_cast<UINT>(strlen(desc.label)),
            desc.label);
    }

    return buffer;
}

// ============================================================================
// d3d11MakeTexture
// ============================================================================

rcp<Texture> Context::d3d11MakeTexture(const TextureDesc& desc)
{
    const D3D11FormatInfo fmtInfo = oreFormatInfo(desc.format);
    // ETC2 / ASTC have no native D3D11 path; `oreFormatInfo` returns
    // an all-`DXGI_FORMAT_UNKNOWN` sentinel for those rather than
    // crashing. Surface a clear error so a Lua script that asks for
    // these on Windows gets a useful message instead of a generic null
    // return.
    if (fmtInfo.resource == DXGI_FORMAT_UNKNOWN)
    {
        setLastError("makeTexture: format %u (etc2*/astc*) has no native "
                     "D3D11 mapping; software-decode to rgba8unorm before "
                     "upload, or feature-gate via Features::etc2 / ::astc",
                     static_cast<unsigned>(desc.format));
        return nullptr;
    }

    auto texture = rcp<Texture>(new Texture(desc));
    texture->m_d3d11Context = m_d3d11Context.Get();

    UINT bindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (fmtInfo.isDepth)
        bindFlags |= D3D11_BIND_DEPTH_STENCIL;
    else if (desc.renderTarget)
        bindFlags |= D3D11_BIND_RENDER_TARGET;

    if (desc.type == TextureType::texture3D)
    {
        D3D11_TEXTURE3D_DESC td{};
        td.Width = desc.width;
        td.Height = desc.height;
        td.Depth = desc.depthOrArrayLayers;
        td.MipLevels = desc.numMipmaps;
        td.Format = fmtInfo.resource;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = bindFlags;

        ComPtr<ID3D11Texture3D> tex3d;
        HRESULT hr =
            m_d3d11Device->CreateTexture3D(&td, nullptr, tex3d.GetAddressOf());
        if (FAILED(hr))
            return nullptr;
        texture->m_d3d11Texture = tex3d;
    }
    else
    {
        D3D11_TEXTURE2D_DESC td{};
        td.Width = desc.width;
        td.Height = desc.height;
        td.MipLevels = desc.numMipmaps;
        td.Format = fmtInfo.resource;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = bindFlags;
        td.SampleDesc.Count = desc.sampleCount;
        td.SampleDesc.Quality = 0;

        if (desc.type == TextureType::cube)
        {
            td.ArraySize = 6;
            td.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
        }
        else if (desc.type == TextureType::array2D)
        {
            td.ArraySize = desc.depthOrArrayLayers;
        }
        else
        {
            td.ArraySize = 1;
        }

        ComPtr<ID3D11Texture2D> tex2d;
        HRESULT hr =
            m_d3d11Device->CreateTexture2D(&td, nullptr, tex2d.GetAddressOf());
        if (FAILED(hr))
            return nullptr;
        texture->m_d3d11Texture = tex2d;
    }

    // Tag with the user's label so RenderDoc / PIX captures show it.
    if (desc.label != nullptr && texture->m_d3d11Texture)
    {
        texture->m_d3d11Texture->SetPrivateData(
            WKPDID_D3DDebugObjectName,
            static_cast<UINT>(strlen(desc.label)),
            desc.label);
    }

    return texture;
}

// ============================================================================
// d3d11MakeTextureView
// ============================================================================

rcp<TextureView> Context::d3d11MakeTextureView(const TextureViewDesc& desc)
{
    Texture* tex = desc.texture;
    if (!tex)
        return nullptr;

    auto view = rcp<TextureView>(new TextureView(ref_rcp(tex), desc));

    const D3D11FormatInfo fmtInfo = oreFormatInfo(tex->format());

    // --- Shader Resource View ---
    if (fmtInfo.srv != DXGI_FORMAT_UNKNOWN)
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = fmtInfo.srv;

        switch (desc.dimension)
        {
            case TextureViewDimension::texture2D:
                if (tex->sampleCount() > 1)
                {
                    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;
                }
                else if (desc.layerCount > 1)
                {
                    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
                    srvDesc.Texture2DArray.MostDetailedMip = desc.baseMipLevel;
                    srvDesc.Texture2DArray.MipLevels = desc.mipCount;
                    srvDesc.Texture2DArray.FirstArraySlice = desc.baseLayer;
                    srvDesc.Texture2DArray.ArraySize = desc.layerCount;
                }
                else
                {
                    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                    srvDesc.Texture2D.MostDetailedMip = desc.baseMipLevel;
                    srvDesc.Texture2D.MipLevels = desc.mipCount;
                }
                break;
            case TextureViewDimension::cube:
                srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
                srvDesc.TextureCube.MostDetailedMip = desc.baseMipLevel;
                srvDesc.TextureCube.MipLevels = desc.mipCount;
                break;
            case TextureViewDimension::cubeArray:
                srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
                srvDesc.TextureCubeArray.MostDetailedMip = desc.baseMipLevel;
                srvDesc.TextureCubeArray.MipLevels = desc.mipCount;
                srvDesc.TextureCubeArray.First2DArrayFace = desc.baseLayer;
                srvDesc.TextureCubeArray.NumCubes = desc.layerCount / 6;
                break;
            case TextureViewDimension::texture3D:
                srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
                srvDesc.Texture3D.MostDetailedMip = desc.baseMipLevel;
                srvDesc.Texture3D.MipLevels = desc.mipCount;
                break;
            case TextureViewDimension::array2D:
                srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
                srvDesc.Texture2DArray.MostDetailedMip = desc.baseMipLevel;
                srvDesc.Texture2DArray.MipLevels = desc.mipCount;
                srvDesc.Texture2DArray.FirstArraySlice = desc.baseLayer;
                srvDesc.Texture2DArray.ArraySize = desc.layerCount;
                break;
        }

        m_d3d11Device->CreateShaderResourceView(
            tex->m_d3d11Texture.Get(),
            &srvDesc,
            view->m_d3dSRV.ReleaseAndGetAddressOf());
    }

    // --- Render Target View (color) ---
    if (!fmtInfo.isDepth && tex->isRenderTarget() &&
        fmtInfo.rtv != DXGI_FORMAT_UNKNOWN)
    {
        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc{};
        rtvDesc.Format = fmtInfo.rtv;

        switch (desc.dimension)
        {
            case TextureViewDimension::texture2D:
                if (tex->sampleCount() > 1)
                {
                    rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
                }
                // Use TEXTURE2DARRAY whenever targeting a specific array slice
                // (baseLayer > 0) or multiple layers, because
                // D3D11_RTV_DIMENSION_TEXTURE2D can only address slice 0.
                // This covers cube-face RTVs where each face is a slice.
                else if (desc.layerCount > 1 || desc.baseLayer > 0)
                {
                    rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
                    rtvDesc.Texture2DArray.MipSlice = desc.baseMipLevel;
                    rtvDesc.Texture2DArray.FirstArraySlice = desc.baseLayer;
                    rtvDesc.Texture2DArray.ArraySize = desc.layerCount;
                }
                else
                {
                    rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
                    rtvDesc.Texture2D.MipSlice = desc.baseMipLevel;
                }
                break;
            case TextureViewDimension::cube:
            case TextureViewDimension::cubeArray:
            case TextureViewDimension::array2D:
                rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
                rtvDesc.Texture2DArray.MipSlice = desc.baseMipLevel;
                rtvDesc.Texture2DArray.FirstArraySlice = desc.baseLayer;
                rtvDesc.Texture2DArray.ArraySize = desc.layerCount;
                break;
            case TextureViewDimension::texture3D:
                rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
                rtvDesc.Texture3D.MipSlice = desc.baseMipLevel;
                rtvDesc.Texture3D.FirstWSlice = desc.baseLayer;
                rtvDesc.Texture3D.WSize = desc.layerCount;
                break;
        }

        m_d3d11Device->CreateRenderTargetView(
            tex->m_d3d11Texture.Get(),
            &rtvDesc,
            view->m_d3dRTV.ReleaseAndGetAddressOf());
    }

    // --- Depth Stencil View ---
    if (fmtInfo.isDepth && fmtInfo.dsv != DXGI_FORMAT_UNKNOWN)
    {
        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
        dsvDesc.Format = fmtInfo.dsv;
        if (tex->sampleCount() > 1)
        {
            dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
        }
        else if (desc.dimension == TextureViewDimension::array2D ||
                 (desc.dimension == TextureViewDimension::texture2D &&
                  (desc.layerCount > 1 || desc.baseLayer > 0)))
        {
            dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
            dsvDesc.Texture2DArray.MipSlice = desc.baseMipLevel;
            dsvDesc.Texture2DArray.FirstArraySlice = desc.baseLayer;
            dsvDesc.Texture2DArray.ArraySize = desc.layerCount;
        }
        else
        {
            dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
            dsvDesc.Texture2D.MipSlice = desc.baseMipLevel;
        }

        m_d3d11Device->CreateDepthStencilView(
            tex->m_d3d11Texture.Get(),
            &dsvDesc,
            view->m_d3dDSV.ReleaseAndGetAddressOf());
    }

    return view;
}

// ============================================================================
// d3d11MakeSampler
// ============================================================================

rcp<Sampler> Context::d3d11MakeSampler(const SamplerDesc& desc)
{
    D3D11_SAMPLER_DESC sd{};
    sd.Filter = oreFilterToD3D(desc.minFilter,
                               desc.magFilter,
                               desc.mipmapFilter,
                               desc.compare,
                               desc.maxAnisotropy);
    sd.AddressU = oreWrapToD3D(desc.wrapU);
    sd.AddressV = oreWrapToD3D(desc.wrapV);
    sd.AddressW = oreWrapToD3D(desc.wrapW);
    sd.MipLODBias = 0.0f;
    sd.MaxAnisotropy = desc.maxAnisotropy;
    sd.ComparisonFunc = (desc.compare != CompareFunction::none)
                            ? oreCompareFunctionToD3D(desc.compare)
                            : D3D11_COMPARISON_NEVER;
    sd.MinLOD = desc.minLod;
    sd.MaxLOD = desc.maxLod;

    auto sampler = rcp<Sampler>(new Sampler());
    HRESULT hr = m_d3d11Device->CreateSamplerState(
        &sd,
        sampler->m_d3dSampler.ReleaseAndGetAddressOf());
    if (FAILED(hr))
        return nullptr;
    return sampler;
}

// ============================================================================
// d3d11MakeShaderModule
// ============================================================================

rcp<ShaderModule> Context::d3d11MakeShaderModule(const ShaderModuleDesc& desc)
{
    auto module = rcp<ShaderModule>(new ShaderModule());
    module->m_stage = desc.stage;

    // Store HLSL source for runtime D3DCompile if provided. AMD drivers
    // require DXBC to be compiled in the same process context where it's
    // used — pre-compiled DXBC from a build step causes driver crashes.
    if (desc.hlslSource && desc.hlslSourceSize > 0)
    {
        module->m_hlslSource =
            std::string(desc.hlslSource, desc.hlslSourceSize);
        if (desc.hlslEntryPoint)
            module->m_hlslEntryPoint = desc.hlslEntryPoint;
    }
    else if (desc.code && desc.codeSize > 0)
    {
        const uint8_t* code = static_cast<const uint8_t*>(desc.code);
        module->m_bytecode.assign(code, code + desc.codeSize);
    }

    module->applyBindingMapFromDesc(desc);
    return module;
}

// ============================================================================
// d3d11MakePipeline
// ============================================================================

rcp<Pipeline> Context::d3d11MakePipeline(const PipelineDesc& desc,
                                         std::string* outError)
{
    auto pipeline = rcp<Pipeline>(new Pipeline(desc));

    // --- Validate user-supplied layouts against shader binding map ---
    {
        std::string err;
        if (!validateLayoutsAgainstBindingMap(pipeline->m_bindingMap,
                                              desc.bindGroupLayouts,
                                              desc.bindGroupLayoutCount,
                                              &err) ||
            !validateColorRequiresFragment(desc.colorCount,
                                           desc.fragmentModule != nullptr,
                                           &err))
        {
            if (outError)
                *outError = err;
            else
                setLastError("makePipeline: %s", err.c_str());
            return nullptr;
        }
    }

    // Keep ShaderModules alive for the lifetime of the Pipeline.
    // The Luau GC can destroy the ScriptedShader (which owns the
    // ShaderModule rcp) at any time after script init.
    pipeline->m_vsModule = ref_rcp(desc.vertexModule);
    if (desc.fragmentModule != nullptr)
        pipeline->m_psModule = ref_rcp(desc.fragmentModule);

    std::string compileErr;
    desc.vertexModule->ensureD3DShaders(m_d3d11Device.Get(), &compileErr);
    if (!compileErr.empty())
    {
        setLastError("Ore D3D11 VS: %s", compileErr.c_str());
        if (outError)
            *outError = m_lastError;
        return nullptr;
    }
    if (desc.fragmentModule != nullptr)
    {
        desc.fragmentModule->ensureD3DShaders(m_d3d11Device.Get(), &compileErr);
        if (!compileErr.empty())
        {
            setLastError("Ore D3D11 PS: %s", compileErr.c_str());
            if (outError)
                *outError = m_lastError;
            return nullptr;
        }
    }

    pipeline->m_d3dVS = desc.vertexModule->m_d3dVertexShader.Get();
    if (desc.fragmentModule != nullptr)
        pipeline->m_d3dPS = desc.fragmentModule->m_d3dPixelShader.Get();

    if (!pipeline->m_d3dVS ||
        (desc.fragmentModule != nullptr && !pipeline->m_d3dPS))
    {
        setLastError("Ore D3D11: %s",
                     !pipeline->m_d3dVS
                         ? "vertexModule was not created as a vertex shader"
                         : "fragmentModule was not created as a pixel shader");
        if (outError)
            *outError = m_lastError;
        return nullptr;
    }

    // --- Input Layout ---
    // Semantic name "TEXCOORD" with SemanticIndex == shaderSlot is the
    // convention used by SPIRV-Cross when targeting HLSL.
    if (desc.vertexBufferCount > 0)
    {
        // Collect elements across all vertex buffers.
        D3D11_INPUT_ELEMENT_DESC elements[32];
        UINT elementCount = 0;

        for (uint32_t bufIdx = 0; bufIdx < desc.vertexBufferCount; ++bufIdx)
        {
            const auto& layout = desc.vertexBuffers[bufIdx];
            for (uint32_t attrIdx = 0; attrIdx < layout.attributeCount;
                 ++attrIdx)
            {
                assert(elementCount < std::size(elements));
                const auto& attr = layout.attributes[attrIdx];
                D3D11_INPUT_ELEMENT_DESC& el = elements[elementCount++];
                el.SemanticName = "TEXCOORD";
                el.SemanticIndex = attr.shaderSlot;
                el.Format = oreVertexFormatToDXGI(attr.format);
                el.InputSlot = bufIdx;
                el.AlignedByteOffset = attr.offset;
                el.InputSlotClass = layout.stepMode == VertexStepMode::instance
                                        ? D3D11_INPUT_PER_INSTANCE_DATA
                                        : D3D11_INPUT_PER_VERTEX_DATA;
                el.InstanceDataStepRate =
                    layout.stepMode == VertexStepMode::instance ? 1 : 0;
            }
        }

        const auto& vsBytecode = desc.vertexModule->m_bytecode;
        HRESULT hr = m_d3d11Device->CreateInputLayout(
            elements,
            elementCount,
            vsBytecode.data(),
            vsBytecode.size(),
            pipeline->m_d3dInputLayout.ReleaseAndGetAddressOf());
        if (FAILED(hr))
        {
            if (outError)
                *outError = "CreateInputLayout failed (HRESULT " +
                            std::to_string(hr) + ")";
            return nullptr;
        }
    }

    // --- Rasterizer State ---
    {
        D3D11_RASTERIZER_DESC rd{};
        rd.FillMode = D3D11_FILL_SOLID;
        rd.CullMode = oreCullModeToD3D(desc.cullMode);
        rd.FrontCounterClockwise =
            (desc.winding == FaceWinding::counterClockwise) ? TRUE : FALSE;
        rd.DepthBias = desc.depthStencil.depthBias;
        rd.DepthBiasClamp = desc.depthStencil.depthBiasClamp;
        rd.SlopeScaledDepthBias = desc.depthStencil.depthBiasSlopeScale;
        rd.DepthClipEnable = TRUE;
        rd.ScissorEnable =
            TRUE; // Always enable scissor; set full rect when not clipping.
        rd.MultisampleEnable = (desc.sampleCount > 1) ? TRUE : FALSE;
        rd.AntialiasedLineEnable = FALSE;

        HRESULT hr = m_d3d11Device->CreateRasterizerState(
            &rd,
            pipeline->m_d3dRasterizer.ReleaseAndGetAddressOf());
        if (FAILED(hr))
        {
            if (outError)
                *outError = "CreateRasterizerState failed (HRESULT " +
                            std::to_string(hr) + ")";
            return nullptr;
        }
    }

    // --- Depth/Stencil State ---
    {
        D3D11_DEPTH_STENCIL_DESC dsd{};
        dsd.DepthEnable =
            (desc.depthStencil.depthCompare != CompareFunction::always ||
             desc.depthStencil.depthWriteEnabled)
                ? TRUE
                : FALSE;
        dsd.DepthWriteMask = desc.depthStencil.depthWriteEnabled
                                 ? D3D11_DEPTH_WRITE_MASK_ALL
                                 : D3D11_DEPTH_WRITE_MASK_ZERO;
        dsd.DepthFunc = oreCompareFunctionToD3D(desc.depthStencil.depthCompare);

        bool hasStencil =
            (desc.stencilFront.compare != CompareFunction::always ||
             desc.stencilFront.failOp != StencilOp::keep ||
             desc.stencilFront.depthFailOp != StencilOp::keep ||
             desc.stencilFront.passOp != StencilOp::keep ||
             desc.stencilBack.compare != CompareFunction::always ||
             desc.stencilBack.failOp != StencilOp::keep ||
             desc.stencilBack.depthFailOp != StencilOp::keep ||
             desc.stencilBack.passOp != StencilOp::keep);

        dsd.StencilEnable = hasStencil ? TRUE : FALSE;
        dsd.StencilReadMask = desc.stencilReadMask;
        dsd.StencilWriteMask = desc.stencilWriteMask;

        auto fillStencilOp = [](D3D11_DEPTH_STENCILOP_DESC& out,
                                const StencilFaceState& s) {
            out.StencilFailOp = oreStencilOpToD3D(s.failOp);
            out.StencilDepthFailOp = oreStencilOpToD3D(s.depthFailOp);
            out.StencilPassOp = oreStencilOpToD3D(s.passOp);
            out.StencilFunc = oreCompareFunctionToD3D(s.compare);
        };
        fillStencilOp(dsd.FrontFace, desc.stencilFront);
        fillStencilOp(dsd.BackFace, desc.stencilBack);

        HRESULT hr = m_d3d11Device->CreateDepthStencilState(
            &dsd,
            pipeline->m_d3dDepthStencil.ReleaseAndGetAddressOf());
        if (FAILED(hr))
        {
            if (outError)
                *outError = "CreateDepthStencilState failed (HRESULT " +
                            std::to_string(hr) + ")";
            return nullptr;
        }
    }

    // --- Blend State ---
    {
        D3D11_BLEND_DESC bd{};
        bd.AlphaToCoverageEnable = FALSE;
        bd.IndependentBlendEnable = (desc.colorCount > 1) ? TRUE : FALSE;

        for (uint32_t i = 0; i < desc.colorCount; ++i)
        {
            const auto& ct = desc.colorTargets[i];
            auto& rt = bd.RenderTarget[i];

            rt.BlendEnable = ct.blendEnabled ? TRUE : FALSE;
            rt.SrcBlend = oreBlendFactorToD3D(ct.blend.srcColor);
            rt.DestBlend = oreBlendFactorToD3D(ct.blend.dstColor);
            rt.BlendOp = oreBlendOpToD3D(ct.blend.colorOp);
            rt.SrcBlendAlpha = oreBlendFactorToD3D(ct.blend.srcAlpha);
            rt.DestBlendAlpha = oreBlendFactorToD3D(ct.blend.dstAlpha);
            rt.BlendOpAlpha = oreBlendOpToD3D(ct.blend.alphaOp);
            rt.RenderTargetWriteMask = oreColorWriteMaskToD3D(ct.writeMask);
        }

        HRESULT hr = m_d3d11Device->CreateBlendState(
            &bd,
            pipeline->m_d3dBlend.ReleaseAndGetAddressOf());
        if (FAILED(hr))
        {
            if (outError)
                *outError = "CreateBlendState failed (HRESULT " +
                            std::to_string(hr) + ")";
            return nullptr;
        }
    }

    return pipeline;
}

// ============================================================================
// d3d11MakeBindGroup
// ============================================================================

rcp<BindGroup> Context::d3d11MakeBindGroup(const BindGroupDesc& desc)
{
    if (desc.layout == nullptr)
    {
        setLastError("makeBindGroup: BindGroupDesc::layout is null");
        return nullptr;
    }
    BindGroupLayout* layout = desc.layout;
    const uint32_t groupIndex = layout->groupIndex();
    if (groupIndex >= kMaxBindGroups)
    {
        setLastError("makeBindGroup: layout->groupIndex %u out of range",
                     groupIndex);
        return nullptr;
    }

    auto bg = rcp<BindGroup>(new BindGroup());
    bg->m_context = this;
    bg->m_layoutRef = ref_rcp(layout);

    // D3D11 has separate VS / PS register namespaces (b0 in VS vs b0 in PS
    // are independent). HLSL SM5.0 has no register spaces, so all groups
    // flatten into a single register namespace per kind. The allocator's
    // per-stage native slots are populated on the layout entries by
    // `makeLayoutFromShader`.
    auto lookupStages = [&](uint32_t binding,
                            BindingKind expected,
                            uint16_t* outVS,
                            uint16_t* outPS) -> bool {
        const BindGroupLayoutEntry* le = layout->findEntry(binding);
        if (le == nullptr)
        {
            setLastError("makeBindGroup: (group=%u, binding=%u) not declared "
                         "in BindGroupLayout",
                         groupIndex,
                         binding);
            return false;
        }
        const bool kindOK = le->kind == expected ||
                            ((le->kind == BindingKind::sampler ||
                              le->kind == BindingKind::comparisonSampler) &&
                             (expected == BindingKind::sampler ||
                              expected == BindingKind::comparisonSampler));
        if (!kindOK)
        {
            setLastError("makeBindGroup: (group=%u, binding=%u) layout kind "
                         "mismatch",
                         groupIndex,
                         binding);
            return false;
        }
        *outVS = (le->nativeSlotVS == BindGroupLayoutEntry::kNativeSlotAbsent)
                     ? BindingMap::kAbsent
                     : static_cast<uint16_t>(le->nativeSlotVS);
        *outPS = (le->nativeSlotFS == BindGroupLayoutEntry::kNativeSlotAbsent)
                     ? BindingMap::kAbsent
                     : static_cast<uint16_t>(le->nativeSlotFS);
        if (*outVS == BindingMap::kAbsent && *outPS == BindingMap::kAbsent)
        {
            setLastError("makeBindGroup: (group=%u, binding=%u) layout has "
                         "no resolved native slot — call "
                         "makeLayoutFromShader",
                         groupIndex,
                         binding);
            return false;
        }
        return true;
    };

    // UBO bindings.
    uint32_t nBufs = std::min(desc.uboCount, 8u);
    bg->m_d3d11UBOs.reserve(nBufs);
    for (uint32_t i = 0; i < nBufs; ++i)
    {
        const auto& entry = desc.ubos[i];
        BindGroup::D3D11UBOBinding binding{};
        binding.buffer = entry.buffer->m_d3d11Buffer.Get();
        binding.binding = entry.slot;
        if (!lookupStages(entry.slot,
                          BindingKind::uniformBuffer,
                          &binding.vsSlot,
                          &binding.psSlot))
            continue;
        binding.offset = entry.offset;
        binding.size = (entry.size != 0)
                           ? entry.size
                           : (entry.buffer->size() - entry.offset);
        binding.hasDynamicOffset = layout->hasDynamicOffset(entry.slot);
        if (binding.hasDynamicOffset)
            bg->m_dynamicOffsetCount++;
        bg->m_d3d11UBOs.push_back(binding);
        bg->m_retainedBuffers.push_back(ref_rcp(entry.buffer));
    }
    // Sort UBOs by WGSL @binding so `dynamicOffsets[]` is consumed in
    // BindGroupLayout-entry order regardless of caller-supplied
    // `desc.ubos[]` ordering (WebGPU contract; matches Dawn).
    std::sort(bg->m_d3d11UBOs.begin(),
              bg->m_d3d11UBOs.end(),
              [](const BindGroup::D3D11UBOBinding& a,
                 const BindGroup::D3D11UBOBinding& b) {
                  return a.binding < b.binding;
              });

    // Texture bindings.
    uint32_t nTexs = std::min(desc.textureCount, 8u);
    bg->m_d3d11Textures.reserve(nTexs);
    for (uint32_t i = 0; i < nTexs; ++i)
    {
        const auto& entry = desc.textures[i];
        BindGroup::D3D11TexBinding binding{};
        binding.srv = entry.view->m_d3dSRV.Get();
        if (!lookupStages(entry.slot,
                          BindingKind::sampledTexture,
                          &binding.vsSlot,
                          &binding.psSlot))
            continue;
        bg->m_d3d11Textures.push_back(binding);
        bg->m_retainedViews.push_back(ref_rcp(entry.view));
    }

    // Sampler bindings.
    uint32_t nSamps = std::min(desc.samplerCount, 8u);
    bg->m_d3d11Samplers.reserve(nSamps);
    for (uint32_t i = 0; i < nSamps; ++i)
    {
        const auto& entry = desc.samplers[i];
        BindGroup::D3D11SamplerBinding binding{};
        binding.sampler = entry.sampler->m_d3dSampler.Get();
        if (!lookupStages(entry.slot,
                          BindingKind::sampler,
                          &binding.vsSlot,
                          &binding.psSlot))
            continue;
        bg->m_d3d11Samplers.push_back(binding);
        bg->m_retainedSamplers.push_back(ref_rcp(entry.sampler));
    }

    return bg;
}

// ============================================================================
// d3d11MakeBindGroupLayout
// ============================================================================

rcp<BindGroupLayout> Context::d3d11MakeBindGroupLayout(
    const BindGroupLayoutDesc& desc)
{
    if (desc.groupIndex >= kMaxBindGroups)
    {
        setLastError("makeBindGroupLayout: groupIndex %u out of range [0, %u)",
                     desc.groupIndex,
                     kMaxBindGroups);
        return nullptr;
    }
    auto layout = rcp<BindGroupLayout>(new BindGroupLayout());
    // D3D11 layouts use immediate-delete (no deferred destroy needed —
    // no GPU handle to outlive a command list). Leave m_context = nullptr.
    layout->m_groupIndex = desc.groupIndex;
    layout->m_entries.reserve(desc.entryCount);
    for (uint32_t i = 0; i < desc.entryCount; ++i)
        layout->m_entries.push_back(desc.entries[i]);
    return layout;
}

// ============================================================================
// d3d11BeginRenderPass
// ============================================================================

RenderPass Context::d3d11BeginRenderPass(const RenderPassDesc& desc,
                                         std::string* outError)
{
    // Collect RTVs and DSV.
    ID3D11RenderTargetView* rtvs[4] = {};
    for (uint32_t i = 0; i < desc.colorCount; ++i)
    {
        rtvs[i] = desc.colorAttachments[i].view->m_d3dRTV.Get();
    }

    ID3D11DepthStencilView* dsv = desc.depthStencil.view
                                      ? desc.depthStencil.view->m_d3dDSV.Get()
                                      : nullptr;

    m_d3d11Context->OMSetRenderTargets(desc.colorCount, rtvs, dsv);

    // Handle clear ops.
    for (uint32_t i = 0; i < desc.colorCount; ++i)
    {
        const auto& ca = desc.colorAttachments[i];
        if (ca.loadOp == LoadOp::clear && rtvs[i] != nullptr)
        {
            float clearColor[4] = {ca.clearColor.r,
                                   ca.clearColor.g,
                                   ca.clearColor.b,
                                   ca.clearColor.a};
            m_d3d11Context->ClearRenderTargetView(rtvs[i], clearColor);
        }
    }

    if (dsv && desc.depthStencil.view)
    {
        UINT clearFlags = 0;
        if (desc.depthStencil.depthLoadOp == LoadOp::clear)
            clearFlags |= D3D11_CLEAR_DEPTH;

        TextureFormat depthFmt = desc.depthStencil.view->texture()->format();
        bool hasStencil = (depthFmt == TextureFormat::depth24plusStencil8 ||
                           depthFmt == TextureFormat::depth32floatStencil8);
        if (hasStencil && desc.depthStencil.stencilLoadOp == LoadOp::clear)
            clearFlags |= D3D11_CLEAR_STENCIL;

        if (clearFlags)
        {
            m_d3d11Context->ClearDepthStencilView(
                dsv,
                clearFlags,
                desc.depthStencil.depthClearValue,
                static_cast<UINT8>(desc.depthStencil.stencilClearValue));
        }
    }

    // Set a default viewport covering the full render target. D3D11
    // initialises the viewport to {0,0,0,0} which clips all geometry.
    // Metal defaults to the full attachment size, so scripts that omit
    // setViewport() work on Metal but would produce empty output on D3D11
    // without this.
    {
        uint32_t w = 0, h = 0;
        if (desc.colorCount > 0 && desc.colorAttachments[0].view)
        {
            w = desc.colorAttachments[0].view->texture()->width();
            h = desc.colorAttachments[0].view->texture()->height();
        }
        else if (desc.depthStencil.view)
        {
            w = desc.depthStencil.view->texture()->width();
            h = desc.depthStencil.view->texture()->height();
        }
        if (w > 0 && h > 0)
        {
            D3D11_VIEWPORT vp{};
            vp.Width = static_cast<float>(w);
            vp.Height = static_cast<float>(h);
            vp.MinDepth = 0.0f;
            vp.MaxDepth = 1.0f;
            m_d3d11Context->RSSetViewports(1, &vp);
            D3D11_RECT scissor = {0, 0, (LONG)w, (LONG)h};
            m_d3d11Context->RSSetScissorRects(1, &scissor);
        }
    }

    RenderPass pass;
    pass.m_context = this;
    pass.m_d3d11Context = m_d3d11Context.Get();
    pass.populateAttachmentMetadata(desc);

    // Record MSAA resolve pairs for finish(). Subresource index follows
    // D3D11's formula: mipLevel + (arraySlice * mipCount).
    pass.m_d3d11ResolveCount = 0;
    for (uint32_t i = 0; i < desc.colorCount; ++i)
    {
        const auto& ca = desc.colorAttachments[i];
        if (ca.resolveTarget && ca.view)
        {
            auto& entry = pass.m_d3d11Resolves[pass.m_d3d11ResolveCount++];
            entry.msaa = ca.view->texture()->m_d3d11Texture.Get();
            entry.resolve = ca.resolveTarget->texture()->m_d3d11Texture.Get();
            entry.format =
                oreFormatInfo(ca.resolveTarget->texture()->format()).resource;
            const uint32_t msaaMipCount = ca.view->texture()->numMipmaps();
            const uint32_t resolveMipCount =
                ca.resolveTarget->texture()->numMipmaps();
            entry.msaaSubresource =
                ca.view->baseMipLevel() + ca.view->baseLayer() * msaaMipCount;
            entry.resolveSubresource =
                ca.resolveTarget->baseMipLevel() +
                ca.resolveTarget->baseLayer() * resolveMipCount;
        }
    }

    return pass;
}

// ============================================================================
// d3d11WrapCanvasTexture
// ============================================================================

rcp<TextureView> Context::d3d11WrapCanvasTexture(gpu::RenderCanvas* canvas)
{
    assert(canvas != nullptr);

    auto* d3dTarget =
        static_cast<gpu::RenderTargetD3D*>(canvas->renderTarget());
    ID3D11Texture2D* d3dTex = d3dTarget->targetTexture();
    assert(d3dTex != nullptr);

    D3D11_TEXTURE2D_DESC d3dDesc{};
    d3dTex->GetDesc(&d3dDesc);
    TextureFormat oreFormat;
    switch (d3dDesc.Format)
    {
        case DXGI_FORMAT_R8G8B8A8_UNORM:
            oreFormat = TextureFormat::rgba8unorm;
            break;
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
            oreFormat = TextureFormat::rgba16float;
            break;
        case DXGI_FORMAT_R10G10B10A2_UNORM:
            oreFormat = TextureFormat::rgb10a2unorm;
            break;
        default: /* DXGI_FORMAT_B8G8R8A8_UNORM */
            oreFormat = TextureFormat::bgra8unorm;
            break;
    }

    TextureDesc texDesc{};
    texDesc.width = canvas->width();
    texDesc.height = canvas->height();
    texDesc.format = oreFormat;
    texDesc.type = TextureType::texture2D;
    texDesc.renderTarget = true;
    texDesc.numMipmaps = 1;
    texDesc.sampleCount = 1;

    auto texture = rcp<Texture>(new Texture(texDesc));
    texture->m_d3d11Texture = d3dTex; // Borrow — RenderCanvas owns it.
    texture->m_d3d11Context = m_d3d11Context.Get();

    TextureViewDesc viewDesc{};
    viewDesc.texture = texture.get();
    viewDesc.dimension = TextureViewDimension::texture2D;
    viewDesc.baseMipLevel = 0;
    viewDesc.mipCount = 1;
    viewDesc.baseLayer = 0;
    viewDesc.layerCount = 1;

    auto view = rcp<TextureView>(new TextureView(std::move(texture), viewDesc));
    // Borrow the existing RTV from the D3D render target (AddRefs via ComPtr).
    view->m_d3dRTV = d3dTarget->targetRTV();
    return view;
}

// ============================================================================
// d3d11WrapRiveTexture
// ============================================================================

rcp<TextureView> Context::d3d11WrapRiveTexture(gpu::Texture* gpuTex,
                                               uint32_t w,
                                               uint32_t h)
{
    if (!gpuTex)
        return nullptr;

    auto* d3dTex = static_cast<ID3D11Texture2D*>(gpuTex->nativeHandle());
    if (!d3dTex)
        return nullptr;

    // Query the actual format from the D3D texture.
    D3D11_TEXTURE2D_DESC d3dDesc{};
    d3dTex->GetDesc(&d3dDesc);
    TextureFormat oreFormat;
    switch (d3dDesc.Format)
    {
        case DXGI_FORMAT_R8G8B8A8_UNORM:
            oreFormat = TextureFormat::rgba8unorm;
            break;
        case DXGI_FORMAT_B8G8R8A8_UNORM:
            oreFormat = TextureFormat::bgra8unorm;
            break;
        default:
            oreFormat = TextureFormat::rgba8unorm;
            break;
    }

    TextureDesc texDesc{};
    texDesc.width = w;
    texDesc.height = h;
    texDesc.format = oreFormat;
    texDesc.type = TextureType::texture2D;
    texDesc.renderTarget = false;
    texDesc.numMipmaps = 1;
    texDesc.sampleCount = 1;

    auto texture = rcp<Texture>(new Texture(texDesc));
    texture->m_d3d11Texture = d3dTex; // Borrow — caller owns via RenderImage.
    texture->m_d3d11Context = m_d3d11Context.Get();

    TextureViewDesc viewDesc{};
    viewDesc.texture = texture.get();
    viewDesc.dimension = TextureViewDimension::texture2D;
    viewDesc.baseMipLevel = 0;
    viewDesc.mipCount = 1;
    viewDesc.baseLayer = 0;
    viewDesc.layerCount = 1;

    auto view = rcp<TextureView>(new TextureView(std::move(texture), viewDesc));
    // Create an SRV for sampling. The canvas path borrows an existing RTV,
    // but for image sampling we need a ShaderResourceView.
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = d3dDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;
    ComPtr<ID3D11Device> device;
    m_d3d11Context->GetDevice(device.GetAddressOf());
    HRESULT hr = device->CreateShaderResourceView(
        d3dTex,
        &srvDesc,
        view->m_d3dSRV.ReleaseAndGetAddressOf());
    if (FAILED(hr))
        return nullptr;
    return view;
}

// ============================================================================
// Public method definitions (D3D11-only builds)
// ============================================================================

#if defined(ORE_BACKEND_D3D11) && !defined(ORE_BACKEND_D3D12)

Context::Context() {}

Context::~Context()
{
    m_d3d11Context.Reset();
    m_d3d11Device.Reset();
}

Context::Context(Context&& other) noexcept :
    m_features(other.m_features),
    m_d3d11Device(std::move(other.m_d3d11Device)),
    m_d3d11Context(std::move(other.m_d3d11Context)),
    m_d3d11Context1(std::move(other.m_d3d11Context1))
{}

Context& Context::operator=(Context&& other) noexcept
{
    if (this != &other)
    {
        m_d3d11Context1.Reset();
        m_d3d11Context.Reset();
        m_d3d11Device.Reset();
        m_features = other.m_features;
        m_d3d11Device = std::move(other.m_d3d11Device);
        m_d3d11Context = std::move(other.m_d3d11Context);
        m_d3d11Context1 = std::move(other.m_d3d11Context1);
    }
    return *this;
}

Context Context::createD3D11(ID3D11Device* device, ID3D11DeviceContext* context)
{
    Context ctx;
    ctx.m_d3d11Device = device;
    ctx.m_d3d11Context = context;
    // QI the 11.1 context for offset-range constant-buffer binds. D3D11.1 is
    // ubiquitous on supported Rive platforms; pre-11.1 drivers fall back to
    // whole-buffer binds (no offsets).
    context->QueryInterface(IID_PPV_ARGS(ctx.m_d3d11Context1.GetAddressOf()));

    // Populate features for D3D11 feature level 11.0.
    Features& f = ctx.m_features;
    f.colorBufferFloat = true;
    f.perTargetBlend = true;
    f.perTargetWriteMask = true;
    f.textureViewSampling = true;
    f.drawBaseInstance =
        true; // DrawInstanced/DrawIndexedInstanced support baseInstance.
    f.depthBiasClamp = true;
    f.anisotropicFiltering = true;
    f.texture3D = true;
    f.textureArrays = true;
    f.computeShaders = true;
    f.storageBuffers = true;
    f.bc = true;
    f.etc2 = false;
    f.astc = false;

    f.maxColorAttachments = 8;
    f.maxTextureSize2D = 16384;
    f.maxTextureSizeCube = 16384;
    f.maxTextureSize3D = 2048;
    f.maxUniformBufferSize = 64 * 1024; // D3D11 CBs capped at 65536 bytes.
    f.maxVertexAttributes = 16;
    f.maxSamplers = 16;

    return ctx;
}

void Context::beginFrame()
{
    // Release deferred BindGroups from last frame. By beginFrame() the
    // caller has waited for the previous frame's GPU work to complete.
    m_deferredBindGroups.clear();
} // D3D11 immediate context has no command buffer.

void Context::waitForGPU() {} // D3D11 immediate context is synchronous.

void Context::endFrame() {}

rcp<Buffer> Context::makeBuffer(const BufferDesc& desc)
{
    return d3d11MakeBuffer(desc);
}

rcp<Texture> Context::makeTexture(const TextureDesc& desc)
{
    return d3d11MakeTexture(desc);
}

rcp<TextureView> Context::makeTextureView(const TextureViewDesc& desc)
{
    return d3d11MakeTextureView(desc);
}

rcp<Sampler> Context::makeSampler(const SamplerDesc& desc)
{
    return d3d11MakeSampler(desc);
}

rcp<ShaderModule> Context::makeShaderModule(const ShaderModuleDesc& desc)
{
    return d3d11MakeShaderModule(desc);
}

rcp<Pipeline> Context::makePipeline(const PipelineDesc& desc,
                                    std::string* outError)
{
    return d3d11MakePipeline(desc, outError);
}

rcp<BindGroup> Context::makeBindGroup(const BindGroupDesc& desc)
{
    return d3d11MakeBindGroup(desc);
}

rcp<BindGroupLayout> Context::makeBindGroupLayout(
    const BindGroupLayoutDesc& desc)
{
    return d3d11MakeBindGroupLayout(desc);
}

RenderPass Context::beginRenderPass(const RenderPassDesc& desc,
                                    std::string* outError)
{
    finishActiveRenderPass();
    return d3d11BeginRenderPass(desc, outError);
}

rcp<TextureView> Context::wrapCanvasTexture(gpu::RenderCanvas* canvas)
{
    return d3d11WrapCanvasTexture(canvas);
}

rcp<TextureView> Context::wrapRiveTexture(gpu::Texture* gpuTex,
                                          uint32_t w,
                                          uint32_t h)
{
    return d3d11WrapRiveTexture(gpuTex, w, h);
}

#endif // ORE_BACKEND_D3D11 && !ORE_BACKEND_D3D12

} // namespace rive::ore
