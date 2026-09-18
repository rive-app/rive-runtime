/*
 * Copyright 2025 Rive
 */

#include "ore_texture_d3d12.hpp"
#include "ore_buffer_d3d12.hpp"
#include "rive/renderer/ore/ore_context_d3d12.hpp"
#include "rive/rive_types.hpp"

#include <d3d12.h>
#include <wrl/client.h>
#include <cassert>
#include <cstdint>
#include <cstring>

using Microsoft::WRL::ComPtr;

namespace rive::ore
{

void TextureD3D12::uploadImpl(const TextureDataDesc& data)
{
#if defined(ORE_BACKEND_D3D12)
    assert(m_d3dOreContext != nullptr);
    ContextD3D12* ctx = m_d3dOreContext;
    // Internal invariants: a live, non-external texture always has these.
    assert(m_d3dTexture != nullptr);
    assert(m_d3dDevice != nullptr);

    // Script-reachable failures report through lastError instead of aborting.
    if (m_d3dIsExternal)
    {
        ctx->setLastError(
            "upload: cannot upload into an external (canvas-wrapped) texture");
        return;
    }
    const uint32_t bpt = textureFormatBytesPerTexel(m_format);
    // No block-aware path yet.
    if (bpt == 0)
    {
        ctx->setLastError("upload: block-compressed formats not yet supported");
        return;
    }
    const uint32_t width = data.width;
    const uint32_t height = data.height;
    const uint32_t depth = data.depth;

    // Subresource index: mipSlice + arraySlice * MipLevels. Transposing it
    // only works when MipLevels == 1.
    D3D12_RESOURCE_DESC texDesc = m_d3dTexture->GetDesc();
    const UINT subresource =
        data.mipLevel + data.layer * static_cast<UINT>(texDesc.MipLevels);

    // Stage just the caller region: caller pitch on read, 256-aligned pitch on
    // write. 64-bit so the total can't wrap before the uint32_t guard.
    const uint64_t srcRow = data.bytesPerRow;
    const uint64_t srcSliceStride = srcRow * data.rowsPerImage;
    const uint64_t copyRowBytes = static_cast<uint64_t>(width) * bpt;
    const uint64_t dstRowPitch =
        (copyRowBytes + (D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1)) &
        ~static_cast<uint64_t>(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);
    const uint64_t totalBytes = dstRowPitch * height * depth;
    // BufferD3D12 size is uint32_t. Refuse rather than truncate.
    if (totalBytes > UINT32_MAX)
    {
        ctx->setLastError(
            "upload: size (%llu) exceeds uint32_t staging buffer max",
            static_cast<unsigned long long>(totalBytes));
        return;
    }

    // Allocate a staging (UPLOAD heap) buffer sized to the region footprint.
    D3D12_HEAP_PROPERTIES uploadHeap = {};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC bufDesc = {};
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Width = totalBytes;
    bufDesc.Height = 1;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.MipLevels = 1;
    bufDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufDesc.SampleDesc.Count = 1;
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    m_uploadBuffer =
        rcp<BufferD3D12>(new BufferD3D12(m_manager,
                                         static_cast<uint32_t>(totalBytes),
                                         BufferUsage::upload));

    HRESULT hr = m_d3dDevice->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &bufDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(m_uploadBuffer->m_d3dBuffer.GetAddressOf()));
    if (FAILED(hr))
    {
        m_uploadBuffer = nullptr;
        ctx->setLastError("upload: staging buffer creation failed (hr=0x%08x)",
                          static_cast<unsigned>(hr));
        return;
    }

    void* mapped = nullptr;
    D3D12_RANGE readRange = {0, 0};
    hr = m_uploadBuffer->m_d3dBuffer->Map(0, &readRange, &mapped);
    if (FAILED(hr) || mapped == nullptr)
    {
        m_uploadBuffer = nullptr;
        ctx->setLastError("upload: staging buffer map failed (hr=0x%08x)",
                          static_cast<unsigned>(hr));
        return;
    }

    // Copy the region row by row, never the full subresource (the over-read).
    const uint8_t* src = static_cast<const uint8_t*>(data.data);
    uint8_t* dst = static_cast<uint8_t*>(mapped);
    for (uint32_t z = 0; z < depth; ++z)
    {
        for (uint32_t y = 0; y < height; ++y)
        {
            memcpy(dst + (static_cast<uint64_t>(z) * height + y) * dstRowPitch,
                   src + static_cast<uint64_t>(z) * srcSliceStride +
                       static_cast<uint64_t>(y) * srcRow,
                   static_cast<size_t>(copyRowBytes));
        }
    }
    m_uploadBuffer->m_d3dBuffer->Unmap(0, nullptr);

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
    footprint.Offset = 0;
    footprint.Footprint.Format = texDesc.Format;
    footprint.Footprint.Width = width;
    footprint.Footprint.Height = height;
    footprint.Footprint.Depth = depth;
    footprint.Footprint.RowPitch = static_cast<UINT>(dstRowPitch);

    // Callers stage uploads before an Ore frame, when the host command list is
    // closed, so queue the copy to record onto a live list once one opens.
    ctx->d3d12QueuePendingTextureUpload({
        ref_rcp(this),
        m_uploadBuffer,
        footprint,
        subresource,
        data.x,
        data.y,
        data.z,
    });
#else
    (void)data;
#endif
}
} // namespace rive::ore
