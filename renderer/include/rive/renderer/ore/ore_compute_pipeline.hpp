/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/renderer/gpu_resource.hpp"
#include "utils/lite_rtti.hpp"
#include "rive/renderer/ore/ore_bind_group_layout.hpp"
#include "rive/renderer/ore/ore_shader_module.hpp"
#include "rive/renderer/ore/ore_types.hpp"

#include <algorithm>
#include <string>

namespace rive::ore
{

class Context;

class ComputePipeline : public rive::gpu::GPUResource,
                        public ENABLE_LITE_RTTI(ComputePipeline)
{
public:
  // What the entry point binds, at its compute slots.
  BindingMap m_bindingMap;

  // One per `@group(N)`, kept alive for the bind groups checked against them.
  rcp<BindGroupLayout> m_layouts[kMaxBindGroups];

  const std::string& entryPoint() const { return m_entryPoint; }

  // Threads per workgroup, and the workgroup variables Metal sizes memory
  // for.
  const RstbWorkgroup& workgroup() const { return m_workgroup; }

  virtual ~ComputePipeline() = default;

protected:
  // `desc` has passed validateComputePipelineDesc, so its entry point has a
  // workgroup.
  ComputePipeline(
      rcp<rive::gpu::GPUResourceManager> manager,
      const ComputePipelineDesc& desc)
      : rive::gpu::GPUResource(std::move(manager))
      , m_bindingMap(bindingMapForComputeEntry(desc.module, desc.entryPoint))
      , m_entryPoint(desc.entryPoint)
      , m_workgroup(*desc.module->workgroup(desc.entryPoint))
  {
    const uint32_t count = std::min(
        desc.bindGroupLayoutCount,
        static_cast<uint32_t>(kMaxBindGroups));
    for (uint32_t i = 0; i < count; ++i)
      m_layouts[i] = ref_rcp(desc.bindGroupLayouts[i]);
  }

private:
  std::string m_entryPoint;
  RstbWorkgroup m_workgroup;
};

} // namespace rive::ore
