/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/ore/ore_context.hpp"
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_texture.hpp"
#include "rive/renderer/ore/ore_sampler.hpp"
#include "rive/renderer/ore/ore_shader_module.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/renderer/ore/ore_render_pass.hpp"
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/d3d12/render_context_d3d12_impl.hpp"
#include "rive/rive_types.hpp"
#include "ore_d3d12_root_sig.hpp"

#include <d3d12.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <string>

using Microsoft::WRL::ComPtr;

namespace rive::ore
{

// ============================================================================
// Enum → DXGI / D3D12 conversion helpers
// (Shared format table with D3D11 but typed for D3D12.)
// ============================================================================

struct D3D12FormatInfo
{
    DXGI_FORMAT resource; // Typeless for depth; typed for colour.
    DXGI_FORMAT srv;      // Shader-resource view format.
    DXGI_FORMAT rtv;      // Render-target view (UNKNOWN for depth).
    DXGI_FORMAT dsv;      // Depth-stencil view (UNKNOWN for colour).
    bool isDepth;
};

static D3D12FormatInfo oreFormatInfoD3D12(TextureFormat fmt)
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
        // Depth — typeless resource for both DSV and SRV.
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
        case TextureFormat::etc2rgb8:
        case TextureFormat::etc2rgba8:
        case TextureFormat::astc4x4:
        case TextureFormat::astc6x6:
        case TextureFormat::astc8x8:
            RIVE_UNREACHABLE();
    }
    RIVE_UNREACHABLE();
}

static D3D12_FILTER oreFilterToD3D12(Filter min,
                                     Filter mag,
                                     Filter mip,
                                     CompareFunction compare,
                                     uint32_t aniso)
{
    bool isComp = (compare != CompareFunction::none);
    if (aniso > 1)
        return isComp ? D3D12_FILTER_COMPARISON_ANISOTROPIC
                      : D3D12_FILTER_ANISOTROPIC;
    if (min == Filter::linear && mag == Filter::linear && mip == Filter::linear)
        return isComp ? D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR
                      : D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    if (min == Filter::linear && mag == Filter::linear &&
        mip == Filter::nearest)
        return isComp ? D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT
                      : D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    if (min == Filter::linear && mag == Filter::nearest &&
        mip == Filter::linear)
        return isComp ? D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR
                      : D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
    if (min == Filter::linear && mag == Filter::nearest &&
        mip == Filter::nearest)
        return isComp ? D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT
                      : D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
    if (min == Filter::nearest && mag == Filter::linear &&
        mip == Filter::linear)
        return isComp ? D3D12_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR
                      : D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR;
    if (min == Filter::nearest && mag == Filter::linear &&
        mip == Filter::nearest)
        return isComp ? D3D12_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT
                      : D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
    if (min == Filter::nearest && mag == Filter::nearest &&
        mip == Filter::linear)
        return isComp ? D3D12_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR
                      : D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
    return isComp ? D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT
                  : D3D12_FILTER_MIN_MAG_MIP_POINT;
}

static D3D12_TEXTURE_ADDRESS_MODE oreWrapToD3D12(WrapMode mode)
{
    switch (mode)
    {
        case WrapMode::repeat:
            return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        case WrapMode::mirrorRepeat:
            return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        case WrapMode::clampToEdge:
            return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    }
    RIVE_UNREACHABLE();
}

static D3D12_COMPARISON_FUNC oreCompareFunctionToD3D12(CompareFunction fn)
{
    switch (fn)
    {
        case CompareFunction::none:
        case CompareFunction::never:
            return D3D12_COMPARISON_FUNC_NEVER;
        case CompareFunction::less:
            return D3D12_COMPARISON_FUNC_LESS;
        case CompareFunction::equal:
            return D3D12_COMPARISON_FUNC_EQUAL;
        case CompareFunction::lessEqual:
            return D3D12_COMPARISON_FUNC_LESS_EQUAL;
        case CompareFunction::greater:
            return D3D12_COMPARISON_FUNC_GREATER;
        case CompareFunction::notEqual:
            return D3D12_COMPARISON_FUNC_NOT_EQUAL;
        case CompareFunction::greaterEqual:
            return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        case CompareFunction::always:
            return D3D12_COMPARISON_FUNC_ALWAYS;
    }
    RIVE_UNREACHABLE();
}

static D3D12_BLEND oreBlendFactorToD3D12(BlendFactor f)
{
    switch (f)
    {
        case BlendFactor::zero:
            return D3D12_BLEND_ZERO;
        case BlendFactor::one:
            return D3D12_BLEND_ONE;
        case BlendFactor::srcColor:
            return D3D12_BLEND_SRC_COLOR;
        case BlendFactor::oneMinusSrcColor:
            return D3D12_BLEND_INV_SRC_COLOR;
        case BlendFactor::srcAlpha:
            return D3D12_BLEND_SRC_ALPHA;
        case BlendFactor::oneMinusSrcAlpha:
            return D3D12_BLEND_INV_SRC_ALPHA;
        case BlendFactor::dstColor:
            return D3D12_BLEND_DEST_COLOR;
        case BlendFactor::oneMinusDstColor:
            return D3D12_BLEND_INV_DEST_COLOR;
        case BlendFactor::dstAlpha:
            return D3D12_BLEND_DEST_ALPHA;
        case BlendFactor::oneMinusDstAlpha:
            return D3D12_BLEND_INV_DEST_ALPHA;
        case BlendFactor::srcAlphaSaturated:
            return D3D12_BLEND_SRC_ALPHA_SAT;
        case BlendFactor::blendColor:
            return D3D12_BLEND_BLEND_FACTOR;
        case BlendFactor::oneMinusBlendColor:
            return D3D12_BLEND_INV_BLEND_FACTOR;
    }
    RIVE_UNREACHABLE();
}

static D3D12_BLEND_OP oreBlendOpToD3D12(BlendOp op)
{
    switch (op)
    {
        case BlendOp::add:
            return D3D12_BLEND_OP_ADD;
        case BlendOp::subtract:
            return D3D12_BLEND_OP_SUBTRACT;
        case BlendOp::reverseSubtract:
            return D3D12_BLEND_OP_REV_SUBTRACT;
        case BlendOp::min:
            return D3D12_BLEND_OP_MIN;
        case BlendOp::max:
            return D3D12_BLEND_OP_MAX;
    }
    RIVE_UNREACHABLE();
}

static D3D12_STENCIL_OP oreStencilOpToD3D12(StencilOp op)
{
    switch (op)
    {
        case StencilOp::keep:
            return D3D12_STENCIL_OP_KEEP;
        case StencilOp::zero:
            return D3D12_STENCIL_OP_ZERO;
        case StencilOp::replace:
            return D3D12_STENCIL_OP_REPLACE;
        case StencilOp::incrementClamp:
            return D3D12_STENCIL_OP_INCR_SAT;
        case StencilOp::decrementClamp:
            return D3D12_STENCIL_OP_DECR_SAT;
        case StencilOp::invert:
            return D3D12_STENCIL_OP_INVERT;
        case StencilOp::incrementWrap:
            return D3D12_STENCIL_OP_INCR;
        case StencilOp::decrementWrap:
            return D3D12_STENCIL_OP_DECR;
    }
    RIVE_UNREACHABLE();
}

static UINT8 oreColorWriteMaskToD3D12(ColorWriteMask mask)
{
    UINT8 d3d = 0;
    if ((mask & ColorWriteMask::red) != ColorWriteMask::none)
        d3d |= D3D12_COLOR_WRITE_ENABLE_RED;
    if ((mask & ColorWriteMask::green) != ColorWriteMask::none)
        d3d |= D3D12_COLOR_WRITE_ENABLE_GREEN;
    if ((mask & ColorWriteMask::blue) != ColorWriteMask::none)
        d3d |= D3D12_COLOR_WRITE_ENABLE_BLUE;
    if ((mask & ColorWriteMask::alpha) != ColorWriteMask::none)
        d3d |= D3D12_COLOR_WRITE_ENABLE_ALPHA;
    return d3d;
}

static DXGI_FORMAT oreVertexFormatToDXGI12(VertexFormat fmt)
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

static D3D12_PRIMITIVE_TOPOLOGY oreTopologyToD3D12(PrimitiveTopology t)
{
    switch (t)
    {
        case PrimitiveTopology::pointList:
            return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
        case PrimitiveTopology::lineList:
            return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
        case PrimitiveTopology::lineStrip:
            return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
        case PrimitiveTopology::triangleList:
            return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        case PrimitiveTopology::triangleStrip:
            return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    }
    RIVE_UNREACHABLE();
}

static D3D12_PRIMITIVE_TOPOLOGY_TYPE oreTopologyTypeToD3D12(PrimitiveTopology t)
{
    switch (t)
    {
        case PrimitiveTopology::pointList:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
        case PrimitiveTopology::lineList:
        case PrimitiveTopology::lineStrip:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        case PrimitiveTopology::triangleList:
        case PrimitiveTopology::triangleStrip:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    }
    RIVE_UNREACHABLE();
}

// ============================================================================
// GPU-side fence wait helper
// ============================================================================

