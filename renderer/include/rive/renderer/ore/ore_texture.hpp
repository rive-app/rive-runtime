/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/renderer/gpu_resource.hpp"
#include "utils/lite_rtti.hpp"
#include "rive/renderer/ore/ore_types.hpp"
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <string>

namespace rive::ore
{

class Context;

class Texture : public rive::gpu::GPUResource, public ENABLE_LITE_RTTI(Texture)
{
public:
    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }
    uint32_t depthOrArrayLayers() const { return m_depthOrArrayLayers; }
    TextureFormat format() const { return m_format; }
    TextureType type() const { return m_type; }
    uint32_t numMipmaps() const { return m_numMipmaps; }
    // Layers a view can span: the six cube faces, the array count, else one.
    uint32_t arrayLayers() const
    {
        switch (m_type)
        {
            case TextureType::cube:
                return 6;
            case TextureType::array2D:
                return m_depthOrArrayLayers;
            default:
                return 1;
        }
    }
    uint32_t sampleCount() const { return m_sampleCount; }
    bool isRenderTarget() const { return m_renderTarget; }

    // Fills the zero fields of `data`, rejects a region outside the mip level
    // or a buffer too small for it, and hands the result to the backend.
    // Returns false with the reason in `outError` and uploads nothing.
    bool upload(TextureDataDesc data, std::string* outError = nullptr);

    virtual ~Texture() = default;

protected:
    // Receives a desc whose fields are all filled and checked.
    virtual void uploadImpl(const TextureDataDesc& data) = 0;

    friend class Context;
    friend class TextureView;
    friend class RenderPass;

    Texture(const TextureDesc& desc) :
        rive::gpu::GPUResource(nullptr),
        m_width(desc.width),
        m_height(desc.height),
        m_depthOrArrayLayers(desc.depthOrArrayLayers),
        m_format(desc.format),
        m_type(desc.type),
        m_renderTarget(desc.renderTarget),
        m_numMipmaps(desc.numMipmaps),
        m_sampleCount(desc.sampleCount)
    {}

    Texture(rcp<rive::gpu::GPUResourceManager> manager,
            const TextureDesc& desc) :
        rive::gpu::GPUResource(std::move(manager)),
        m_width(desc.width),
        m_height(desc.height),
        m_depthOrArrayLayers(desc.depthOrArrayLayers),
        m_format(desc.format),
        m_type(desc.type),
        m_renderTarget(desc.renderTarget),
        m_numMipmaps(desc.numMipmaps),
        m_sampleCount(desc.sampleCount)
    {}

    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_depthOrArrayLayers;
    TextureFormat m_format;
    TextureType m_type;
    bool m_renderTarget;
    uint32_t m_numMipmaps;
    uint32_t m_sampleCount;
};

class TextureView : public rive::gpu::GPUResource,
                    public ENABLE_LITE_RTTI(TextureView)
{
public:
    Texture* texture() const { return m_texture.get(); }
    TextureViewDimension dimension() const { return m_dimension; }
    TextureAspect aspect() const { return m_aspect; }
    uint32_t baseMipLevel() const { return m_baseMipLevel; }
    uint32_t mipCount() const { return m_mipCount; }
    uint32_t baseLayer() const { return m_baseLayer; }
    uint32_t layerCount() const { return m_layerCount; }
    // Extent of the base mip level, which is what a pass over this view
    // renders into.
    uint32_t width() const
    {
        return std::max(1u, m_texture->width() >> m_baseMipLevel);
    }
    uint32_t height() const
    {
        return std::max(1u, m_texture->height() >> m_baseMipLevel);
    }

    virtual ~TextureView() = default;

protected:
    friend class Context;
    friend class RenderPass;

    TextureView(rcp<Texture> texture, const TextureViewDesc& desc) :
        rive::gpu::GPUResource(nullptr),
        m_texture(std::move(texture)),
        m_dimension(desc.dimension),
        m_aspect(desc.aspect),
        m_baseMipLevel(desc.baseMipLevel),
        m_mipCount(desc.mipCount),
        m_baseLayer(desc.baseLayer),
        m_layerCount(desc.layerCount)
    {}

    TextureView(rcp<rive::gpu::GPUResourceManager> manager,
                rcp<Texture> texture,
                const TextureViewDesc& desc) :
        rive::gpu::GPUResource(std::move(manager)),
        m_texture(std::move(texture)),
        m_dimension(desc.dimension),
        m_aspect(desc.aspect),
        m_baseMipLevel(desc.baseMipLevel),
        m_mipCount(desc.mipCount),
        m_baseLayer(desc.baseLayer),
        m_layerCount(desc.layerCount)
    {}

    rcp<Texture> m_texture;
    TextureViewDimension m_dimension;
    TextureAspect m_aspect;
    uint32_t m_baseMipLevel;
    uint32_t m_mipCount;
    uint32_t m_baseLayer;
    uint32_t m_layerCount;
};

