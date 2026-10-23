/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/renderer/gpu_resource.hpp"
#include "utils/lite_rtti.hpp"
#include "rive/renderer/ore/ore_types.hpp"
#include "rive/renderer/ore/ore_bind_group_layout.hpp"
#include "rive/renderer/ore/ore_shader_module.hpp"

#include <vector>

namespace rive::ore
{

class Context;

class Pipeline : public rive::gpu::GPUResource,
                 public ENABLE_LITE_RTTI(Pipeline)
{
public:
  const PipelineDesc& desc() const { return m_desc; }

  // What the entry points bind, each stage from its own module; backends
  // validate the layouts against it.
  BindingMap m_bindingMap;

  // Older bakes leave it false, and their layouts then cover whole files.
  bool m_entryUseKnown = false;

  // Which sampler serves which texture. GL needs it: GLSL folds the two
  // into one uniform at the texture's unit. Empty on other backends.
  std::vector<ShaderModule::TextureSamplerPair> m_textureSamplerPairs;

  struct StageSlots
  {
    uint16_t vs = BindingMap::kAbsent;
    uint16_t fs = BindingMap::kAbsent;
    SlotSpace space = SlotSpace::none;
    bool operator==(const StageSlots& o) const
    {
      return vs == o.vs && fs == o.fs && space == o.space;
    }
  };

  // Where this pipeline's shaders read (group, binding), absent unless it is
  // the same kind of resource here.
  StageSlots slots(uint32_t group, uint32_t binding, SlotSpace space) const
  {
    const std::vector<StageSlots>& table = m_groupSlots[group];
    return binding < table.size() && table[binding].space == space
               ? table[binding]
               : StageSlots{};
  }

  // Layouts the pipeline was created with — one per `@group(N)`. Keeps
  // them alive (the per-backend native handles inside the layouts are
  // referenced by the pipeline's compiled state). Script lanes check each
  // draw's bind groups against them.
  rcp<BindGroupLayout> m_layouts[kMaxBindGroups];

  virtual ~Pipeline() = default;

protected:
  friend class Context;
  friend class RenderPass;

  Pipeline(const PipelineDesc& desc) : Pipeline(nullptr, desc) {}

  Pipeline(rcp<rive::gpu::GPUResourceManager> manager, const PipelineDesc& desc)
      : rive::gpu::GPUResource(std::move(manager))
      , m_desc(desc)
  {
    m_bindingMap = bindingMapForEntries(
        desc.vertexModule,
        desc.vertexEntryPoint,
        desc.fragmentModule,
        desc.fragmentEntryPoint);
    m_entryUseKnown = entryUseKnown(
        desc.vertexModule,
        desc.vertexEntryPoint,
        desc.fragmentModule,
        desc.fragmentEntryPoint);
    m_textureSamplerPairs = pipelineSamplerPairs(desc);
    for (size_t i = 0; i < m_bindingMap.size(); ++i)
    {
      const BindingMap::Entry& e = m_bindingMap.at(i);
      if (e.group >= kMaxBindGroups)
        continue;
      std::vector<StageSlots>& table = m_groupSlots[e.group];
      if (table.size() <= e.binding)
        table.resize(e.binding + 1u);
      table[e.binding] = {
          e.backendSlot[static_cast<size_t>(BindingMap::Stage::VS)],
          e.backendSlot[static_cast<size_t>(BindingMap::Stage::FS)],
          slotSpace(e.kind)};
    }

    // Stash the user-supplied layouts. Backends overwrite NULL
    // entries (groups the shader doesn't bind) with a no-op
    // BindGroupLayout if needed for empty-set semantics.
    const uint32_t count = std::min(
        desc.bindGroupLayoutCount,
        static_cast<uint32_t>(kMaxBindGroups));
    for (uint32_t i = 0; i < count; ++i)
    {
      m_layouts[i] = ref_rcp(desc.bindGroupLayouts[i]);
    }
    ownVertexLayout();
  }

  PipelineDesc m_desc;

private:
  // Indexed by @binding.
  std::vector<StageSlots> m_groupSlots[kMaxBindGroups];

  // The desc's vertex layout points into caller memory the deferred replay
  // frees right after makePipeline, so deep copy it into owned storage.
  std::vector<VertexBufferLayout> m_ownedVertexBuffers;
  std::vector<VertexAttribute> m_ownedAttributes;

  void ownVertexLayout()
  {
    if (m_desc.vertexBufferCount == 0 || m_desc.vertexBuffers == nullptr)
    {
      m_desc.vertexBuffers = nullptr;
      m_desc.vertexBufferCount = 0;
      return;
    }
    // Reserve up front so the vector never reallocates while we repoint
    // into it.
    size_t total = 0;
    for (uint32_t i = 0; i < m_desc.vertexBufferCount; ++i)
    {
      total += m_desc.vertexBuffers[i].attributeCount;
    }
    m_ownedAttributes.reserve(total);
    m_ownedVertexBuffers.assign(
        m_desc.vertexBuffers,
        m_desc.vertexBuffers + m_desc.vertexBufferCount);
    for (uint32_t i = 0; i < m_desc.vertexBufferCount; ++i)
    {
      const VertexBufferLayout& src = m_desc.vertexBuffers[i];
      size_t start = m_ownedAttributes.size();
      if (src.attributes != nullptr && src.attributeCount > 0)
      {
        m_ownedAttributes.insert(
            m_ownedAttributes.end(),
            src.attributes,
            src.attributes + src.attributeCount);
      }
      m_ownedVertexBuffers[i].attributes = src.attributeCount > 0
                                               ? &m_ownedAttributes[start]
                                               : nullptr;
    }
    m_desc.vertexBuffers = m_ownedVertexBuffers.data();
  }
};

} // namespace rive::ore
