/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/renderer/gpu_resource.hpp"
#include "utils/lite_rtti.hpp"
#include "rive/renderer/ore/ore_types.hpp"

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
    uint32_t sampleCount() const { return m_sampleCount; }
    bool isRenderTarget() const { return m_renderTarget; }

    virtual void upload(const TextureDataDesc& data) = 0;

    virtual ~Texture() = default;

protected:
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

} // namespace rive::ore
