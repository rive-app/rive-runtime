/*
 * Copyright 2026 Rive
 */

#include "ore_compute_metal.hpp"
#include "ore_bind_group_metal.hpp"
#include "ore_buffer_metal.hpp"
#include "rive/renderer/ore/ore_context_metal.hpp"

namespace rive::ore
{

ComputePassMetal::ComputePassMetal(
    ContextMetal* context, id<MTLComputeCommandEncoder> encoder)
    : ComputePass(context)
    , m_metal(context)
    , m_mtlEncoder(encoder)
{}

ComputePassMetal::~ComputePassMetal() { finish(); }

void ComputePassMetal::applyPipeline(ComputePipeline* pipeline)
{
  auto* metal = static_cast<ComputePipelineMetal*>(pipeline);
  [m_mtlEncoder setComputePipelineState:metal->m_mtlState];
  // The kernel takes each workgroup variable as memory Metal sizes by index.
  const std::vector<uint32_t>& memory = pipeline->workgroup().memory;
  for (size_t i = 0; i < memory.size(); ++i)
  {
    [m_mtlEncoder setThreadgroupMemoryLength:(memory[i] + 15u) & ~15u
                                     atIndex:i];
  }
}

void ComputePassMetal::applyBindGroup(uint32_t groupIndex)
{
  auto* group = static_cast<BindGroupMetal*>(m_groups[groupIndex].get());
  const BindingMap& map = m_pipeline->m_bindingMap;
  const BindingMap::Stage stage = BindingMap::Stage::CS;
  uint32_t dynamicIndex = 0;
  for (auto& b : group->m_mtlBuffers)
  {
    uint32_t offset = b.offset;
    if (b.hasDynamicOffset && dynamicIndex < m_groupOffsetCounts[groupIndex])
      offset += m_groupOffsets[groupIndex][dynamicIndex++];
    const uint16_t slot = map.lookup(
        groupIndex, b.binding, ResourceKind::UniformBuffer, stage);
    if (slot == BindingMap::kAbsent)
      continue;
    b.src->markBound();
    [m_mtlEncoder setBuffer:b.src->current() offset:offset atIndex:slot];
  }
  for (auto& b : group->m_mtlStorageBuffers)
  {
    uint16_t slot = map.lookup(
        groupIndex, b.binding, ResourceKind::StorageBufferRW, stage);
    if (slot == BindingMap::kAbsent)
    {
      slot = map.lookup(
          groupIndex, b.binding, ResourceKind::StorageBufferRO, stage);
    }
    if (slot != BindingMap::kAbsent)
      [m_mtlEncoder setBuffer:b.src->current() offset:b.offset atIndex:slot];
  }
  for (auto& t : group->m_mtlTextures)
  {
    const uint16_t slot = map.lookup(
        groupIndex, t.binding, ResourceKind::SampledTexture, stage);
    if (slot != BindingMap::kAbsent)
      [m_mtlEncoder setTexture:t.texture atIndex:slot];
  }
  for (auto& s : group->m_mtlSamplers)
  {
    const uint16_t slot = map.lookup(
        groupIndex, s.binding, ResourceKind::Sampler, stage);
    if (slot != BindingMap::kAbsent)
      [m_mtlEncoder setSamplerState:s.sampler atIndex:slot];
  }
}

void ComputePassMetal::dispatch(uint32_t x, uint32_t y, uint32_t z)
{
  for (uint32_t g = 0; g < kMaxBindGroups; ++g)
  {
    if (m_groups[g] != nullptr)
      applyBindGroup(g);
  }
  auto* metal = static_cast<ComputePipelineMetal*>(m_pipeline.get());
  if (BindGroupMetal::boundSizes(metal->m_sized, m_groups, m_sizes))
  {
    [m_mtlEncoder setBytes:m_sizes.data()
                    length:m_sizes.size() * sizeof(uint32_t)
                   atIndex:kMetalReservedBufferSlot];
  }
  const uint32_t* size = m_pipeline->workgroup().size;
  [m_mtlEncoder dispatchThreadgroups:MTLSizeMake(x, y, z)
               threadsPerThreadgroup:MTLSizeMake(size[0], size[1], size[2])];
}

void ComputePassMetal::end()
{
  [m_mtlEncoder endEncoding];
  m_mtlEncoder = nil;
  m_metal->encoderEnded();
}

} // namespace rive::ore
