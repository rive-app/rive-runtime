/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/renderer/texture.hpp"

namespace rive
{
class RiveRenderImage : public LITE_RTTI_OVERRIDE(RenderImage, RiveRenderImage)
{
public:
    RiveRenderImage(rcp<gpu::Texture> texture) :
        RiveRenderImage(texture->width(), texture->height())
    {
        resetTexture(std::move(texture));
    }

    rcp<gpu::Texture> refTexture() const { return m_texture; }
    const gpu::Texture* getTexture() const { return m_texture.get(); }

protected:
    RiveRenderImage(int width, int height)
    {
        m_Width = width;
        m_Height = height;
    }

    void resetTexture(rcp<gpu::Texture> texture = nullptr)
    {
        assert(texture == nullptr || texture->width() == m_Width);
        assert(texture == nullptr || texture->height() == m_Height);
        m_texture = std::move(texture);
    }

    // Used by the android runtime to send m_texture off to the worker thread to
    // be deleted.
    gpu::Texture* releaseTexture() { return m_texture.release(); }

private:
    rcp<gpu::Texture> m_texture;
};
} // namespace rive
