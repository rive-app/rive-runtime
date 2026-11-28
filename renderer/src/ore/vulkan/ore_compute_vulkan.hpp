/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_compute_pass.hpp"
#include "rive/renderer/ore/ore_compute_pipeline.hpp"
#include <vulkan/vulkan.h>

namespace rive::ore
{

class ContextVulkan;

class ComputePipelineVulkan
    : public LITE_RTTI_OVERRIDE(ComputePipeline, ComputePipelineVulkan)
{
public:
  ComputePipelineVulkan(
      rcp<rive::gpu::GPUResourceManager> manager,
      const ComputePipelineDesc& desc)
      : lite_rtti_override(std::move(manager), desc)
  {}
  ~ComputePipelineVulkan() override;

private:
  friend class ContextVulkan;
  friend class ComputePassVulkan;

  VkDevice m_vkDevice = VK_NULL_HANDLE; // Weak ref.
  VkPipeline m_vkPipeline = VK_NULL_HANDLE;
  VkPipelineLayout m_vkPipelineLayout = VK_NULL_HANDLE;
  PFN_vkDestroyPipeline m_vkDestroyPipeline = nullptr;
  PFN_vkDestroyPipelineLayout m_vkDestroyPipelineLayout = nullptr;
};

class ComputePassVulkan : public ComputePass
{
public:
  ComputePassVulkan(ContextVulkan* context, VkCommandBuffer commands);
  ~ComputePassVulkan() override;

protected:
  void applyPipeline(ComputePipeline* pipeline) override;
  void dispatch(uint32_t x, uint32_t y, uint32_t z) override;
  void end() override;

private:
  ContextVulkan* m_vkContext;
  VkCommandBuffer m_vkCmdBuf;
};

} // namespace rive::ore
