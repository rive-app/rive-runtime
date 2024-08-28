/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/refcnt.hpp"
#include "rive/renderer.hpp"
#include "rive/renderer/render_context_impl.hpp"

namespace rive::gpu
{
class Texture : public RefCnt<Texture>
{
public:
    Texture(uint32_t width, uint32_t height);
    virtual ~Texture() {}

    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }

    // Quazi-unique identifier of the underlying GPU texture resource managed by this class.
    uint32_t textureResourceHash() const { return m_textureResourceHash; }

    // 64-bit handle that allows shaders to sample this texture without a texture binding.
    // Only supported if PlatformFeatures::supportsBindlessTextures is set, otherwise 0.
    uint64_t bindlessTextureHandle() const { return m_bindlessTextureHandle; }

protected:
    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_textureResourceHash;
    uint64_t m_bindlessTextureHandle = 0;
};

class Image : public lite_rtti_override<RenderImage, Image>
{
public:
    Image(rcp<Texture> texture) : Image(texture->width(), texture->height())
    {
        resetTexture(std::move(texture));
    }

    rcp<Texture> refTexture() const { return m_texture; }
    const Texture* getTexture() const { return m_texture.get(); }

protected:
    Image(int width, int height)
    {
        m_Width = width;
        m_Height = height;
    }

    void resetTexture(rcp<Texture> texture = nullptr)
    {
        assert(texture == nullptr || texture->width() == m_Width);
        assert(texture == nullptr || texture->height() == m_Height);
        m_texture = std::move(texture);
    }

    // Used by the android runtime to send m_texture off to the worker thread to be deleted.
    Texture* releaseTexture() { return m_texture.release(); }

private:
    rcp<Texture> m_texture;
};
} // namespace rive::gpu
