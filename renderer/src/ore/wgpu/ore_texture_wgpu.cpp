/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/ore/ore_texture.hpp"
#include "rive/renderer/ore/ore_types.hpp"
#include "rive/rive_types.hpp"

namespace rive::ore
{

void Texture::upload(const TextureDataDesc& data)
{
    assert(m_wgpuTexture != nullptr);
    assert(data.data != nullptr);

    wgpu::TexelCopyTextureInfo dst{};
    dst.texture = m_wgpuTexture;
    dst.mipLevel = data.mipLevel;
    dst.origin = {data.x, data.y, data.layer};
    dst.aspect = wgpu::TextureAspect::All;

    // Compute the actual row stride. If not provided, assume tightly packed.
    // Block-compressed formats have no simple bytes-per-texel value, so callers
    // must always supply bytesPerRow for them.
    constexpr uint32_t kDawnBytesPerRowAlignment = 256;
    uint32_t bpt = textureFormatBytesPerTexel(m_format);
    assert((bpt != 0 || data.bytesPerRow != 0) &&
           "bytesPerRow must be provided for block-compressed formats");
    uint32_t actualBytesPerRow = data.bytesPerRow;
    if (actualBytesPerRow == 0)
        actualBytesPerRow = data.width * bpt;

    uint32_t rowsPerImage =
        data.rowsPerImage > 0 ? data.rowsPerImage : data.height;

    // Dawn requires bytesPerRow to be a multiple of 256 when height > 1.
    // If the data is tightly packed and doesn't meet alignment, upload row by
    // row for uncompressed 2D textures (bpt != 0, depth == 1): height == 1 per
    // call allows WGPU_COPY_STRIDE_UNDEFINED, avoiding the alignment
    // requirement. Block-compressed formats (bpt == 0) can't use this path
    // since we can't compute a packed row size — callers must supply
    // 256-aligned strides. For depth > 1, bytesPerRow and rowsPerImage must be
    // valid — callers are expected to supply aligned strides for 3D/array
    // uploads.
    bool needsRowByRow = (bpt != 0) && (data.height > 1) && (data.depth == 1) &&
                         (actualBytesPerRow % kDawnBytesPerRowAlignment != 0);
    if (needsRowByRow)
    {
        // Use the packed pixel size (no padding) as the data size passed to
        // WriteTexture — WGPU_COPY_STRIDE_UNDEFINED means Dawn expects packed
        // data for a single row. Advance rowPtr by the full source stride so
        // any caller-provided row padding is correctly skipped.
        uint32_t packedRowBytes = data.width * bpt;
        wgpu::TexelCopyBufferLayout rowLayout{};
        rowLayout.bytesPerRow = WGPU_COPY_STRIDE_UNDEFINED;
        rowLayout.rowsPerImage = WGPU_COPY_STRIDE_UNDEFINED;
        wgpu::Extent3D rowExtent{data.width, 1, 1};
        const uint8_t* rowPtr = static_cast<const uint8_t*>(data.data);
        for (uint32_t y = 0; y < data.height; ++y)
        {
            dst.origin.y = data.y + y;
            m_wgpuQueue.WriteTexture(&dst,
                                     rowPtr,
                                     packedRowBytes,
                                     &rowLayout,
                                     &rowExtent);
            rowPtr += actualBytesPerRow;
        }
        return;
    }

    wgpu::TexelCopyBufferLayout layout{};
    layout.bytesPerRow = actualBytesPerRow;
    layout.rowsPerImage = rowsPerImage;

    wgpu::Extent3D extent{data.width, data.height, data.depth};

    uint32_t dataSize = actualBytesPerRow * rowsPerImage * data.depth;
    m_wgpuQueue.WriteTexture(&dst, data.data, dataSize, &layout, &extent);
}

void Texture::onRefCntReachedZero() const
{
    // wgpu::Texture RAII destructor releases the GPU resource.
    delete this;
}

void TextureView::onRefCntReachedZero() const
{
    // wgpu::TextureView RAII destructor releases the GPU resource.
    delete this;
}

} // namespace rive::ore