static void d3d12WaitFence(ID3D12CommandQueue* queue,
                           ID3D12Fence* fence,
                           uint64_t& fenceValue,
                           HANDLE fenceEvent)
{
    ++fenceValue;
    queue->Signal(fence, fenceValue);
    if (fence->GetCompletedValue() < fenceValue)
    {
        fence->SetEventOnCompletion(fenceValue, fenceEvent);
        WaitForSingleObject(fenceEvent, INFINITE);
    }
}

// ============================================================================
// Context — public method definitions (D3D12-only builds)
// When both D3D11 and D3D12 are compiled, the combined
// ore_context_d3d11_d3d12.cpp file provides these methods with dispatch.
// ============================================================================

#if defined(ORE_BACKEND_D3D12) && !defined(ORE_BACKEND_D3D11)

Context::Context() {}

Context::~Context()
{
#if defined(ORE_BACKEND_D3D12)
    if (m_d3dDevice)
    {
        // Flush all pending GPU work before releasing resources. In
        // external-CL mode Ore has no frame fence of its own, but the host
        // must have waited on its fence before entering the destructor; we
        // additionally wait on our own queue so pending destroys are safe.
        if (m_d3dQueue && m_d3dFence && m_d3dFenceEvent)
            d3d12WaitFence(m_d3dQueue.Get(),
                           m_d3dFence.Get(),
                           m_d3dFenceValue,
                           m_d3dFenceEvent);

        // Drain any closures that were queued while in external-CL mode.
        d3dDrainDeferred();

        m_d3dPendingUploads.clear();

        if (m_d3dFenceEvent)
        {
            CloseHandle(m_d3dFenceEvent);
            m_d3dFenceEvent = nullptr;
        }
    }
#endif
}

Context::Context(Context&& other) noexcept :
    m_features(other.m_features)
#if defined(ORE_BACKEND_D3D12)
    ,
    m_d3dDevice(std::move(other.m_d3dDevice)),
    m_d3dQueue(std::move(other.m_d3dQueue)),
    m_d3dAllocator(std::move(other.m_d3dAllocator)),
    m_d3dOwnedCmdList(std::move(other.m_d3dOwnedCmdList)),
    m_d3dCmdList(other.m_d3dCmdList),
    m_d3dUploadAllocator(std::move(other.m_d3dUploadAllocator)),
    m_d3dUploadCmdList(std::move(other.m_d3dUploadCmdList)),
    m_d3dUploadListOpen(other.m_d3dUploadListOpen),
    m_d3dFence(std::move(other.m_d3dFence)),
    m_d3dFenceValue(other.m_d3dFenceValue),
    m_d3dFenceEvent(other.m_d3dFenceEvent),
    m_d3dCpuSrvHeap(std::move(other.m_d3dCpuSrvHeap)),
    m_d3dCpuRtvHeap(std::move(other.m_d3dCpuRtvHeap)),
    m_d3dCpuDsvHeap(std::move(other.m_d3dCpuDsvHeap)),
    m_d3dCpuSamplerHeap(std::move(other.m_d3dCpuSamplerHeap)),
    m_d3dCpuSrvAllocated(other.m_d3dCpuSrvAllocated),
    m_d3dCpuRtvAllocated(other.m_d3dCpuRtvAllocated),
    m_d3dCpuDsvAllocated(other.m_d3dCpuDsvAllocated),
    m_d3dCpuSamplerAllocated(other.m_d3dCpuSamplerAllocated),
    m_d3dSrvDescSize(other.m_d3dSrvDescSize),
    m_d3dRtvDescSize(other.m_d3dRtvDescSize),
    m_d3dDsvDescSize(other.m_d3dDsvDescSize),
    m_d3dSamplerDescSize(other.m_d3dSamplerDescSize),
    m_d3dNullSrv(other.m_d3dNullSrv),
    m_d3dNullSampler(other.m_d3dNullSampler),
    m_d3dGpuSrvHeap(std::move(other.m_d3dGpuSrvHeap)),
    m_d3dGpuSamplerHeap(std::move(other.m_d3dGpuSamplerHeap)),
    m_d3dGpuSrvAllocated(other.m_d3dGpuSrvAllocated),
    m_d3dGpuSamplerAllocated(other.m_d3dGpuSamplerAllocated),
    m_d3dPendingUploads(std::move(other.m_d3dPendingUploads)),
    m_d3dExternalCmdList(other.m_d3dExternalCmdList),
    m_d3dDeferredDestroys(std::move(other.m_d3dDeferredDestroys))
#endif
{
#if defined(ORE_BACKEND_D3D12)
    other.m_d3dFenceEvent = nullptr;
    other.m_d3dFenceValue = 0;
    other.m_d3dUploadListOpen = false;
    other.m_d3dCmdList = nullptr;
    other.m_d3dExternalCmdList = false;
#endif
}

Context& Context::operator=(Context&& other) noexcept
{
    if (this != &other)
    {
        this->~Context();
        new (this) Context(std::move(other));
    }
    return *this;
}

// ============================================================================
// Context::createD3D12
// ============================================================================

Context Context::createD3D12(ID3D12Device* device, ID3D12CommandQueue* queue)
{
    Context ctx;
    ctx.m_d3dDevice = device;
    ctx.m_d3dQueue = queue;

    // --- Command allocator + list for rendering ---
    [[maybe_unused]] HRESULT hr;
    hr = device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(ctx.m_d3dAllocator.GetAddressOf()));
    assert(SUCCEEDED(hr));
    hr = device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        ctx.m_d3dAllocator.Get(),
        nullptr,
        IID_PPV_ARGS(ctx.m_d3dOwnedCmdList.GetAddressOf()));
    assert(SUCCEEDED(hr));
    ctx.m_d3dCmdList = ctx.m_d3dOwnedCmdList.Get();
    // Close immediately; reopened in beginFrame().
    ctx.m_d3dCmdList->Close();

    // --- Upload allocator + list ---
    hr = device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(ctx.m_d3dUploadAllocator.GetAddressOf()));
    assert(SUCCEEDED(hr));
    hr = device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        ctx.m_d3dUploadAllocator.Get(),
        nullptr,
        IID_PPV_ARGS(ctx.m_d3dUploadCmdList.GetAddressOf()));
    assert(SUCCEEDED(hr));
    ctx.m_d3dUploadCmdList->Close();

    // --- Fence ---
    hr = device->CreateFence(0,
                             D3D12_FENCE_FLAG_NONE,
                             IID_PPV_ARGS(ctx.m_d3dFence.GetAddressOf()));
    assert(SUCCEEDED(hr));
    ctx.m_d3dFenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    assert(ctx.m_d3dFenceEvent != nullptr);

    // Root signatures are per-pipeline (RFC v5 §3.2.2). Each pipeline
    // builds its own from its `ore::BindingMap` via `buildPerPipeline-
    // RootSig` at `d3d12MakePipeline` time; the render pass binds that
    // sig at `setPipeline` when it differs from the currently-bound
    // one. No context-wide root signature is needed.

    // --- Descriptor heap sizes ---
    ctx.m_d3dSrvDescSize = device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    ctx.m_d3dRtvDescSize = device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    ctx.m_d3dDsvDescSize = device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    ctx.m_d3dSamplerDescSize = device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

    // --- CPU-visible heaps ---
    auto makeCpuHeap = [&](D3D12_DESCRIPTOR_HEAP_TYPE type,
                           UINT count,
                           ComPtr<ID3D12DescriptorHeap>& out) {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = type;
        desc.NumDescriptors = count;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        [[maybe_unused]] HRESULT r =
            device->CreateDescriptorHeap(&desc,
                                         IID_PPV_ARGS(out.GetAddressOf()));
        assert(SUCCEEDED(r));
    };
    makeCpuHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                1024,
                ctx.m_d3dCpuSrvHeap);
    makeCpuHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 256, ctx.m_d3dCpuRtvHeap);
    makeCpuHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 64, ctx.m_d3dCpuDsvHeap);
    makeCpuHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
                256,
                ctx.m_d3dCpuSamplerHeap);

    // --- GPU-visible heaps ---
    auto makeGpuHeap = [&](D3D12_DESCRIPTOR_HEAP_TYPE type,
                           UINT count,
                           ComPtr<ID3D12DescriptorHeap>& out) {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = type;
        desc.NumDescriptors = count;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        [[maybe_unused]] HRESULT r =
            device->CreateDescriptorHeap(&desc,
                                         IID_PPV_ARGS(out.GetAddressOf()));
        assert(SUCCEEDED(r));
    };
    makeGpuHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                4096,
                ctx.m_d3dGpuSrvHeap);
    makeGpuHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
                512,
                ctx.m_d3dGpuSamplerHeap);

    // --- Null SRV (2D texture, for unused descriptor-table slots) ---
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle =
            ctx.m_d3dCpuSrvHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += ctx.m_d3dCpuSrvAllocated++ * ctx.m_d3dSrvDescSize;
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping =
            D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = 1;
        device->CreateShaderResourceView(nullptr, &srvDesc, handle);
        ctx.m_d3dNullSrv = handle;
    }

    // --- Null Sampler ---
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle =
            ctx.m_d3dCpuSamplerHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += ctx.m_d3dCpuSamplerAllocated++ * ctx.m_d3dSamplerDescSize;
        D3D12_SAMPLER_DESC sampDesc = {};
        sampDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        sampDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        sampDesc.MaxLOD = D3D12_FLOAT32_MAX;
        device->CreateSampler(&sampDesc, handle);
        ctx.m_d3dNullSampler = handle;
    }

    // --- Features ---
    Features& f = ctx.m_features;
    f.colorBufferFloat = true;
    f.perTargetBlend = true;
    f.perTargetWriteMask = true;
    f.textureViewSampling = true;
    f.drawBaseInstance = true;
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
    f.maxUniformBufferSize = 65536;
    f.maxVertexAttributes = 16;
    f.maxSamplers = 16;

    return ctx;
}

