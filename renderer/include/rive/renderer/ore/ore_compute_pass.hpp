/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/refcnt.hpp"
#include "rive/renderer/ore/ore_compute_pipeline.hpp"
#include "rive/renderer/ore/ore_pass.hpp"
#include "rive/renderer/ore/ore_types.hpp"

#include <cstdint>

namespace rive::ore
{

class BindGroup;

class ComputePass : public Pass
{
public:
  inline void setPipeline(ComputePipeline* pipeline);

  // Bind a pre-created BindGroup, optionally overriding dynamic UBO offsets.
  virtual void setBindGroup(
      uint32_t groupIndex,
      BindGroup* bg,
      const uint32_t* dynamicOffsets = nullptr,
      uint32_t dynamicOffsetCount = 0) = 0;

  // Refused, with the context's last error set, without a pipeline or past
  // the device's workgroup count.
  inline void dispatchWorkgroups(uint32_t x, uint32_t y = 1, uint32_t z = 1);

  inline void finish() final;

protected:
  explicit ComputePass(Context* context) : Pass(context) {}

  virtual void applyPipeline(ComputePipeline* pipeline) = 0;
  virtual void dispatch(uint32_t x, uint32_t y, uint32_t z) = 0;
  virtual void end() = 0;

  rcp<ComputePipeline> m_pipeline;
};

} // namespace rive::ore
