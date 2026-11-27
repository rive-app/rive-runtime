/*
 * Copyright 2025 Rive
 */

#include "ore_bind_group_metal.hpp"

namespace rive::ore
{

bool BindGroupMetal::boundSizes(
    const SizedBindings& sized,
    const rcp<BindGroup>* groups,
    std::vector<uint32_t>& out)
{
  out.assign(sized.size(), 0);
  for (size_t i = 0; i < sized.size(); ++i)
  {
    if (sized[i].group >= kMaxBindGroups || groups[sized[i].group] == nullptr)
      continue;
    auto* group = static_cast<BindGroupMetal*>(groups[sized[i].group].get());
    for (const MTLBufferBinding& b : group->m_mtlStorageBuffers)
    {
      if (b.binding == sized[i].binding)
        out[i] = b.size;
    }
  }
  return !out.empty();
}

} // namespace rive::ore