// ============================================================================
// beginFrame / endFrame / waitForGPU
// ============================================================================

void Context::beginFrame()
{
    // Release deferred BindGroups from last frame. By beginFrame() the
    // caller has waited for the previous frame's GPU work to complete.
    m_deferredBindGroups.clear();

#if defined(ORE_BACKEND_D3D12)
    // Drain any destroys queued while the previous frame was in external-CL
    // mode. In owned-CL mode the queue is empty (closures run immediately).
    d3dDrainDeferred();

    m_d3dExternalCmdList = false;
    m_d3dCmdList = m_d3dOwnedCmdList.Get();

    // Flush any queued texture uploads before opening the main command list.
    d3d12FlushUploads();

    m_d3dAllocator->Reset();
    m_d3dCmdList->Reset(m_d3dAllocator.Get(), nullptr);

    // Reset frame-scoped GPU heap allocation offsets.
    m_d3dGpuSrvAllocated = 0;
    m_d3dGpuSamplerAllocated = 0;

    // Bind GPU-visible heaps once (both SRV/CBV and Sampler). Root sig
    // is bound by `RenderPass::setPipeline` at the first pipeline use —
    // every pipeline ships with its own (RFC v5 §3.2.2).
    ID3D12DescriptorHeap* heaps[] = {m_d3dGpuSrvHeap.Get(),
                                     m_d3dGpuSamplerHeap.Get()};
    m_d3dCmdList->SetDescriptorHeaps(2, heaps);
#endif
}

void Context::beginFrame(ID3D12GraphicsCommandList* externalCl)
{
#if defined(ORE_BACKEND_D3D12)
    assert(externalCl != nullptr);

    m_deferredBindGroups.clear();
    d3dDrainDeferred();

    // Our upload CL is independent of the host's frame CL — submit any
    // pending uploads now so textures are ready before the host's draws.
    // The submit happens on the shared queue; D3D12 queue ordering ensures
    // the uploads finish before the host's eventual frame submission.
    d3d12FlushUploads();

    m_d3dGpuSrvAllocated = 0;
    m_d3dGpuSamplerAllocated = 0;

    m_d3dCmdList = externalCl;
    m_d3dExternalCmdList = true;

    // The host's command list carries whatever state the host set before
    // handing it to us. Bind Ore's descriptor heaps; each pipeline binds
    // its own per-pipeline root sig at setPipeline. The host is
    // responsible for re-binding its own state after endFrame().
    ID3D12DescriptorHeap* heaps[] = {m_d3dGpuSrvHeap.Get(),
                                     m_d3dGpuSamplerHeap.Get()};
    m_d3dCmdList->SetDescriptorHeaps(2, heaps);
#else
    (void)externalCl;
#endif
}

void Context::waitForGPU()
{
#if defined(ORE_BACKEND_D3D12)
    d3d12WaitFence(m_d3dQueue.Get(),
                   m_d3dFence.Get(),
                   m_d3dFenceValue,
                   m_d3dFenceEvent);
#endif
}

void Context::endFrame()
{
#if defined(ORE_BACKEND_D3D12)
    if (m_d3dExternalCmdList)
    {
        // External-CL mode: host owns Close/Execute/fence. Deferred destroys
        // wait until the next beginFrame() when the host has waited on the
        // prior frame fence.
        return;
    }

    m_d3dOwnedCmdList->Close();

    ID3D12CommandList* lists[] = {m_d3dOwnedCmdList.Get()};
    m_d3dQueue->ExecuteCommandLists(1, lists);

    d3d12WaitFence(m_d3dQueue.Get(),
                   m_d3dFence.Get(),
                   m_d3dFenceValue,
                   m_d3dFenceEvent);

    // Release staging upload resources now that the GPU is idle.
    m_d3dPendingUploads.clear();
#endif
}

#endif // D3D12-only (Context lifecycle + beginFrame/endFrame)

// ============================================================================
// d3d12FlushUploads + GPU-visible heap allocation helpers (called by
// RenderPass and d3d12* helpers)
// ============================================================================

#if defined(ORE_BACKEND_D3D12)

void Context::d3dDrainDeferred()
{
    for (auto& destroy : m_d3dDeferredDestroys)
        destroy();
    m_d3dDeferredDestroys.clear();
}

void Context::d3dDeferDestroy(std::function<void()> destroy)
{
    if (m_d3dExternalCmdList)
        m_d3dDeferredDestroys.push_back(std::move(destroy));
    else
        destroy();
}

void Context::d3d12FlushUploads()
{
    if (!m_d3dUploadListOpen)
        return;

    m_d3dUploadCmdList->Close();
    ID3D12CommandList* lists[] = {m_d3dUploadCmdList.Get()};
    m_d3dQueue->ExecuteCommandLists(1, lists);
    d3d12WaitFence(m_d3dQueue.Get(),
                   m_d3dFence.Get(),
                   m_d3dFenceValue,
                   m_d3dFenceEvent);
    m_d3dPendingUploads.clear();
    m_d3dUploadListOpen = false;
}

UINT Context::d3d12AllocGpuSrvSlots(UINT count)
{
    // Heap exhaustion is reachable from a Lua script that creates more
    // bind groups than the GPU heap can hold (4096 SRV/CBV/UAV slots).
    // Pre-fix this asserted and crashed in debug, GPU-hung in release.
    // Per `feedback_lua_gpu_misuse_validation`, surface as setLastError
    // and return a sentinel so the apply path can no-op the bind.
    if (m_d3dGpuSrvAllocated + count > 4096)
    {
        setLastError("d3d12AllocGpuSrvSlots: GPU SRV heap exhausted (need %u, "
                     "%u/4096 used)",
                     count,
                     m_d3dGpuSrvAllocated);
        return UINT_MAX;
    }
    UINT start = m_d3dGpuSrvAllocated;
    m_d3dGpuSrvAllocated += count;
    return start;
}

UINT Context::d3d12AllocGpuSamplerSlots(UINT count)
{
    if (m_d3dGpuSamplerAllocated + count > 512)
    {
        setLastError("d3d12AllocGpuSamplerSlots: GPU sampler heap exhausted "
                     "(need %u, %u/512 used)",
                     count,
                     m_d3dGpuSamplerAllocated);
        return UINT_MAX;
    }
    UINT start = m_d3dGpuSamplerAllocated;
    m_d3dGpuSamplerAllocated += count;
    return start;
}

D3D12_CPU_DESCRIPTOR_HANDLE Context::d3d12GpuSrvCpuHandle(UINT index) const
{
    D3D12_CPU_DESCRIPTOR_HANDLE h =
        m_d3dGpuSrvHeap->GetCPUDescriptorHandleForHeapStart();
    h.ptr += (SIZE_T)index * m_d3dSrvDescSize;
    return h;
}

D3D12_GPU_DESCRIPTOR_HANDLE Context::d3d12GpuSrvGpuHandle(UINT index) const
{
    D3D12_GPU_DESCRIPTOR_HANDLE h =
        m_d3dGpuSrvHeap->GetGPUDescriptorHandleForHeapStart();
    h.ptr += (UINT64)index * m_d3dSrvDescSize;
    return h;
}

D3D12_CPU_DESCRIPTOR_HANDLE Context::d3d12GpuSamplerCpuHandle(UINT index) const
{
    D3D12_CPU_DESCRIPTOR_HANDLE h =
        m_d3dGpuSamplerHeap->GetCPUDescriptorHandleForHeapStart();
    h.ptr += (SIZE_T)index * m_d3dSamplerDescSize;
    return h;
}

D3D12_GPU_DESCRIPTOR_HANDLE Context::d3d12GpuSamplerGpuHandle(UINT index) const
{
    D3D12_GPU_DESCRIPTOR_HANDLE h =
        m_d3dGpuSamplerHeap->GetGPUDescriptorHandleForHeapStart();
    h.ptr += (UINT64)index * m_d3dSamplerDescSize;
    return h;
}

#endif // ORE_BACKEND_D3D12

// ============================================================================
// D3D12 helper methods — always compiled (used by combined d3d11+d3d12
// dispatch file).
// ============================================================================

// ============================================================================
// d3d12MakeBuffer
// ============================================================================

