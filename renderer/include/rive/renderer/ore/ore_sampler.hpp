/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/renderer/gpu_resource.hpp"
#include "utils/lite_rtti.hpp"
#include "rive/renderer/ore/ore_types.hpp"

namespace rive::ore
{

class Context;

class Sampler : public rive::gpu::GPUResource, public ENABLE_LITE_RTTI(Sampler)
{
public:
  virtual ~Sampler() = default;

  // WebGPU binds filtering and comparison samplers only to bindings of their
  // own type, so creation records both for the checks every backend runs.
  void recordDesc(const SamplerDesc& desc)
  {
    m_filtering = desc.minFilter == Filter::linear ||
                  desc.magFilter == Filter::linear ||
                  desc.mipmapFilter == Filter::linear;
    m_comparison = desc.compare != CompareFunction::none;
    m_described = true;
  }
  bool described() const { return m_described; }
  bool filtering() const { return m_filtering; }
  bool comparison() const { return m_comparison; }

protected:
  friend class Context;
  friend class RenderPass;

  Sampler() : rive::gpu::GPUResource(nullptr) {}
  Sampler(rcp<rive::gpu::GPUResourceManager> manager)
      : rive::gpu::GPUResource(std::move(manager))
  {}

private:
  bool m_described = false;
  bool m_filtering = false;
  bool m_comparison = false;
};

inline rcp<Sampler> describedSampler(
    rcp<Sampler> sampler,
    const SamplerDesc& desc)
{
  if (sampler != nullptr)
    sampler->recordDesc(desc);
  return sampler;
}

} // namespace rive::ore
