/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_render_pass.hpp"
#include "rive/renderer/ore/ore_script_guards.hpp"

#include <cassert>
#include <cstring>

namespace rive::ore
{

// setBindGroup calls a script makes before its first setPipeline. WebGPU
// allows that order, but Vulkan and D3D12 record a bind against the bound
// pipeline's layout, so the call waits for the pipeline.
class DeferredBindGroups
{
public:
    void defer(uint32_t groupIndex,
               BindGroup* group,
               const uint32_t* offsets,
               uint32_t offsetCount)
    {
        assert(groupIndex < kMaxBindGroups &&
               offsetCount <= kMaxDynamicOffsets);
        Entry& entry = m_entries[groupIndex];
        entry.group = ref_rcp(group);
        entry.offsetCount = offsetCount;
        if (offsetCount != 0)
        {
            memcpy(entry.offsets, offsets, offsetCount * sizeof(uint32_t));
        }
    }

    // Binds what waited, once the pass has a pipeline.
    void flush(RenderPass* pass)
    {
        for (uint32_t i = 0; i < kMaxBindGroups; ++i)
        {
            Entry& entry = m_entries[i];
            if (entry.group != nullptr)
            {
                pass->setBindGroup(i,
                                   entry.group.get(),
                                   entry.offsetCount != 0 ? entry.offsets
                                                          : nullptr,
                                   entry.offsetCount);
                entry.group = nullptr;
            }
        }
    }

private:
    struct Entry
    {
        rcp<BindGroup> group;
        uint32_t offsets[kMaxDynamicOffsets] = {};
        uint32_t offsetCount = 0;
    };
    Entry m_entries[kMaxBindGroups];
};

} // namespace rive::ore