rcp<Buffer> Context::d3d12MakeBuffer(const BufferDesc& desc)
{
#if defined(ORE_BACKEND_D3D12)
    auto buffer = rcp<Buffer>(new Buffer(desc.size, desc.usage));
    buffer->m_d3dOreContext = this;

    // All ORE buffers live in UPLOAD heap — simple, safe for test workloads.
    UINT64 alignedSize = desc.size;
    if (desc.usage == BufferUsage::uniform)
        alignedSize = (alignedSize + 255) & ~255ULL; // 256-byte CB alignment.

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Width = alignedSize;
    resDesc.Height = 1;
    resDesc.DepthOrArraySize = 1;
    resDesc.MipLevels = 1;
    resDesc.Format = DXGI_FORMAT_UNKNOWN;
    resDesc.SampleDesc.Count = 1;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    HRESULT hr = m_d3dDevice->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(buffer->m_d3dBuffer.GetAddressOf()));
    if (FAILED(hr))
        return nullptr;

    // Persistently map. UPLOAD heap memory is write-combined; keep mapped.
    D3D12_RANGE readRange = {0, 0};
    hr = buffer->m_d3dBuffer->Map(0, &readRange, &buffer->m_d3dMappedPtr);
    if (FAILED(hr))
        return nullptr;

    if (desc.data)
        memcpy(buffer->m_d3dMappedPtr, desc.data, desc.size);

    return buffer;
#else
    (void)desc;
    return {};
#endif
}

// ============================================================================
// d3d12MakeTexture
// ============================================================================

rcp<Texture> Context::d3d12MakeTexture(const TextureDesc& desc)
{
#if defined(ORE_BACKEND_D3D12)
    auto texture = rcp<Texture>(new Texture(desc));
    texture->m_d3dDevice = m_d3dDevice.Get();
    texture->m_d3dOreContext = this;

    const D3D12FormatInfo fmtInfo = oreFormatInfoD3D12(desc.format);

    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
    D3D12_RESOURCE_STATES initialState;
    const D3D12_CLEAR_VALUE* clearVal = nullptr;
    D3D12_CLEAR_VALUE clearValue = {};

    if (fmtInfo.isDepth)
    {
        flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        initialState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        clearValue.Format = fmtInfo.dsv;
        clearValue.DepthStencil.Depth = 1.0f;
        clearValue.DepthStencil.Stencil = 0;
        clearVal = &clearValue;
    }
    else if (desc.renderTarget)
    {
        flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        initialState = D3D12_RESOURCE_STATE_RENDER_TARGET;
        clearValue.Format = fmtInfo.rtv;
        clearValue.Color[3] = 1.0f;
        clearVal = &clearValue;
    }
    else
    {
        initialState = D3D12_RESOURCE_STATE_COPY_DEST;
    }
    texture->m_d3dCurrentState = initialState;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.MipLevels = static_cast<UINT16>(desc.numMipmaps);
    resDesc.Format = fmtInfo.resource;
    resDesc.Width = desc.width;
    resDesc.Height = desc.height;
    resDesc.Flags = flags;
    resDesc.SampleDesc.Count = desc.sampleCount;
    resDesc.SampleDesc.Quality = 0;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    if (desc.type == TextureType::texture3D)
    {
        resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
        resDesc.DepthOrArraySize = static_cast<UINT16>(desc.depthOrArrayLayers);
    }
    else
    {
        resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        resDesc.DepthOrArraySize =
            (desc.type == TextureType::cube)
                ? 6
                : static_cast<UINT16>(desc.depthOrArrayLayers);
    }

    HRESULT hr = m_d3dDevice->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        initialState,
        clearVal,
        IID_PPV_ARGS(texture->m_d3dTexture.GetAddressOf()));
    if (FAILED(hr))
        return nullptr;

    return texture;
#else
    (void)desc;
    return {};
#endif
}

// ============================================================================
// d3d12MakeTextureView
// ============================================================================

rcp<TextureView> Context::d3d12MakeTextureView(const TextureViewDesc& desc)
{
#if defined(ORE_BACKEND_D3D12)
    Texture* tex = desc.texture;
    if (!tex)
        return nullptr;

    auto view = rcp<TextureView>(new TextureView(ref_rcp(tex), desc));
    view->m_d3dOreContext = this;

    const D3D12FormatInfo fmtInfo = oreFormatInfoD3D12(tex->format());

    // --- SRV ---
    if (fmtInfo.srv != DXGI_FORMAT_UNKNOWN)
    {
        if (m_d3dCpuSrvAllocated >= 1024)
            return nullptr; // CPU SRV descriptor heap exhausted.
        D3D12_CPU_DESCRIPTOR_HANDLE handle =
            m_d3dCpuSrvHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += (SIZE_T)m_d3dCpuSrvAllocated++ * m_d3dSrvDescSize;

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = fmtInfo.srv;
        srvDesc.Shader4ComponentMapping =
            D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        switch (desc.dimension)
        {
            case TextureViewDimension::texture2D:
                if (tex->sampleCount() > 1)
                {
                    // MSAA resources require the _MS dimension variants.
                    // Using TEXTURE2D against a multisample resource is an
                    // illegal call that the driver caches and surfaces as
                    // DXGI_ERROR_INVALID_CALL → DEVICE_REMOVED at the next
                    // device API call.
                    if (desc.layerCount > 1)
                    {
                        srvDesc.ViewDimension =
                            D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
                        srvDesc.Texture2DMSArray.FirstArraySlice =
                            desc.baseLayer;
                        srvDesc.Texture2DMSArray.ArraySize = desc.layerCount;
                    }
                    else
                    {
                        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
                    }
                }
                else if (desc.layerCount > 1)
                {
                    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
                    srvDesc.Texture2DArray.MostDetailedMip = desc.baseMipLevel;
                    srvDesc.Texture2DArray.MipLevels = desc.mipCount;
                    srvDesc.Texture2DArray.FirstArraySlice = desc.baseLayer;
                    srvDesc.Texture2DArray.ArraySize = desc.layerCount;
                }
                else
                {
                    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                    srvDesc.Texture2D.MostDetailedMip = desc.baseMipLevel;
                    srvDesc.Texture2D.MipLevels = desc.mipCount;
                }
                break;
            case TextureViewDimension::cube:
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
                srvDesc.TextureCube.MostDetailedMip = desc.baseMipLevel;
                srvDesc.TextureCube.MipLevels = desc.mipCount;
                break;
            case TextureViewDimension::cubeArray:
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
                srvDesc.TextureCubeArray.MostDetailedMip = desc.baseMipLevel;
                srvDesc.TextureCubeArray.MipLevels = desc.mipCount;
                srvDesc.TextureCubeArray.First2DArrayFace = desc.baseLayer;
                srvDesc.TextureCubeArray.NumCubes = desc.layerCount / 6;
                break;
            case TextureViewDimension::texture3D:
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
                srvDesc.Texture3D.MostDetailedMip = desc.baseMipLevel;
                srvDesc.Texture3D.MipLevels = desc.mipCount;
                break;
            case TextureViewDimension::array2D:
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
                srvDesc.Texture2DArray.MostDetailedMip = desc.baseMipLevel;
                srvDesc.Texture2DArray.MipLevels = desc.mipCount;
                srvDesc.Texture2DArray.FirstArraySlice = desc.baseLayer;
                srvDesc.Texture2DArray.ArraySize = desc.layerCount;
                break;
        }

        m_d3dDevice->CreateShaderResourceView(tex->m_d3dTexture.Get(),
                                              &srvDesc,
                                              handle);
        view->m_d3dSrvHandle = handle;
    }

    // --- RTV (colour only) ---
    if (!fmtInfo.isDepth && tex->isRenderTarget() &&
        fmtInfo.rtv != DXGI_FORMAT_UNKNOWN)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle =
            m_d3dCpuRtvHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += (SIZE_T)m_d3dCpuRtvAllocated++ * m_d3dRtvDescSize;

        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = fmtInfo.rtv;

        switch (desc.dimension)
        {
            case TextureViewDimension::texture2D:
                if (tex->sampleCount() > 1)
                {
                    // MSAA resources require the _MS dimension variants.
                    if (desc.layerCount > 1 || desc.baseLayer > 0)
                    {
                        rtvDesc.ViewDimension =
                            D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
                        rtvDesc.Texture2DMSArray.FirstArraySlice =
                            desc.baseLayer;
                        rtvDesc.Texture2DMSArray.ArraySize = desc.layerCount;
                    }
                    else
                    {
                        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
                    }
                }
                else if (desc.layerCount > 1 || desc.baseLayer > 0)
                {
                    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                    rtvDesc.Texture2DArray.MipSlice = desc.baseMipLevel;
                    rtvDesc.Texture2DArray.FirstArraySlice = desc.baseLayer;
                    rtvDesc.Texture2DArray.ArraySize = desc.layerCount;
                }
                else
                {
                    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
                    rtvDesc.Texture2D.MipSlice = desc.baseMipLevel;
                }
                break;
            case TextureViewDimension::cube:
            case TextureViewDimension::cubeArray:
            case TextureViewDimension::array2D:
                rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                rtvDesc.Texture2DArray.MipSlice = desc.baseMipLevel;
                rtvDesc.Texture2DArray.FirstArraySlice = desc.baseLayer;
                rtvDesc.Texture2DArray.ArraySize = desc.layerCount;
                break;
            case TextureViewDimension::texture3D:
                rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
                rtvDesc.Texture3D.MipSlice = desc.baseMipLevel;
                rtvDesc.Texture3D.FirstWSlice = desc.baseLayer;
                rtvDesc.Texture3D.WSize = desc.layerCount;
                break;
        }

        m_d3dDevice->CreateRenderTargetView(tex->m_d3dTexture.Get(),
                                            &rtvDesc,
                                            handle);
        view->m_d3dRtvHandle = handle;
    }

    // --- DSV (depth only) ---
    if (fmtInfo.isDepth && fmtInfo.dsv != DXGI_FORMAT_UNKNOWN)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle =
            m_d3dCpuDsvHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += (SIZE_T)m_d3dCpuDsvAllocated++ * m_d3dDsvDescSize;

        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = fmtInfo.dsv;
        bool isArray = (desc.dimension == TextureViewDimension::array2D ||
                        (desc.dimension == TextureViewDimension::texture2D &&
                         (desc.layerCount > 1 || desc.baseLayer > 0)));

        if (tex->sampleCount() > 1)
        {
            // MSAA depth resources require the _MS dimension variants.
            if (isArray)
            {
                dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
                dsvDesc.Texture2DMSArray.FirstArraySlice = desc.baseLayer;
                dsvDesc.Texture2DMSArray.ArraySize = desc.layerCount;
            }
            else
            {
                dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
            }
        }
        else if (isArray)
        {
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
            dsvDesc.Texture2DArray.MipSlice = desc.baseMipLevel;
            dsvDesc.Texture2DArray.FirstArraySlice = desc.baseLayer;
            dsvDesc.Texture2DArray.ArraySize = desc.layerCount;
        }
        else
        {
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            dsvDesc.Texture2D.MipSlice = desc.baseMipLevel;
        }

        m_d3dDevice->CreateDepthStencilView(tex->m_d3dTexture.Get(),
                                            &dsvDesc,
                                            handle);
        view->m_d3dDsvHandle = handle;
    }

    return view;