inline bool uploadFail(std::string* outError, const char* fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 2, 3)))
#endif
    ;
inline bool uploadFail(std::string* outError, const char* fmt, ...)
{
    if (outError != nullptr)
    {
        va_list args;
        va_start(args, fmt);
        char buf[256];
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        *outError = buf;
    }
    return false;
}

inline bool Texture::upload(TextureDataDesc data, std::string* outError)
{
    if (data.data == nullptr)
    {
        return uploadFail(outError, "upload: data is null");
    }
    if (data.mipLevel >= m_numMipmaps)
    {
        return uploadFail(outError,
                          "upload: mipLevel %u exceeds %u levels",
                          data.mipLevel,
                          m_numMipmaps);
    }
    uint32_t layers = arrayLayers();
    if (data.layer >= layers)
    {
        return uploadFail(outError,
                          "upload: layer %u exceeds %u layers",
                          data.layer,
                          layers);
    }
    uint32_t mipW = std::max(1u, m_width >> data.mipLevel);
    uint32_t mipH = std::max(1u, m_height >> data.mipLevel);
    uint32_t mipD = m_type == TextureType::texture3D
                        ? std::max(1u, m_depthOrArrayLayers >> data.mipLevel)
                        : 1u;
    if (data.x >= mipW || data.y >= mipH || data.z >= mipD)
    {
        return uploadFail(
            outError,
            "upload: origin (%u, %u, %u) outside mip %u (%ux%ux%u)",
            data.x,
            data.y,
            data.z,
            data.mipLevel,
            mipW,
            mipH,
            mipD);
    }
    if (data.width == 0)
    {
        data.width = mipW - data.x;
    }
    if (data.height == 0)
    {
        data.height = mipH - data.y;
    }
    if (data.depth == 0)
    {
        data.depth = mipD - data.z;
    }
    if (data.width > mipW - data.x || data.height > mipH - data.y ||
        data.depth > mipD - data.z)
    {
        return uploadFail(
            outError,
            "upload: region %ux%ux%u at (%u, %u, %u) exceeds mip %u "
            "(%ux%ux%u)",
            data.width,
            data.height,
            data.depth,
            data.x,
            data.y,
            data.z,
            data.mipLevel,
            mipW,
            mipH,
            mipD);
    }
    uint32_t bpt = textureFormatBytesPerTexel(m_format);
    if (data.bytesPerRow == 0)
    {
        if (bpt == 0)
        {
            return uploadFail(
                outError,
                "upload: bytesPerRow is required for block compressed "
                "formats");
        }
        data.bytesPerRow = data.width * bpt;
    }
    else if (bpt != 0 && (data.bytesPerRow % bpt != 0 ||
                          data.bytesPerRow < uint64_t(data.width) * bpt))
    {
        return uploadFail(
            outError,
            "upload: bytesPerRow %u does not cover %u texels of %u "
            "bytes",
            data.bytesPerRow,
            data.width,
            bpt);
    }
    if (data.rowsPerImage == 0)
    {
        data.rowsPerImage = data.height;
    }
    else if (data.rowsPerImage < data.height)
    {
        return uploadFail(outError,
                          "upload: rowsPerImage %u is less than height %u",
                          data.rowsPerImage,
                          data.height);
    }
    uint64_t required =
        uint64_t(data.bytesPerRow) * data.rowsPerImage * data.depth;
    if (data.dataSize != 0 && data.dataSize < required)
    {
        return uploadFail(outError,
                          "upload: data is %u bytes but the region needs %llu",
                          data.dataSize,
                          static_cast<unsigned long long>(required));
    }
    uploadImpl(data);
    return true;
}

} // namespace rive::ore
