#pragma once
#include "rive/renderer/ore/ore_bind_group.hpp"
#include <cstdint>
#include <vector>

namespace rive::ore
{
class ContextGL;
class RenderPassGL;
class SamplerGL;
class TextureGL;

class BindGroupGL : public LITE_RTTI_OVERRIDE(BindGroup, BindGroupGL)
{
public:
  struct GLUBOBinding
  {
    unsigned int buffer = 0; // GLuint
    uint32_t offset = 0;
    uint32_t size = 0;
    uint32_t binding = 0; // WGSL @binding, for sort
    bool hasDynamicOffset = false;
  };
  struct GLTexBinding
  {
    unsigned int texture = 0; // GLuint
    unsigned int target = 0;  // GLenum (GL_TEXTURE_2D etc.)
    uint32_t binding = 0;     // WGSL @binding, for pair lookup
    // Null for borrowed textures, whose mip range belongs to their owner.
    TextureGL* owned = nullptr;
    uint32_t baseLevel = 0;
    uint32_t maxLevel = 0;
  };
  struct GLSamplerBinding
  {
    SamplerGL* sampler = nullptr; // held by m_retainedSamplers
    uint32_t binding = 0;         // WGSL @binding, for pair lookup
  };

  BindGroupGL() = default;
  ~BindGroupGL() override = default;

private:
  friend class ContextGL;
  friend class RenderPassGL;
  std::vector<GLUBOBinding> m_glUBOs;
  std::vector<GLTexBinding> m_glTextures;
  std::vector<GLSamplerBinding> m_glSamplers;
};
} // namespace rive::ore