#else
    (void)desc;
    return {};
#endif
}

// ============================================================================
// d3d12MakeSampler
// ============================================================================

rcp<Sampler> Context::d3d12MakeSampler(const SamplerDesc& desc)
{
#if defined(ORE_BACKEND_D3D12)
    auto sampler = rcp<Sampler>(new Sampler());
    sampler->m_d3dOreContext = this;

    D3D12_CPU_DESCRIPTOR_HANDLE handle =
        m_d3dCpuSamplerHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += (SIZE_T)m_d3dCpuSamplerAllocated++ * m_d3dSamplerDescSize;

    D3D12_SAMPLER_DESC sd = {};
    sd.Filter = oreFilterToD3D12(desc.minFilter,
                                 desc.magFilter,
                                 desc.mipmapFilter,
                                 desc.compare,
                                 desc.maxAnisotropy);
    sd.AddressU = oreWrapToD3D12(desc.wrapU);
    sd.AddressV = oreWrapToD3D12(desc.wrapV);
    sd.AddressW = oreWrapToD3D12(desc.wrapW);
    sd.MaxAnisotropy = desc.maxAnisotropy;
    sd.ComparisonFunc = (desc.compare != CompareFunction::none)
                            ? oreCompareFunctionToD3D12(desc.compare)
                            : D3D12_COMPARISON_FUNC_ALWAYS;
    sd.MinLOD = desc.minLod;
    sd.MaxLOD = desc.maxLod;

    m_d3dDevice->CreateSampler(&sd, handle);
    sampler->m_d3dSamplerHandle = handle;

    return sampler;
#else
    (void)desc;
    return {};
#endif
}

// ============================================================================
// d3d12MakeShaderModule
// ============================================================================

rcp<ShaderModule> Context::d3d12MakeShaderModule(const ShaderModuleDesc& desc)
{
#if defined(ORE_BACKEND_D3D12)
    auto module = rcp<ShaderModule>(new ShaderModule());

    // HLSL source path: compile via D3DCompile in-process. This mirrors the
    // D3D11 backend — the RSTB ships HLSL source for SM5 (not pre-compiled
    // DXBC) because compiling in the same process avoids AMD driver issues
    // with cross-process bytecode.
    if (desc.hlslSource && desc.hlslSourceSize > 0)
    {
        // Normalize line endings — SPIRV-Cross emits \r\n on Windows, which
        // can confuse D3DCompile's preprocessor.
        std::string source(desc.hlslSource, desc.hlslSourceSize);
        source.erase(std::remove(source.begin(), source.end(), '\r'),
                     source.end());

        const char* target =
            (desc.stage == ShaderStage::fragment) ? "ps_5_0" : "vs_5_0";
        const char* entry = (desc.hlslEntryPoint && *desc.hlslEntryPoint)
                                ? desc.hlslEntryPoint
                                : "main";
        ComPtr<ID3DBlob> compiled;
        ComPtr<ID3DBlob> errors;
        HRESULT hr = D3DCompile(source.c_str(),
                                source.size(),
                                nullptr,
                                nullptr,
                                nullptr,
                                entry,
                                target,
                                D3DCOMPILE_ENABLE_STRICTNESS |
                                    D3DCOMPILE_OPTIMIZATION_LEVEL3,
                                0,
                                compiled.GetAddressOf(),
                                errors.GetAddressOf());
        if (FAILED(hr))
        {
            const char* errMsg =
                errors ? static_cast<const char*>(errors->GetBufferPointer())
                       : "(no error log)";
            setLastError(
                "Ore D3D12: D3DCompile failed (entry=%s target=%s hr=0x%08x): "
                "%s",
                entry,
                target,
                static_cast<unsigned>(hr),
                errMsg);
            return nullptr;
        }

        const uint8_t* bytes =
            static_cast<const uint8_t*>(compiled->GetBufferPointer());
        module->m_d3dBytecode.assign(bytes, bytes + compiled->GetBufferSize());
        module->m_d3dIsVertex = (desc.stage == ShaderStage::vertex);
        module->applyBindingMapFromDesc(desc);
        return module;
    }

    // Pre-compiled DXBC path.
    const uint8_t* code = static_cast<const uint8_t*>(desc.code);
    module->m_d3dBytecode.assign(code, code + desc.codeSize);

    // Probe bytecode to determine VS vs PS.
    // DXBC programs have a 4-byte "DXBC" magic at offset 0 and a program type
    // DWORD at offset 0x0C: 0xFFFF = PS, 0xFFFE = VS.
    if (desc.codeSize >= 0x10)
    {
        const uint32_t* dwords = reinterpret_cast<const uint32_t*>(desc.code);
        uint32_t shaderType = dwords[3] >> 16; // High 16 bits of version token.
        module->m_d3dIsVertex = (shaderType == 0xFFFE);
    }

    module->applyBindingMapFromDesc(desc);
    return module;
#else
    (void)desc;
    return {};
#endif
}

// ============================================================================
// d3d12MakePipeline
// ============================================================================

