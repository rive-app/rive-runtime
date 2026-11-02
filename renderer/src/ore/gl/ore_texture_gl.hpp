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
  void uploadImpl(const TextureDataDesc& data) override;
  // GLES3 has no texture views, so a sampled view's mip range is texture
  // state. Expects the texture bound at `unit`.
  void setSampledLevels(uint32_t unit, uint32_t base, uint32_t max);

private:
  friend class ContextGL;
  unsigned int m_glTexture = 0;
  unsigned int m_glRenderbuffer = 0;
  unsigned int m_glTarget = 0; // GLenum
  // False only for textures wrapped from outside, which aren't ours to
  // delete.
  bool m_glOwnsTexture = true;
  uint32_t m_glBaseLevel = 0;
  uint32_t m_glMaxLevel = 0;
};

class TextureViewGL : public LITE_RTTI_OVERRIDE(TextureView, TextureViewGL)
{
public:
  TextureViewGL(rcp<Texture> texture, const TextureViewDesc& desc)
      : lite_rtti_override(std::move(texture), desc)
  {}
};
} // namespace rive::ore
