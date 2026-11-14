/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/refcnt.hpp"
#include "rive/shapes/paint/layer_mask_mode.hpp"

#include <cstddef>

namespace rive::gpu
{
class RenderCanvas;
}

namespace rive::ore
{
class BindGroupLayout;
class Context;
class Pipeline;

// Masks a layer in one Ore pass, writing each content texel, scaled by a
// factor from the matching coverage texel, into a result canvas.
class LayerMaskCompositor
{
public:
  LayerMaskCompositor();
  ~LayerMaskCompositor();

  bool compose(
      Context& ore,
      gpu::RenderCanvas* content,
      gpu::RenderCanvas* coverage,
      gpu::RenderCanvas* result,
      LayerMaskMode mode);

  // A pipeline failed to build, so every later compose would fail too and a
  // host should stop offering masks.
  bool failed() const { return m_failed; }

private:
  Pipeline* pipelineFor(Context& ore, LayerMaskMode mode);

  static constexpr size_t kModeCount = 4;
  rcp<BindGroupLayout> m_layout;
  rcp<Pipeline> m_pipelines[kModeCount];
  bool m_failed = false;
};
} // namespace rive::ore
