/*
 * Copyright 2026 Rive
 */

#include "ore_compute_vulkan.hpp"
#include "ore_bind_group_vulkan.hpp"
#include "rive/renderer/ore/ore_context_vulkan.hpp"

namespace rive::ore
{

ComputePipelineVulkan::~ComputePipelineVulkan()
{
  if (m_vkPipeline != VK_NULL_HANDLE && m_vkDestroyPipeline != nullptr)
    m_vkDestroyPipeline(m_vkDevice, m_vkPipeline, nullptr);
  if (m_vkPipelineLayout != VK_NULL_HANDLE &&
      m_vkDestroyPipelineLayout != nullptr)
    m_vkDestroyPipelineLayout(m_vkDevice, m_vkPipelineLayout, nullptr);
}

ComputePassVulkan::ComputePassVulkan(
    ContextVulkan* context,
    VkCommandBuffer commands)
    : ComputePass(context)
    , m_vkContext(context)
    , m_vkCmdBuf(commands)
{}

ComputePassVulkan::~ComputePassVulkan() { finish(); }

void ComputePassVulkan::applyPipeline(ComputePipeline* pipeline)
{
  auto* vulkan = static_cast<ComputePipelineVulkan*>(pipeline);
  m_vkContext->m_vk->CmdBindPipeline(
      m_vkCmdBuf,
      VK_PIPELINE_BIND_POINT_COMPUTE,
      vulkan->m_vkPipeline);
}

void ComputePassVulkan::dispatch(uint32_t x, uint32_t y, uint32_t z)
{
  auto* pipeline = static_cast<ComputePipelineVulkan*>(m_pipeline.get());
  for (uint32_t g = 0; g < kMaxBindGroups; ++g)
  {
    // A group left at an index this pipeline binds nothing at stays out.
    auto* group = static_cast<BindGroupVulkan*>(m_groups[g].get());
    if (group == nullptr || !hasEntries(pipeline->m_layouts[g]))
      continue;
    VkDescriptorSet set = group->resolveDescriptorSet();
    if (set == VK_NULL_HANDLE)
    {
      m_vkContext->setLastError(
          "ore: Vulkan descriptor set allocation failed for group %u",
          g);
      return;
    }
    group->markUBOsBound();
    m_vkContext->m_vk->CmdBindDescriptorSets(
        m_vkCmdBuf,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        pipeline->m_vkPipelineLayout,
        g,
        1,
        &set,
        m_groupOffsetCounts[g],
        m_groupOffsets[g]);
  }
  m_vkContext->m_vk->CmdDispatch(m_vkCmdBuf, x, y, z);
  // The next dispatch, draw or copy may touch what this one read and wrote.
  m_vkContext->vkBarrierAfter(
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      VK_ACCESS_SHADER_WRITE_BIT);
}

void ComputePassVulkan::end() { m_vkContext->m_vkPassOpen = false; }

} // namespace rive::ore
