#pragma once
#include "rive/renderer/ore/ore_sampler.hpp"
#include "rive/renderer/ore/ore_context_d3d12.hpp"
#include <d3d12.h>

namespace rive::ore
{
class ContextD3D12;

class SamplerD3D12 : public LITE_RTTI_OVERRIDE(Sampler, SamplerD3D12)
{
public:
  SamplerD3D12(rcp<rive::gpu::GPUResourceManager> manager)
      : lite_rtti_override(std::move(manager))
  {}
  ~SamplerD3D12() override = default;

private:
  friend class ContextD3D12;
  D3D12_CPU_DESCRIPTOR_HANDLE m_d3dSamplerHandle = {};
  // Given back when the sampler goes.
  D3D12DescriptorSlot m_d3dSamplerSlot;
};
} // namespace rive::ore
