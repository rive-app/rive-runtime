/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/refcnt.hpp"
#include "rive/renderer.hpp"
#include "rive/math/math_types.hpp"
#include "rive/pls/pls_render_context_impl.hpp"

namespace rive::pls
{
class PLSTexture : public RefCnt<PLSTexture>
{
public:
    PLSTexture(uint32_t width, uint32_t height);
    virtual ~PLSTexture() {}

    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }

    // Quazi-unique identifier of the underlying GPU texture resource managed by this class.
    uint32_t textureResourceHash() const { return m_textureResourceHash; }

private:
    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_textureResourceHash;
};

class PLSImage : public lite_rtti_override<RenderImage, PLSImage>
{
public:
    PLSImage(rcp<PLSTexture> texture) : PLSImage(texture->width(), texture->height())
    {
        resetTexture(std::move(texture));
    }

    rcp<PLSTexture> refTexture() const { return m_texture; }
    const PLSTexture* getTexture() const { return m_texture.get(); }

protected:
    PLSImage(int width, int height)
    {
        m_Width = width;
        m_Height = height;
    }

    void resetTexture(rcp<PLSTexture> texture = nullptr)
    {
        assert(texture == nullptr || texture->width() == m_Width);
        assert(texture == nullptr || texture->height() == m_Height);
        m_texture = std::move(texture);
    }

    // Used by the android runtime to send m_texture off to the worker thread to be deleted.
    PLSTexture* releaseTexture() { return m_texture.release(); }

private:
    rcp<PLSTexture> m_texture;
};
} // namespace rive::pls
