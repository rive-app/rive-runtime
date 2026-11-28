/*
 * Copyright 2026 Rive
 */

#include "ore_compute_d3d11.hpp"
#include "ore_bind_group_d3d11.hpp"
#include "rive/renderer/ore/ore_context_d3d11.hpp"

namespace rive::ore
{

#if defined(ORE_BACKEND_D3D11)

ComputePassD3D11::ComputePassD3D11(ContextD3D11* context)
    : ComputePass(context)
    , m_d3d11(context)
{}

ComputePassD3D11::~ComputePassD3D11() { finish(); }

void ComputePassD3D11::applyPipeline(ComputePipeline*) {}

void ComputePassD3D11::dispatch(uint32_t x, uint32_t y, uint32_t z)
{
  ID3D11DeviceContext* context = m_d3d11->m_d3d11Context.Get();
  ID3D11DeviceContext1* context1 = m_d3d11->m_d3d11Context1.Get();
  auto* pipeline = static_cast<ComputePipelineD3D11*>(m_pipeline.get());
  const BindingMap& map = pipeline->m_bindingMap;
  const BindingMap::Stage stage = BindingMap::Stage::CS;
  context->CSSetShader(pipeline->m_d3dCS, nullptr, 0);

  for (uint32_t g = 0; g < kMaxBindGroups; ++g)
  {
    // A group left at an index this pipeline binds nothing at stays out.
    auto* group = static_cast<BindGroupD3D11*>(m_groups[g].get());
    if (group == nullptr || !hasEntries(pipeline->m_layouts[g]))
      continue;
    uint32_t dynamicIndex = 0;
    for (const auto& ubo : group->m_d3d11UBOs)
    {
      uint32_t offset = ubo.offset;
      if (ubo.hasDynamicOffset && dynamicIndex < m_groupOffsetCounts[g])
        offset += m_groupOffsets[g][dynamicIndex++];
      const uint16_t slot = map.lookup(
          g,
          ubo.binding,
          ResourceKind::UniformBuffer,
          stage);
      if (slot == BindingMap::kAbsent)
        continue;
      ID3D11Buffer* buffer = ubo.buffer;
      if (offset != 0 && context1 != nullptr)
      {
        const D3D11ConstantRange range = d3d11ConstantRange(offset, ubo.size);
        context1->CSSetConstantBuffers1(
            slot,
            1,
            &buffer,
            &range.first,
            &range.count);
      }
      else
      {
        context->CSSetConstantBuffers(slot, 1, &buffer);
      }
    }
    for (const auto& texture : group->m_d3d11Textures)
    {
      const uint16_t slot = map.lookup(
          g,
          texture.binding,
          ResourceKind::SampledTexture,
          stage);
      if (slot == BindingMap::kAbsent)
        continue;
      context->CSSetShaderResources(slot, 1, &texture.srv);
      m_srvSlots.push_back(slot);
    }
    for (const auto& sampler : group->m_d3d11Samplers)
    {
      const uint16_t slot = map.lookup(
          g,
          sampler.binding,
          ResourceKind::Sampler,
          stage);
      if (slot != BindingMap::kAbsent)
        context->CSSetSamplers(slot, 1, &sampler.sampler);
    }
    for (const auto& storage : group->m_d3d11Storage)
    {
      const bool writable = storage.uav != nullptr;
      const uint16_t slot = map.lookup(
          g,
          storage.binding,
          writable ? ResourceKind::StorageBufferRW
                   : ResourceKind::StorageBufferRO,
          stage);
      if (slot == BindingMap::kAbsent)
        continue;
      if (writable)
      {
        ID3D11UnorderedAccessView* uav = storage.uav.Get();
        context->CSSetUnorderedAccessViews(slot, 1, &uav, nullptr);
        m_uavSlots.push_back(slot);
      }
      else
      {
        ID3D11ShaderResourceView* srv = storage.srv.Get();
        context->CSSetShaderResources(slot, 1, &srv);
        m_srvSlots.push_back(slot);
      }
    }
  }

  m_d3d11->d3d11BindWorkgroupCounts(x, y, z);
  context->Dispatch(x, y, z);

  // Left bound, what this dispatch wrote could not be read by a later draw,
  // nor what it read written.
  ID3D11ShaderResourceView* noSRV = nullptr;
  ID3D11UnorderedAccessView* noUAV = nullptr;
  for (uint32_t slot : m_uavSlots)
    context->CSSetUnorderedAccessViews(slot, 1, &noUAV, nullptr);
  for (uint32_t slot : m_srvSlots)
    context->CSSetShaderResources(slot, 1, &noSRV);
  m_uavSlots.clear();
  m_srvSlots.clear();
}

void ComputePassD3D11::end()
{
  m_d3d11->m_d3d11PassOpen = false;
  m_d3d11->m_d3d11ComputeOpen = false;
}

#endif // ORE_BACKEND_D3D11

} // namespace rive::ore
