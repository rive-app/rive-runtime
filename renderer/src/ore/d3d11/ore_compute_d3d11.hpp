/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/renderer/ore/ore_compute_pass.hpp"
#include "rive/renderer/ore/ore_compute_pipeline.hpp"
#include "ore_shader_module_d3d11.hpp"
#include <d3d11.h>
#include <vector>

namespace rive::ore
{

class ContextD3D11;

class ComputePipelineD3D11
    : public LITE_RTTI_OVERRIDE(ComputePipeline, ComputePipelineD3D11)
{
public:
  explicit ComputePipelineD3D11(const ComputePipelineDesc& desc)
      : lite_rtti_override(nullptr, desc)
  {}

private:
  friend class ContextD3D11;
  friend class ComputePassD3D11;

  // Valid while m_module lives.
  ID3D11ComputeShader* m_d3dCS = nullptr;
  rcp<ShaderModuleD3D11> m_module;
};

class ComputePassD3D11 : public ComputePass
{
public:
  explicit ComputePassD3D11(ContextD3D11* context);
  ~ComputePassD3D11() override;

protected:
  void applyPipeline(ComputePipeline* pipeline) override;
  void dispatch(uint32_t x, uint32_t y, uint32_t z) override;
  void end() override;

private:
  ContextD3D11* m_d3d11;
  // The view slots a dispatch bound, to clear once it has run.
  std::vector<uint32_t> m_srvSlots;
  std::vector<uint32_t> m_uavSlots;
};

} // namespace rive::ore
