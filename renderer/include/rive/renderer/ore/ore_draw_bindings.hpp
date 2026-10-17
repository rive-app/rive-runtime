/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"

#include <cstdio>
#include <cstring>
#include <string>

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

// What a script's pass has bound, so each draw can check its bind groups
// against the pipeline's layouts as WebGPU does, rather than misbind.
class DrawBindings
{
public:
  void setPipeline(Pipeline* pipeline) { m_pipeline = ref_rcp(pipeline); }

  void setBindGroup(uint32_t groupIndex, BindGroup* group)
  {
    if (groupIndex < kMaxBindGroups)
      m_groups[groupIndex] = ref_rcp(group);
  }

  bool validate(std::string* outError) const
  {
    if (m_pipeline == nullptr)
      return true;
    for (uint32_t g = 0; g < kMaxBindGroups; ++g)
    {
      const BindGroupLayout* layout = m_pipeline->m_layouts[g].get();
      if (layout == nullptr || layout->entries().empty())
        continue;
      const BindGroup* group = m_groups[g].get();
      // An older bake's layouts cover whole files, so an unset group
      // may be one its entry points never read.
      if (group == nullptr && !m_pipeline->m_entryUseKnown)
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
    return true;
  }

private:
  rcp<Pipeline> m_pipeline;
  rcp<BindGroup> m_groups[kMaxBindGroups];
};

} // namespace rive::ore