rcp<Pipeline> Context::d3d12MakePipeline(const PipelineDesc& desc,
                                         std::string* outError)
{
    (void)outError;
#if defined(ORE_BACKEND_D3D12)
    auto pipeline = rcp<Pipeline>(new Pipeline(desc));
    pipeline->m_d3dOreContext = this;
    pipeline->m_d3dTopology = oreTopologyToD3D12(desc.topology);

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

    // Per-pipeline root sig: composed from user-supplied BindGroupLayouts.
    HRESULT rootSigHr = S_OK;
    const char* rootSigErrMsg = nullptr;
    pipeline->m_d3dRootSigOwned =
        buildPerPipelineRootSig(m_d3dDevice.Get(),
                                desc.bindGroupLayouts,
                                desc.bindGroupLayoutCount,
                                pipeline->m_d3dGroupParams,
                                &rootSigHr,
                                &rootSigErrMsg);
    if (!pipeline->m_d3dRootSigOwned)
    {
        // D3D12SerializeRootSignature/CreateRootSignature returned a
        // failure HRESULT. Most common in practice: the device has been
        // removed (TDR) or rejected the (otherwise valid) blob. Bail
        // out gracefully so the caller's `if (!pipeline)` path runs
        // instead of asserting in the helper. Per
        // `feedback_lua_gpu_misuse_validation` — driver-side failure
        // surfaces via `setLastError`, not a process abort.
        HRESULT removedReason = m_d3dDevice->GetDeviceRemovedReason();
        setLastError("makePipeline: D3D12 root signature build failed "
                     "(label=%s, hr=0x%08lX, msg=%s, "
                     "deviceRemovedReason=0x%08lX)",
                     desc.label ? desc.label : "(none)",
                     static_cast<unsigned long>(rootSigHr),
                     rootSigErrMsg ? rootSigErrMsg : "(none)",
                     static_cast<unsigned long>(removedReason));
        if (outError)
            *outError = lastError();
        return nullptr;
    }

    // Cache vertex strides for use in RenderPass::setVertexBuffer().
    for (uint32_t i = 0; i < desc.vertexBufferCount && i < 8; ++i)
        pipeline->m_d3dVertexStrides[i] = desc.vertexBuffers[i].stride;

    // --- Input layout elements ---
    D3D12_INPUT_ELEMENT_DESC elements[32] = {};
    UINT elementCount = 0;
    for (uint32_t bufIdx = 0; bufIdx < desc.vertexBufferCount; ++bufIdx)
    {
        const auto& layout = desc.vertexBuffers[bufIdx];
        for (uint32_t ai = 0; ai < layout.attributeCount; ++ai)
        {
            assert(elementCount < std::size(elements));
            const auto& attr = layout.attributes[ai];
            D3D12_INPUT_ELEMENT_DESC& el = elements[elementCount++];
            el.SemanticName = "TEXCOORD";
            el.SemanticIndex = attr.shaderSlot;
            el.Format = oreVertexFormatToDXGI12(attr.format);
            el.InputSlot = bufIdx;
            el.AlignedByteOffset = attr.offset;
            el.InputSlotClass =
                layout.stepMode == VertexStepMode::instance
                    ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA
                    : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
            el.InstanceDataStepRate =
                layout.stepMode == VertexStepMode::instance ? 1 : 0;
        }
    }

    // --- Blend state ---
    D3D12_BLEND_DESC blendDesc = {};
    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = (desc.colorCount > 1) ? TRUE : FALSE;
    for (uint32_t i = 0; i < desc.colorCount; ++i)
    {
        const auto& ct = desc.colorTargets[i];
        auto& rt = blendDesc.RenderTarget[i];
        rt.BlendEnable = ct.blendEnabled ? TRUE : FALSE;
        rt.SrcBlend = oreBlendFactorToD3D12(ct.blend.srcColor);
        rt.DestBlend = oreBlendFactorToD3D12(ct.blend.dstColor);
        rt.BlendOp = oreBlendOpToD3D12(ct.blend.colorOp);
        rt.SrcBlendAlpha = oreBlendFactorToD3D12(ct.blend.srcAlpha);
        rt.DestBlendAlpha = oreBlendFactorToD3D12(ct.blend.dstAlpha);
        rt.BlendOpAlpha = oreBlendOpToD3D12(ct.blend.alphaOp);
        rt.RenderTargetWriteMask = oreColorWriteMaskToD3D12(ct.writeMask);
        rt.LogicOpEnable = FALSE;
        rt.LogicOp = D3D12_LOGIC_OP_NOOP;
    }

    // --- Rasterizer state ---
    D3D12_RASTERIZER_DESC rastDesc = {};
    rastDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rastDesc.CullMode = (desc.cullMode == CullMode::none) ? D3D12_CULL_MODE_NONE
                        : (desc.cullMode == CullMode::front)
                            ? D3D12_CULL_MODE_FRONT
                            : D3D12_CULL_MODE_BACK;
    rastDesc.FrontCounterClockwise =
        (desc.winding == FaceWinding::counterClockwise) ? TRUE : FALSE;
    rastDesc.DepthBias = desc.depthStencil.depthBias;
    rastDesc.DepthBiasClamp = desc.depthStencil.depthBiasClamp;
    rastDesc.SlopeScaledDepthBias = desc.depthStencil.depthBiasSlopeScale;
    rastDesc.DepthClipEnable = TRUE;
    rastDesc.MultisampleEnable = (desc.sampleCount > 1) ? TRUE : FALSE;
    rastDesc.AntialiasedLineEnable = FALSE;

    // --- Depth/stencil state ---
    D3D12_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable =
        (desc.depthStencil.depthCompare != CompareFunction::always ||
         desc.depthStencil.depthWriteEnabled)
            ? TRUE
            : FALSE;
    dsDesc.DepthWriteMask = desc.depthStencil.depthWriteEnabled
                                ? D3D12_DEPTH_WRITE_MASK_ALL
                                : D3D12_DEPTH_WRITE_MASK_ZERO;
    dsDesc.DepthFunc =
        oreCompareFunctionToD3D12(desc.depthStencil.depthCompare);

    bool hasStencil = (desc.stencilFront.compare != CompareFunction::always ||
                       desc.stencilFront.failOp != StencilOp::keep ||
                       desc.stencilFront.depthFailOp != StencilOp::keep ||
                       desc.stencilFront.passOp != StencilOp::keep ||
                       desc.stencilBack.compare != CompareFunction::always ||
                       desc.stencilBack.failOp != StencilOp::keep ||
                       desc.stencilBack.depthFailOp != StencilOp::keep ||
                       desc.stencilBack.passOp != StencilOp::keep);
    dsDesc.StencilEnable = hasStencil ? TRUE : FALSE;
    dsDesc.StencilReadMask = desc.stencilReadMask;
    dsDesc.StencilWriteMask = desc.stencilWriteMask;

    auto fillStencilOp = [](D3D12_DEPTH_STENCILOP_DESC& out,
                            const StencilFaceState& s) {
        out.StencilFailOp = oreStencilOpToD3D12(s.failOp);
        out.StencilDepthFailOp = oreStencilOpToD3D12(s.depthFailOp);
        out.StencilPassOp = oreStencilOpToD3D12(s.passOp);
        out.StencilFunc = oreCompareFunctionToD3D12(s.compare);
    };
    fillStencilOp(dsDesc.FrontFace, desc.stencilFront);
    fillStencilOp(dsDesc.BackFace, desc.stencilBack);

    // --- Determine DSV and MSAA formats ---
    DXGI_FORMAT dsvFormat = DXGI_FORMAT_UNKNOWN;
    if (dsDesc.DepthEnable || hasStencil)
        dsvFormat = oreFormatInfoD3D12(desc.depthStencil.format).dsv;

    // --- Build PSO ---
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = pipeline->m_d3dRootSigOwned.Get();
    psoDesc.VS.pShaderBytecode = desc.vertexModule->m_d3dBytecode.data();
    psoDesc.VS.BytecodeLength = desc.vertexModule->m_d3dBytecode.size();
    if (desc.fragmentModule != nullptr)
    {
        psoDesc.PS.pShaderBytecode = desc.fragmentModule->m_d3dBytecode.data();
        psoDesc.PS.BytecodeLength = desc.fragmentModule->m_d3dBytecode.size();
    }
    // else: psoDesc.PS stays zero — D3D12 allows null PS for depth-only.
    psoDesc.BlendState = blendDesc;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.RasterizerState = rastDesc;
    psoDesc.DepthStencilState = dsDesc;
    psoDesc.InputLayout = {elements, elementCount};
    psoDesc.PrimitiveTopologyType = oreTopologyTypeToD3D12(desc.topology);
    psoDesc.NumRenderTargets = desc.colorCount;
    for (uint32_t i = 0; i < desc.colorCount; ++i)
        psoDesc.RTVFormats[i] =
            oreFormatInfoD3D12(desc.colorTargets[i].format).rtv;
    psoDesc.DSVFormat = dsvFormat;
    psoDesc.SampleDesc.Count = desc.sampleCount;
    psoDesc.SampleDesc.Quality = 0;

    HRESULT hr = m_d3dDevice->CreateGraphicsPipelineState(
        &psoDesc,
        IID_PPV_ARGS(pipeline->m_d3dPSO.GetAddressOf()));
    if (FAILED(hr))
    {
        if (outError)
            *outError = "CreateGraphicsPipelineState failed";
        return nullptr;
    }

    return pipeline;
#else
    (void)desc;
    return {};
#endif
}

// =====================================================================

// ============================================================================
// d3d12MakeBindGroup
// ============================================================================

