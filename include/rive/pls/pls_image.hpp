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
    virtual ~PLSTexture() {}
};

class PLSImage : public RenderImage
{
public:
    PLSImage(int width, int height, std::unique_ptr<const uint8_t[]> imageDataRGBA) :
        m_tempImageDataRGBA(std::move(imageDataRGBA))
    {
        m_Width = width;
        m_Height = height;
    }

    rcp<PLSTexture> refTexture(PLSRenderContextImpl* plsContextImpl) const
    {
        if (m_texture == nullptr)
        {
            uint32_t mipLevelCount = math::msb(m_Width | m_Height);
            m_texture = plsContextImpl->makeImageTexture(m_Width,
                                                         m_Height,
                                                         mipLevelCount,
                                                         m_tempImageDataRGBA.get());
            m_tempImageDataRGBA = nullptr;
        }
        return m_texture;
    }

protected:
    // Used by the android runtime to send m_texture off to the worker thread to be deleted.
    PLSTexture* releaseTexture() { return m_texture.release(); }

private:
    // Temporarily stores the image data until until the first call to refTexture().
    mutable std::unique_ptr<const uint8_t[]> m_tempImageDataRGBA;
    mutable rcp<PLSTexture> m_texture;
};
} // namespace rive::pls
