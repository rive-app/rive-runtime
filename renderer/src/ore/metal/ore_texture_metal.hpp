#pragma once
#include "rive/renderer/ore/ore_texture.hpp"
#import <Metal/Metal.h>

namespace rive::ore
{
class ContextMetal;

class TextureMetal : public LITE_RTTI_OVERRIDE(Texture, TextureMetal)
{
public:
    TextureMetal(const TextureDesc& desc) : lite_rtti_override(desc) {}
    ~TextureMetal() override = default; // ARC releases m_mtlTexture
    void upload(const TextureDataDesc& data) override;
    id<MTLTexture> mtlTexture() const { return m_mtlTexture; }

private:
    friend class ContextMetal;
    id<MTLTexture> m_mtlTexture = nil;
};

class TextureViewMetal
    : public LITE_RTTI_OVERRIDE(TextureView, TextureViewMetal)
{
public:
    TextureViewMetal(rcp<Texture> texture, const TextureViewDesc& desc) :
        lite_rtti_override(std::move(texture), desc)
    {}
    ~TextureViewMetal() override = default;
    id<MTLTexture> mtlTexture() const
    {
        return m_mtlTextureView
                   ? m_mtlTextureView
                   : static_cast<TextureMetal*>(m_texture.get())->mtlTexture();
    }

private:
    friend class ContextMetal;
    id<MTLTexture> m_mtlTextureView = nil;
};
} // namespace rive::ore