rcp<BindGroup> Context::d3d12MakeBindGroup(const BindGroupDesc& desc)
{
#if defined(ORE_BACKEND_D3D12)
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

    // Count dynamic-offset UBOs declared by the layout — authoritative.
    uint32_t dynamicCount = 0;
    for (const auto& e : layout->entries())
    {
        if (e.kind == BindingKind::uniformBuffer && e.hasDynamicOffset)
            ++dynamicCount;
    }
    bg->m_dynamicOffsetCount = dynamicCount;

    // Native HLSL register slot per binding comes from the layout's
    // pre-resolved nativeSlotVS/FS (D3D12 root tables are stage-shared,
    // so VS slot suffices; fall back to FS for FS-only resources).
    auto nativeSlot =
        [&](uint32_t binding, BindingKind expected, uint32_t* outSlot) -> bool {
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
        uint32_t s =
            (le->nativeSlotVS != BindGroupLayoutEntry::kNativeSlotAbsent)
                ? le->nativeSlotVS
                : le->nativeSlotFS;
        if (s == BindGroupLayoutEntry::kNativeSlotAbsent)
        {
            setLastError("makeBindGroup: (group=%u, binding=%u) layout has "
                         "no resolved native slot — call "
                         "makeLayoutFromShader",
                         groupIndex,
                         binding);
            return false;
        }
        *outSlot = s;
        return true;
    };

    // UBO bindings.
    for (uint32_t i = 0; i < desc.uboCount; ++i)
    {
        const auto& entry = desc.ubos[i];
        assert(entry.buffer != nullptr);
        uint32_t slot = 0;
        if (!nativeSlot(entry.slot, BindingKind::uniformBuffer, &slot))
            continue;
        assert(slot < 8);
        bg->m_d3dUBOAddresses[slot] =
            entry.buffer->m_d3dBuffer->GetGPUVirtualAddress() + entry.offset;
        bg->m_d3dUBOSizes[slot] =
            entry.size > 0 ? entry.size
                           : static_cast<uint32_t>(entry.buffer->size());
        bg->m_d3dUBOSlotMask |= static_cast<uint8_t>(1u << slot);
        if (layout->hasDynamicOffset(entry.slot))
            bg->m_d3dUBODynamicOffsetMask |= static_cast<uint8_t>(1u << slot);
        bg->m_retainedBuffers.push_back(ref_rcp(entry.buffer));
    }

    // Texture bindings.
    for (uint32_t i = 0; i < desc.textureCount; ++i)
    {
        const auto& entry = desc.textures[i];
        assert(entry.view != nullptr);
        uint32_t slot = 0;
        if (!nativeSlot(entry.slot, BindingKind::sampledTexture, &slot))
            continue;
        assert(slot < 8);
        bg->m_d3dSrvHandles[slot] = entry.view->m_d3dSrvHandle;
        bg->m_d3dSrvTextures[slot] = entry.view->texture();
        bg->m_d3dSrvSlotMask |= static_cast<uint8_t>(1u << slot);
        bg->m_retainedViews.push_back(ref_rcp(entry.view));
    }

    // Sampler bindings.
    for (uint32_t i = 0; i < desc.samplerCount; ++i)
    {
        const auto& entry = desc.samplers[i];
        assert(entry.sampler != nullptr);
        uint32_t slot = 0;
        if (!nativeSlot(entry.slot, BindingKind::sampler, &slot))
            continue;
        assert(slot < 8);
        bg->m_d3dSamplerHandles[slot] = entry.sampler->m_d3dSamplerHandle;
        bg->m_d3dSamplerSlotMask |= static_cast<uint8_t>(1u << slot);
        bg->m_retainedSamplers.push_back(ref_rcp(entry.sampler));
    }

    return bg;
#else
    (void)desc;
    return nullptr;
#endif
}

// ============================================================================
// d3d12MakeBindGroupLayout
// ============================================================================

rcp<BindGroupLayout> Context::d3d12MakeBindGroupLayout(
    const BindGroupLayoutDesc& desc)
{
#if defined(ORE_BACKEND_D3D12)
    if (desc.groupIndex >= kMaxBindGroups)
    {
        setLastError("makeBindGroupLayout: groupIndex %u out of range [0, %u)",
                     desc.groupIndex,
                     kMaxBindGroups);
        return nullptr;
    }
    auto layout = rcp<BindGroupLayout>(new BindGroupLayout());
    layout->m_context = this;
    layout->m_groupIndex = desc.groupIndex;
    layout->m_entries.reserve(desc.entryCount);
    for (uint32_t i = 0; i < desc.entryCount; ++i)
        layout->m_entries.push_back(desc.entries[i]);

    // Pre-compute D3D12 per-group descriptor-table shape from the user's
    // entries. The pipeline's root sig will compose these at makePipeline
    // time, assigning final root-parameter indices.
    tallyGroupFromLayoutDesc(desc, &layout->m_d3dGroupParams);
    return layout;
#else
    (void)desc;
    return nullptr;
#endif
}

// ============================================================================
// d3d12BeginRenderPass
// ============================================================================

RenderPass Context::d3d12BeginRenderPass(const RenderPassDesc& desc,
                                         std::string* outError)
{
#if defined(ORE_BACKEND_D3D12)
    // Flush any pending texture uploads before rendering.
    d3d12FlushUploads();

    RenderPass pass;
    pass.m_context = this;
    pass.m_d3dCmdList = m_d3dCmdList;
    pass.m_d3dDevice = m_d3dDevice.Get();
    pass.m_d3dContext = this;
    // `m_d3dCurrentRootSig` starts nullptr; the first `setPipeline`
    // calls `SetGraphicsRootSignature` with the pipeline's sig (every
    // pipeline ships with its own per RFC v5 §3.2.2).
    pass.populateAttachmentMetadata(desc);

    // Collect RTVs and DSV handles, transitioning resources to the correct
    // state.
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[4] = {};
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = {};
    bool hasDsv = false;

    for (uint32_t i = 0; i < desc.colorCount; ++i)
    {
        TextureView* view = desc.colorAttachments[i].view;
        assert(view != nullptr);

        Texture* tex = view->texture();
        if (tex->m_d3dCurrentState != D3D12_RESOURCE_STATE_RENDER_TARGET)
        {
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = tex->m_d3dTexture.Get();
            barrier.Transition.StateBefore = tex->m_d3dCurrentState;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.Subresource =
                D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            m_d3dCmdList->ResourceBarrier(1, &barrier);
            tex->m_d3dCurrentState = D3D12_RESOURCE_STATE_RENDER_TARGET;
        }

        rtvHandles[i] = view->m_d3dRtvHandle;

        // Record resource + final state for finish() to transition back.
        pass.m_d3dColorResources[i] = tex->m_d3dTexture.Get();
        pass.m_d3dColorTextures[i] = tex;
        // External (canvas) textures go back to COMMON; ORE-owned go to
        // PIXEL_SHADER_RESOURCE.
        const D3D12_RESOURCE_STATES colourFinalState =
            tex->m_d3dIsExternal ? D3D12_RESOURCE_STATE_COMMON
                                 : D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        pass.m_d3dColorFinalStates[i] = colourFinalState;
        pass.m_d3dColorCount++;

        // Record MSAA resolve pair if present. D3D12 subresource index
        // for the common single-plane case is mipLevel + arraySlice*mipCount.
        TextureView* resolveView = desc.colorAttachments[i].resolveTarget;
        if (resolveView != nullptr && resolveView->texture() != nullptr)
        {
            Texture* resolveTex = resolveView->texture();
            auto& r = pass.m_d3dResolves[pass.m_d3dResolveCount++];
            r.msaa = tex->m_d3dTexture.Get();
            r.resolve = resolveTex->m_d3dTexture.Get();
            r.resolveTex = resolveTex;
            r.format = oreFormatInfoD3D12(resolveTex->format()).resource;
            const uint32_t msaaMips = tex->numMipmaps();
            const uint32_t resolveMips = resolveTex->numMipmaps();
            r.msaaSubresource =
                view->baseMipLevel() + view->baseLayer() * msaaMips;
            r.resolveSubresource = resolveView->baseMipLevel() +
                                   resolveView->baseLayer() * resolveMips;
            r.resolvePriorState = resolveTex->m_d3dCurrentState;
            r.resolveFinalState =
                resolveTex->m_d3dIsExternal
                    ? D3D12_RESOURCE_STATE_COMMON
                    : D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        }
    }

    if (desc.depthStencil.view)
    {
        TextureView* dsView = desc.depthStencil.view;
        Texture* dsTex = dsView->texture();

        D3D12_RESOURCE_STATES dsTarget = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        if (dsTex->m_d3dCurrentState != dsTarget)
        {
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = dsTex->m_d3dTexture.Get();
            barrier.Transition.StateBefore = dsTex->m_d3dCurrentState;
            barrier.Transition.StateAfter = dsTarget;
            barrier.Transition.Subresource =
                D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            m_d3dCmdList->ResourceBarrier(1, &barrier);
            dsTex->m_d3dCurrentState = dsTarget;
        }

        dsvHandle = dsView->m_d3dDsvHandle;
        hasDsv = true;

        pass.m_d3dDepthResource = dsTex->m_d3dTexture.Get();
        pass.m_d3dDepthTexture = dsTex;
        // When the depth is stored the caller intends to sample it as a
        // shader resource (e.g. shadow map read-back), so transition to
        // PIXEL_SHADER_RESOURCE.  Otherwise DEPTH_READ is sufficient.
        pass.m_d3dDepthFinalState =
            (desc.depthStencil.depthStoreOp == StoreOp::store)
                ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
                : D3D12_RESOURCE_STATE_DEPTH_READ;
    }

    m_d3dCmdList->OMSetRenderTargets(desc.colorCount,
                                     rtvHandles,
                                     FALSE,
                                     hasDsv ? &dsvHandle : nullptr);

    // Handle clear ops.
    for (uint32_t i = 0; i < desc.colorCount; ++i)
    {
        const auto& ca = desc.colorAttachments[i];
        if (ca.loadOp == LoadOp::clear)
        {
            float c[4] = {ca.clearColor.r,
                          ca.clearColor.g,
                          ca.clearColor.b,
                          ca.clearColor.a};
            m_d3dCmdList->ClearRenderTargetView(rtvHandles[i], c, 0, nullptr);
        }
    }

    // Default viewport + scissor covering the full attachment. D3D12
    // initialises viewport/scissor to zero on a fresh command list which
    // clips all geometry. Metal defaults to the full attachment size, so
    // scripts that omit setViewport() work on Metal but would produce empty
    // output on D3D12 without this. Mirrors d3d11BeginRenderPass.
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
            D3D12_VIEWPORT vp = {0.f,
                                 0.f,
                                 static_cast<float>(w),
                                 static_cast<float>(h),
                                 0.f,
                                 1.f};
            m_d3dCmdList->RSSetViewports(1, &vp);
            D3D12_RECT scissor = {0, 0, (LONG)w, (LONG)h};
            m_d3dCmdList->RSSetScissorRects(1, &scissor);
        }
    }

    if (hasDsv && desc.depthStencil.view)
    {
        D3D12_CLEAR_FLAGS clearFlags = {};
        if (desc.depthStencil.depthLoadOp == LoadOp::clear)
            clearFlags |= D3D12_CLEAR_FLAG_DEPTH;
        TextureFormat depthFmt = desc.depthStencil.view->texture()->format();
        bool hasStencil = (depthFmt == TextureFormat::depth24plusStencil8 ||
                           depthFmt == TextureFormat::depth32floatStencil8);
        if (hasStencil && desc.depthStencil.stencilLoadOp == LoadOp::clear)
            clearFlags |= D3D12_CLEAR_FLAG_STENCIL;
        if (clearFlags)
            m_d3dCmdList->ClearDepthStencilView(
                dsvHandle,
                clearFlags,
                desc.depthStencil.depthClearValue,
                static_cast<UINT8>(desc.depthStencil.stencilClearValue),
                0,
                nullptr);
    }

    return pass;
