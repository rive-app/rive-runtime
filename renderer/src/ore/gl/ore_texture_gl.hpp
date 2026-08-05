#pragma once
#include "rive/renderer/ore/ore_texture.hpp"
#include "rive/renderer/rive_render_image.hpp"

namespace rive::ore
{
class ContextGL;

class TextureGL : public LITE_RTTI_OVERRIDE(Texture, TextureGL)
{
public:
    TextureGL(const TextureDesc& desc) : lite_rtti_override(desc) {}
    ~TextureGL() override;
    void upload(const TextureDataDesc& data) override;

private:
    friend class ContextGL;
    unsigned int m_glTexture = 0;
    unsigned int m_glRenderbuffer = 0;
    unsigned int m_glTarget = 0; // GLenum
    bool m_glOwnsTexture = false;
};

class TextureViewGL : public LITE_RTTI_OVERRIDE(TextureView, TextureViewGL)
{
public:
    TextureViewGL(rcp<Texture> texture, const TextureViewDesc& desc) :
        lite_rtti_override(std::move(texture), desc)
    {}
    ~TextureViewGL() override;

    // The canvas import mirror owns the GL texture this view borrows, so the
    // view must keep it alive. Null for ordinary views.
    void retainCanvasMirror(rcp<RenderImage> mirror)
    {
        m_retainedCanvasMirror = std::move(mirror);
    }

private:
    friend class ContextGL;
    unsigned int m_glTextureView = 0; // GLenum; 0 means use base texture
    rcp<RenderImage> m_retainedCanvasMirror;
};
} // namespace rive::ore
