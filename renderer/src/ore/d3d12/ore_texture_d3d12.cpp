/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/ore/ore_texture.hpp"
#include "rive/renderer/ore/ore_context.hpp"
#include "rive/rive_types.hpp"

#include <d3d12.h>
#include <wrl/client.h>
#include <cassert>
#include <cstring>

using Microsoft::WRL::ComPtr;

namespace rive::ore
{

// --- Public method definitions (D3D12-only builds) ---
// When both D3D11 and D3D12 are compiled, the combined
// ore_context_d3d11_d3d12.cpp file provides these methods with dispatch.
#if defined(ORE_BACKEND_D3D12) && !defined(ORE_BACKEND_D3D11)

void Texture::upload(const TextureDataDesc& data)
{
#if defined(ORE_BACKEND_D3D12)
    assert(m_d3dTexture != nullptr);
    assert(m_d3dDevice != nullptr);
    assert(m_d3dOreContext != nullptr);
    assert(!m_d3dIsExternal &&
           "Cannot upload into an external (canvas-wrapped) texture");
    assert(data.data != nullptr);

    Context* ctx = m_d3dOreContext;

    // Open the upload command list if not already recording.
    if (!ctx->m_d3dUploadListOpen)
    {
        ctx->m_d3dUploadAllocator->Reset();
        ctx->m_d3dUploadCmdList->Reset(ctx->m_d3dUploadAllocator.Get(),
                                       nullptr);
        ctx->m_d3dUploadListOpen = true;
    }

    // Compute the placed footprint for the requested subresource.
    // D3D12 subresource index formula:
    //   `mipSlice + arraySlice * MipLevels + planeSlice * MipLevels *
    //   ArraySize`
    // (see D3D12CalcSubresource in d3dx12.h). Pre-fix this was
    // `mipLevel * DepthOrArraySize + layer`, which is the *transposed*
    // formula — works only when MipLevels=1 (every Ore GM today) and
    // silently mis-targets the wrong subresource for any mipmapped
    // array texture.
    D3D12_RESOURCE_DESC texDesc = m_d3dTexture->GetDesc();
    UINT subresource =
        data.mipLevel + data.layer * static_cast<UINT>(texDesc.MipLevels);

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
    UINT numRows = 0;
    UINT64 rowBytes = 0;
    UINT64 totalBytes = 0;
    m_d3dDevice->GetCopyableFootprints(&texDesc,
                                       subresource,
                                       1,
                                       /*baseOffset=*/0,
                                       &footprint,
                                       &numRows,
                                       &rowBytes,
                                       &totalBytes);

    // Allocate a staging (UPLOAD heap) buffer.
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

    ComPtr<ID3D12Resource> staging;
    [[maybe_unused]] HRESULT hr = m_d3dDevice->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &bufDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(staging.GetAddressOf()));
    assert(SUCCEEDED(hr) && "Texture::upload: staging buffer creation failed");

    // Map and copy row by row, respecting the D3D12 pitch alignment.
    void* mapped = nullptr;
    D3D12_RANGE readRange = {0, 0};
    staging->Map(0, &readRange, &mapped);

    const uint8_t* src = static_cast<const uint8_t*>(data.data);
    uint8_t* dst = static_cast<uint8_t*>(mapped);
    UINT64 srcRow = data.bytesPerRow ? data.bytesPerRow : rowBytes;
    UINT64 dstRow = footprint.Footprint.RowPitch;

    for (UINT row = 0; row < numRows; ++row)
        memcpy(dst + row * dstRow, src + row * srcRow, (size_t)rowBytes);

    staging->Unmap(0, nullptr);

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
        ctx->m_d3dUploadCmdList->ResourceBarrier(1, &barrier);
        m_d3dCurrentState = D3D12_RESOURCE_STATE_COPY_DEST;
    }

    // Record the copy.
    D3D12_TEXTURE_COPY_LOCATION dst_loc = {};
    dst_loc.pResource = m_d3dTexture.Get();
    dst_loc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst_loc.SubresourceIndex = subresource;

    D3D12_TEXTURE_COPY_LOCATION src_loc = {};
    src_loc.pResource = staging.Get();
    src_loc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src_loc.PlacedFootprint = footprint;

    D3D12_BOX box = {};
    box.left = data.x;
    box.top = data.y;
    box.front = data.z;
    box.right = data.x + data.width;
    box.bottom = data.y + data.height;
    box.back = data.z + data.depth;

    ctx->m_d3dUploadCmdList
        ->CopyTextureRegion(&dst_loc, data.x, data.y, data.z, &src_loc, &box);

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
        ctx->m_d3dUploadCmdList->ResourceBarrier(1, &barrier);
        m_d3dCurrentState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    }

    // Keep the staging buffer alive until the GPU is done (freed in
    // endFrame/FlushUploads).
    ctx->m_d3dPendingUploads.push_back(std::move(staging));
#else
    (void)data;
#endif
}

void Texture::onRefCntReachedZero() const
{
    Context* ctx = m_d3dOreContext;
    auto destroy = [p = const_cast<Texture*>(this)]() { delete p; };
    if (ctx != nullptr)
        ctx->d3dDeferDestroy(std::move(destroy));
    else
        destroy();
}

void TextureView::onRefCntReachedZero() const
{
    Context* ctx = m_d3dOreContext;
    auto destroy = [p = const_cast<TextureView*>(this)]() { delete p; };
    if (ctx != nullptr)
        ctx->d3dDeferDestroy(std::move(destroy));
    else
        destroy();
}

#endif // D3D12-only

} // namespace rive::ore