#else
    (void)desc;
    return RenderPass{};
#endif
}

// ============================================================================
// d3d12WrapCanvasTexture
// ============================================================================

rcp<TextureView> Context::d3d12WrapCanvasTexture(gpu::RenderCanvas* canvas)
{
#if defined(ORE_BACKEND_D3D12)
    assert(canvas != nullptr);

    auto* d3dTarget =
        static_cast<gpu::RenderTargetD3D12*>(canvas->renderTarget());
    gpu::D3D12Texture* d3dTex = d3dTarget->targetTexture();
    assert(d3dTex != nullptr);

    DXGI_FORMAT dxgiFmt = d3dTex->format();

    TextureFormat oreFormat;
    switch (dxgiFmt)
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
    texture->m_d3dTexture =
        d3dTex->resource(); // Borrow — RenderCanvas owns it.
    texture->m_d3dCurrentState = D3D12_RESOURCE_STATE_COMMON;
    texture->m_d3dIsExternal = true;
    texture->m_d3dDevice = m_d3dDevice.Get();
    texture->m_d3dOreContext = this;

    TextureViewDesc viewDesc{};
    viewDesc.texture = texture.get();
    viewDesc.dimension = TextureViewDimension::texture2D;
    viewDesc.baseMipLevel = 0;
    viewDesc.mipCount = 1;
    viewDesc.baseLayer = 0;
    viewDesc.layerCount = 1;

    auto view = rcp<TextureView>(new TextureView(std::move(texture), viewDesc));
    view->m_d3dOreContext = this;

    // Create the RTV in our CPU RTV heap.
    D3D12_CPU_DESCRIPTOR_HANDLE handle =
        m_d3dCpuRtvHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += (SIZE_T)m_d3dCpuRtvAllocated++ * m_d3dRtvDescSize;

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = dxgiFmt;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = 0;

    m_d3dDevice->CreateRenderTargetView(d3dTex->resource(), &rtvDesc, handle);
    view->m_d3dRtvHandle = handle;

    return view;
#else
    (void)canvas;
    return {};
#endif
}

rcp<TextureView> Context::d3d12WrapRiveTexture(gpu::Texture* gpuTex,
                                               uint32_t w,
                                               uint32_t h)
{
#if defined(ORE_BACKEND_D3D12)
    if (!gpuTex)
        return nullptr;

    // nativeHandle() returns a D3D12Texture* for D3D12.
    auto* d3dTex = static_cast<gpu::D3D12Texture*>(gpuTex->nativeHandle());
    if (!d3dTex)
        return nullptr;

    DXGI_FORMAT dxgiFmt = d3dTex->format();

    TextureFormat oreFormat;
    switch (dxgiFmt)
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
    texDesc.width = w;
    texDesc.height = h;
    texDesc.format = oreFormat;
    texDesc.type = TextureType::texture2D;
    texDesc.renderTarget = false;
    texDesc.numMipmaps = 1;
    texDesc.sampleCount = 1;

    auto texture = rcp<Texture>(new Texture(texDesc));
    texture->m_d3dTexture = d3dTex->resource(); // Borrow.
    texture->m_d3dCurrentState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    texture->m_d3dIsExternal = true;
    texture->m_d3dDevice = m_d3dDevice.Get();
    texture->m_d3dOreContext = this;

    // In external-CL mode the Rive texture may be in any state coming out of
    // Rive's last recording (e.g. RENDER_TARGET from a PLS flush). Emit a
    // transition into the same CL so the draw we're about to record sees it
    // in PIXEL_SHADER_RESOURCE. This is the D3D12 analogue of the Vulkan
    // wrapRiveTexture barrier — but we only do it when external-CL mode is
    // active, because in owned-CL mode the Ore CL is submitted before Rive's
    // CL and a barrier recorded here would reference a state the texture
    // isn't actually in at submission time.
    if (m_d3dExternalCmdList)
    {
        assert(m_d3dCmdList != nullptr);
        auto* manager =
            static_cast<gpu::D3D12ResourceManager*>(d3dTex->manager());
        if (manager != nullptr &&
            d3dTex->lastState() != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
        {
            manager->transition(m_d3dCmdList,
                                d3dTex,
                                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }
    }

    TextureViewDesc viewDesc{};
    viewDesc.texture = texture.get();
    viewDesc.dimension = TextureViewDimension::texture2D;
    viewDesc.baseMipLevel = 0;
    viewDesc.mipCount = 1;
    viewDesc.baseLayer = 0;
    viewDesc.layerCount = 1;

    auto view = rcp<TextureView>(new TextureView(std::move(texture), viewDesc));
    view->m_d3dOreContext = this;
    // Create SRV for sampling (images are read-only, no RTV needed).
    if (m_d3dCpuSrvAllocated >= 1024)
        return nullptr; // CPU SRV descriptor heap exhausted.
    D3D12_CPU_DESCRIPTOR_HANDLE srvHandle =
        m_d3dCpuSrvHeap->GetCPUDescriptorHandleForHeapStart();
    srvHandle.ptr += (SIZE_T)m_d3dCpuSrvAllocated++ * m_d3dSrvDescSize;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = dxgiFmt;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;

    m_d3dDevice->CreateShaderResourceView(d3dTex->resource(),
                                          &srvDesc,
                                          srvHandle);
    view->m_d3dSrvHandle = srvHandle;
    return view;
#else
    (void)gpuTex;
    (void)w;
    (void)h;
    return nullptr;
#endif
}

// ============================================================================
// Public method definitions (D3D12-only builds)
// When both D3D11 and D3D12 are compiled, the combined
// ore_context_d3d11_d3d12.cpp file provides these methods with dispatch.
// ============================================================================

#if defined(ORE_BACKEND_D3D12) && !defined(ORE_BACKEND_D3D11)

rcp<Buffer> Context::makeBuffer(const BufferDesc& desc)
{
    return d3d12MakeBuffer(desc);
}

rcp<Texture> Context::makeTexture(const TextureDesc& desc)
{
    return d3d12MakeTexture(desc);
}

rcp<TextureView> Context::makeTextureView(const TextureViewDesc& desc)
{
    return d3d12MakeTextureView(desc);
}

rcp<Sampler> Context::makeSampler(const SamplerDesc& desc)
{
    return d3d12MakeSampler(desc);
}

rcp<ShaderModule> Context::makeShaderModule(const ShaderModuleDesc& desc)
{
    return d3d12MakeShaderModule(desc);
}

rcp<Pipeline> Context::makePipeline(const PipelineDesc& desc,
                                    std::string* outError)
{
    return d3d12MakePipeline(desc, outError);
}

rcp<BindGroup> Context::makeBindGroup(const BindGroupDesc& desc)
{
    return d3d12MakeBindGroup(desc);
}

rcp<BindGroupLayout> Context::makeBindGroupLayout(
    const BindGroupLayoutDesc& desc)
{
    return d3d12MakeBindGroupLayout(desc);
}

RenderPass Context::beginRenderPass(const RenderPassDesc& desc,
                                    std::string* outError)
{
    finishActiveRenderPass();
    return d3d12BeginRenderPass(desc, outError);
}

rcp<TextureView> Context::wrapCanvasTexture(gpu::RenderCanvas* canvas)
{
    return d3d12WrapCanvasTexture(canvas);
}

rcp<TextureView> Context::wrapRiveTexture(gpu::Texture* gpuTex,
                                          uint32_t w,
                                          uint32_t h)
{
    return d3d12WrapRiveTexture(gpuTex, w, h);
}

#endif // D3D12-only (public method stubs)

} // namespace rive::ore
