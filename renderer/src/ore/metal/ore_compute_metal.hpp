/*
 * Copyright 2026 Rive
 */

#pragma once
#include "ore_bind_group_metal.hpp"
#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_compute_pass.hpp"
#include "rive/renderer/ore/ore_compute_pipeline.hpp"
#import <Metal/Metal.h>

namespace rive::ore
{
class ContextMetal;

class ComputePipelineMetal
    : public LITE_RTTI_OVERRIDE(ComputePipeline, ComputePipelineMetal)
{
public:
  explicit ComputePipelineMetal(const ComputePipelineDesc& desc)
      : lite_rtti_override(nullptr, desc)
      , m_sized(sizedBindingsOf(desc.module))
  {}
  ~ComputePipelineMetal() override = default; // ARC releases m_mtlState

private:
  friend class ContextMetal;
  friend class ComputePassMetal;
  id<MTLComputePipelineState> m_mtlState = nil;
  SizedBindings m_sized;
};

class ComputePassMetal : public ComputePass
{
public:
  ComputePassMetal(ContextMetal* context, id<MTLComputeCommandEncoder> encoder);
  ~ComputePassMetal() override;

protected:
  void applyPipeline(ComputePipeline* pipeline) override;
  void dispatch(uint32_t x, uint32_t y, uint32_t z) override;
  void end() override;

private:
  // At the pipeline's compute slots.
  void applyBindGroup(uint32_t groupIndex);

  ContextMetal* m_metal;
  id<MTLComputeCommandEncoder> m_mtlEncoder = nil;
  std::vector<uint32_t> m_sizes;
};
} // namespace rive::ore
