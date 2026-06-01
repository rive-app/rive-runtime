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

void TextureD3D12::upload(const TextureDataDesc& data)
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
    if (data.data == nullptr)
    {
        ctx->setLastError("upload: data is null");
        return;
    }

    const uint32_t bpt = textureFormatBytesPerTexel(m_format);
    // mipLevel must index a declared level.
    if (data.mipLevel >= m_numMipmaps)
    {
        ctx->setLastError("upload: mipLevel (%u) >= numMipmaps (%u)",
                          data.mipLevel,
                          m_numMipmaps);
        return;
    }
    // layer must index a declared slice (1 for non-array/non-cube).
    if (data.layer >= m_depthOrArrayLayers)
    {
        ctx->setLastError("upload: layer (%u) >= depthOrArrayLayers (%u)",
                          data.layer,
                          m_depthOrArrayLayers);
        return;
    }
    // Mip-adjusted extents (floor(max(1, dim >> mipLevel))).
    const uint32_t mipWidth =
        (m_width >> data.mipLevel) > 0 ? (m_width >> data.mipLevel) : 1u;
    const uint32_t mipHeight =
        (m_height >> data.mipLevel) > 0 ? (m_height >> data.mipLevel) : 1u;
    const uint32_t width = data.width > 0 ? data.width : mipWidth;
    const uint32_t height = data.height > 0 ? data.height : mipHeight;
    // Only 3D carries depth > 1. The 2D family picks its slice via the
    // subresource index and uploads at depth 1.
    const uint32_t maxDepth =
        m_type == TextureType::texture3D ? m_depthOrArrayLayers : 1u;
    const uint32_t depth = data.depth > 0 ? data.depth : maxDepth;
    // Region must fit the mip extent. 64-bit so a big offset can't wrap.
    if (static_cast<uint64_t>(data.x) + width > mipWidth ||
        static_cast<uint64_t>(data.y) + height > mipHeight)
    {
        ctx->setLastError("upload: region (x=%u y=%u w=%u h=%u) out of bounds "
                          "for mip %u (%ux%u)",
                          data.x,
                          data.y,
                          width,
                          height,
                          data.mipLevel,
                          mipWidth,
                          mipHeight);
        return;
    }
    if (static_cast<uint64_t>(data.z) + depth > maxDepth)
    {
        ctx->setLastError(
            "upload: z-region (z=%u depth=%u) out of bounds (maxDepth=%u)",
            data.z,
            depth,
            maxDepth);
        return;
    }
    // bytesPerTexel == 0 is block-compressed. No block-aware path yet.
    if (bpt == 0)
    {
        ctx->setLastError("upload: block-compressed formats not yet supported");
        return;
    }
    if (data.bytesPerRow != 0 && (data.bytesPerRow % bpt) != 0)
    {
        ctx->setLastError("upload: bytesPerRow (%u) must be a whole number of "
                          "texels (bytesPerTexel=%u)",
                          data.bytesPerRow,
                          bpt);
        return;
    }
    if (data.bytesPerRow != 0 &&
        data.bytesPerRow < static_cast<uint64_t>(width) * bpt)
    {
        ctx->setLastError(
            "upload: bytesPerRow (%u) < width * bytesPerTexel (%llu)",
            data.bytesPerRow,
            static_cast<unsigned long long>(static_cast<uint64_t>(width) *
                                            bpt));
        return;
    }
    if (data.rowsPerImage > 0 && data.rowsPerImage < height)
    {
        ctx->setLastError("upload: rowsPerImage (%u) < height (%u)",
                          data.rowsPerImage,
                          height);
        return;
    }

    // Subresource index: mipSlice + arraySlice * MipLevels. Transposing it
    // only works when MipLevels == 1.
    D3D12_RESOURCE_DESC texDesc = m_d3dTexture->GetDesc();
    const UINT subresource =
        data.mipLevel + data.layer * static_cast<UINT>(texDesc.MipLevels);

    // Stage just the caller region: caller pitch on read, 256-aligned pitch on
    // write. 64-bit so the total can't wrap before the uint32_t guard.
    const uint64_t srcRow = data.bytesPerRow != 0
                                ? data.bytesPerRow
                                : static_cast<uint64_t>(width) * bpt;
    const uint32_t rowsPerImage =
        data.rowsPerImage > 0 ? data.rowsPerImage : height;
    const uint64_t srcSliceStride = srcRow * rowsPerImage;
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

    // If the texture is not in COPY_DEST state, transition it.
    if (m_d3dCurrentState != D3D12_RESOURCE_STATE_COPY_DEST)
    {
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = m_d3dTexture.Get();
        barrier.Transition.StateBefore = m_d3dCurrentState;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.Subresource =
            D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        ctx->m_d3dCmdList->ResourceBarrier(1, &barrier);
        m_d3dCurrentState = D3D12_RESOURCE_STATE_COPY_DEST;
    }

    // Copy the whole staged region to (x, y, z). No src box needed.
    D3D12_TEXTURE_COPY_LOCATION dst_loc = {};
    dst_loc.pResource = m_d3dTexture.Get();
    dst_loc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst_loc.SubresourceIndex = subresource;

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
    footprint.Offset = 0;
    footprint.Footprint.Format = texDesc.Format;
    footprint.Footprint.Width = width;
    footprint.Footprint.Height = height;
    footprint.Footprint.Depth = depth;
    footprint.Footprint.RowPitch = static_cast<UINT>(dstRowPitch);

    D3D12_TEXTURE_COPY_LOCATION src_loc = {};
    src_loc.pResource = m_uploadBuffer->m_d3dBuffer.Get();
    src_loc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src_loc.PlacedFootprint = footprint;

    ctx->m_d3dCmdList->CopyTextureRegion(&dst_loc,
                                         data.x,
                                         data.y,
                                         data.z,
                                         &src_loc,
                                         nullptr);

    // Transition to PIXEL_SHADER_RESOURCE so it's ready to sample without
    // an explicit barrier in the render pass.
    {
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = m_d3dTexture.Get();
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.StateAfter =
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barrier.Transition.Subresource =
            D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        ctx->m_d3dCmdList->ResourceBarrier(1, &barrier);
        m_d3dCurrentState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    }
#else
    (void)data;
#endif
}
} // namespace rive::ore
