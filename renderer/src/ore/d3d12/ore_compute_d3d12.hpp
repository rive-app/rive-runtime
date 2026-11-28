/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/renderer/ore/ore_compute_pass.hpp"
#include "rive/renderer/ore/ore_compute_pipeline.hpp"
#include "ore_pipeline_d3d12.hpp"

#include <d3d12.h>
#include <wrl/client.h>

namespace rive::ore
{
class ContextD3D12;

class ComputePipelineD3D12
    : public LITE_RTTI_OVERRIDE(ComputePipeline, ComputePipelineD3D12)
{
public:
  ComputePipelineD3D12(
      rcp<rive::gpu::GPUResourceManager> manager,
      const ComputePipelineDesc& desc)
      : lite_rtti_override(std::move(manager), desc)
  {}

private:
  friend class ContextD3D12;
  friend class ComputePassD3D12;

  Microsoft::WRL::ComPtr<ID3D12PipelineState> m_d3dPSO;
  Microsoft::WRL::ComPtr<ID3D12RootSignature> m_d3dRootSig;
  D3D12GroupRootParams m_d3dGroupParams[kMaxBindGroups];
  // The root constants a dispatch writes its workgroup counts to.
  int8_t m_d3dWorkgroupCountParam = -1;
};

class ComputePassD3D12 : public ComputePass
{
public:
  explicit ComputePassD3D12(ContextD3D12* context);
  ~ComputePassD3D12() override;

protected:
  void applyPipeline(ComputePipeline* pipeline) override;
  void dispatch(uint32_t x, uint32_t y, uint32_t z) override;
  void end() override;

private:
  // False, with the context's last error set, when a group has no table.
  bool setGroupTables(const ComputePipelineD3D12& pipeline);

  ContextD3D12* m_d3d12;
};

} // namespace rive::ore
