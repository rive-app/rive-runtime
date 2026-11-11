#pragma once
#include "rive/renderer/ore/ore_sampler.hpp"

namespace rive::ore
{
class ContextGL;
class RenderPassGL;

class SamplerGL : public LITE_RTTI_OVERRIDE(Sampler, SamplerGL)
{
public:
  SamplerGL() = default;
  ~SamplerGL() override;

private:
  friend class ContextGL;
  friend class RenderPassGL;
  unsigned int m_glSampler = 0; // GLuint
};
} // namespace rive::ore
