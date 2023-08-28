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
    PLSTexture(uint32_t width, uint32_t height) : m_width(width), m_height(height) {}
    virtual ~PLSTexture() {}

    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }

private:
    uint32_t m_width;
    uint32_t m_height;
};

class PLSImage : public RenderImage
{
public:
    PLSImage(rcp<PLSTexture> texture) : PLSImage(texture->width(), texture->height())
    {
        resetTexture(std::move(texture));
    }

    rcp<PLSTexture> refTexture(PLSRenderContextImpl* plsContextImpl) const { return m_texture; }

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
