/*
 * Copyright 2025 Rive
 */

#include "offscreen_render_target.hpp"

#ifdef RIVE_TOOLS_NO_GL

namespace rive_tests
{
rive::rcp<OffscreenRenderTarget> OffscreenRenderTarget::MakeGL(
    rive::gpu::RenderContextGLImpl*,
    uint32_t width,
    uint32_t height)
{
    return nullptr;
}
} // namespace rive_tests

#else

#include "rive/renderer/rive_render_image.hpp"
#include "rive/renderer/gl/render_context_gl_impl.hpp"
#include "rive/renderer/gl/render_target_gl.hpp"

namespace rive_tests
{
class OffscreenRenderTargetGL : public OffscreenRenderTarget
{
public:
    OffscreenRenderTargetGL(rive::gpu::RenderContextGLImpl* renderContextImpl,
                            uint32_t width,
                            uint32_t height) :
        m_textureTarget(
            rive::make_rcp<TextureTarget>(renderContextImpl, width, height))
    {}

    rive::RenderImage* asRenderImage() override
    {
        return m_textureTarget->renderImage();
    }

    rive::gpu::RenderTarget* asRenderTarget() override
    {
        return m_textureTarget.get();
    }

private:
    class TextureTarget : public rive::gpu::TextureRenderTargetGL
    {
    public:
        TextureTarget(rive::gpu::RenderContextGLImpl* renderContextImpl,
                      uint32_t width,
                      uint32_t height) :
            TextureRenderTargetGL(width, height)
        {
            GLuint tex;
            glGenTextures(1, &tex);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 256, 256);
            m_renderImage = rive::make_rcp<rive::RiveRenderImage>(
                renderContextImpl->adoptImageTexture(width, height, tex));
            setTargetTexture(tex);
        }

        rive::RiveRenderImage* renderImage() const
        {
            return m_renderImage.get();
        }

    private:
        rive::rcp<rive::RiveRenderImage> m_renderImage;
    };

    rive::rcp<TextureTarget> m_textureTarget;
};

rive::rcp<OffscreenRenderTarget> OffscreenRenderTarget::MakeGL(
    rive::gpu::RenderContextGLImpl* impl,
    uint32_t width,
    uint32_t height)
{
    return rive::make_rcp<OffscreenRenderTargetGL>(impl, width, height);
}
}; // namespace rive_tests

#endif
