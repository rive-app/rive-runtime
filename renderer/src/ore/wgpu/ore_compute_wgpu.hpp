/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/renderer/ore/ore_compute_pass.hpp"
#include "rive/renderer/ore/ore_compute_pipeline.hpp"
#include <webgpu/webgpu_cpp.h>

namespace rive::ore
{

class ContextWGPU;

class ComputePipelineWGPU
    : public LITE_RTTI_OVERRIDE(ComputePipeline, ComputePipelineWGPU)
{
public:
  explicit ComputePipelineWGPU(const ComputePipelineDesc& desc)
      : lite_rtti_override(nullptr, desc)
  {}

private:
  friend class ContextWGPU;
  friend class ComputePassWGPU;

  wgpu::PipelineLayout m_wgpuPipelineLayout;
  wgpu::ComputePipeline m_wgpuPipeline;
};

class ComputePassWGPU : public ComputePass
{
public:
  ComputePassWGPU(ContextWGPU* context, wgpu::ComputePassEncoder encoder);
  ~ComputePassWGPU() override;

protected:
  void applyPipeline(ComputePipeline* pipeline) override;
  void dispatch(uint32_t x, uint32_t y, uint32_t z) override;
  void end() override;

private:
  ContextWGPU* m_wgpuContext;
  wgpu::ComputePassEncoder m_wgpuPassEncoder;
};

} // namespace rive::ore
