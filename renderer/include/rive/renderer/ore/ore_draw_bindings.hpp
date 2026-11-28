/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_compute_pipeline.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace rive::ore
{

// Same entries, native slots included, so groups of either bind alike.
inline bool bindGroupLayoutsMatch(
    const BindGroupLayout& a,
    const BindGroupLayout& b)
{
  if (&a == &b)
    return true;
  if (a.groupIndex() != b.groupIndex() ||
      a.entries().size() != b.entries().size())
    return false;
  for (const BindGroupLayoutEntry& e : a.entries())
  {
    const BindGroupLayoutEntry* other = b.findEntry(e.binding);
    if (other == nullptr || std::memcmp(&e, other, sizeof(e)) != 0)
      return false;
  }
  return true;
}

// What a script's pass has bound, so each draw or dispatch can check it
// against the pipeline's layouts as WebGPU does, rather than misbind.
class DrawBindings
{
public:
  void setPipeline(Pipeline* pipeline) { m_pipeline = ref_rcp(pipeline); }
  void setPipeline(ComputePipeline* pipeline)
  {
    m_computePipeline = ref_rcp(pipeline);
  }

  void setBindGroup(uint32_t groupIndex, BindGroup* group)
  {
    if (groupIndex < kMaxBindGroups)
      m_groups[groupIndex] = ref_rcp(group);
  }

  // Call as a render pass sets `group`. False when the pass would bind one
  // storage buffer read_write and read only, which WebGPU refuses pass wide.
  bool joinRenderPass(BindGroup* group, std::string* outError)
  {
    if (group->storageUses().empty())
      return true;
    bool joined = false;
    for (const BindGroup::StorageUse& use : group->storageUses())
    {
      bool mixed = bindsWithOtherAccess(*group, use);
      for (const rcp<BindGroup>& set : m_passGroups)
      {
        mixed = mixed || bindsWithOtherAccess(*set, use);
        joined = joined || set.get() == group;
      }
      if (mixed)
      {
        return fail(
            outError,
            group->groupIndex(),
            use.slot,
            "a render pass cannot bind one storage buffer both read_write "
            "and read only");
      }
    }
    if (!joined)
      m_passGroups.push_back(ref_rcp(group));
    return true;
  }

  bool validate(std::string* outError) const
  {
    // A pass is of one kind, so it sets one of the two.
    const rcp<BindGroupLayout>* layouts = m_pipeline != nullptr
                                              ? m_pipeline->m_layouts
                                          : m_computePipeline != nullptr
                                              ? m_computePipeline->m_layouts
                                              : nullptr;
    if (layouts == nullptr)
      return true;
    // A compute pipeline's layouts are its entry point's own.
    const bool entryUseKnown = m_pipeline == nullptr ||
                               m_pipeline->m_entryUseKnown;
    for (uint32_t g = 0; g < kMaxBindGroups; ++g)
    {
      const BindGroupLayout* layout = layouts[g].get();
      if (!hasEntries(layouts[g]))
        continue;
      const BindGroup* group = m_groups[g].get();
      // An older bake's layouts cover whole files, so an unset group
      // may be one its entry points never read.
      if (group == nullptr && !entryUseKnown)
        continue;
      const char* problem =
          group == nullptr ? "the pipeline binds this group but none "
                             "is set"
          : group->layout() == nullptr ||
                  !bindGroupLayoutsMatch(*group->layout(), *layout)
              ? "the bind group's layout does not match the pipeline's; "
                "build it from this pipeline's getBindGroupLayout, or "
                "share layouts explicitly"
              : nullptr;
      if (problem != nullptr)
      {
        if (outError != nullptr)
        {
          char message[224];
          snprintf(message, sizeof(message), "@group(%u): %s", g, problem);
          *outError = message;
        }
        return false;
      }
    }
    for (uint32_t g = 0; g < kMaxBindGroups; ++g)
    {
      if (!groupInUse(layouts, g))
        continue;
      for (const BindGroup::StorageUse& use : m_groups[g]->storageUses())
      {
        const char* problem = use.writable ? writeClash(layouts, use) : nullptr;
        if (problem != nullptr)
          return fail(outError, g, use.slot, problem);
      }
    }
    return true;
  }

private:
  static bool fail(
      std::string* outError,
      uint32_t group,
      uint32_t binding,
      const char* problem)
  {
    if (outError != nullptr)
    {
      char message[224];
      snprintf(
          message,
          sizeof(message),
          "@group(%u) @binding(%u): %s",
          group,
          binding,
          problem);
      *outError = message;
    }
    return false;
  }

  static bool bindsWithOtherAccess(
      const BindGroup& group,
      const BindGroup::StorageUse& use)
  {
    for (const BindGroup::StorageUse& other : group.storageUses())
    {
      if (other.buffer == use.buffer && other.writable != use.writable)
        return true;
    }
    return false;
  }

  bool groupInUse(const rcp<BindGroupLayout>* layouts, uint32_t g) const
  {
    return hasEntries(layouts[g]) && m_groups[g] != nullptr;
  }

  // Null, or why a draw or dispatch that writes through `written` is refused.
  // Direct3D 11 keeps only one writable view of a buffer bound.
  const char* writeClash(
      const rcp<BindGroupLayout>* layouts,
      const BindGroup::StorageUse& written) const
  {
    for (uint32_t g = 0; g < kMaxBindGroups; ++g)
    {
      if (!groupInUse(layouts, g))
        continue;
      for (const BindGroup::StorageUse& other : m_groups[g]->storageUses())
      {
        if (&other == &written || other.buffer != written.buffer)
          continue;
        return other.writable
                   ? "a storage buffer is bound read_write twice in one draw "
                     "or dispatch"
                   : "a storage buffer bound read_write is also bound read "
                     "only in the same draw or dispatch";
      }
    }
    return nullptr;
  }

  rcp<Pipeline> m_pipeline;
  rcp<ComputePipeline> m_computePipeline;
  rcp<BindGroup> m_groups[kMaxBindGroups];
  // Every group with a storage buffer the render pass has set.
  std::vector<rcp<BindGroup>> m_passGroups;
};

} // namespace rive::ore
