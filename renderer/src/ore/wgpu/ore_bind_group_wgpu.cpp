/*
 * Copyright 2025 Rive
 */

#include "ore_bind_group_wgpu.hpp"
#include "ore_buffer_wgpu.hpp"
#include "rive/renderer/ore/ore_context_wgpu.hpp"

namespace rive::ore
{

void BindGroupWGPU::markUBOsBound()
{
    for (const auto& u : m_uboEntries)
        u.buffer->markBound();
}

const wgpu::BindGroup& BindGroupWGPU::resolveBindGroup()
{
    // Reuse a cached bind group if every UBO is on the same backing as when it
    // was built.
    for (const auto& cached : m_cache)
    {
        bool match = true;
        for (size_t i = 0; i < m_uboEntries.size(); ++i)
        {
            if (cached.key[i] != m_uboEntries[i].buffer->current().Get())
            {
                match = false;
                break;
            }
        }
        if (match)
            return cached.bindGroup;
    }

    std::vector<wgpu::BindGroupEntry> entries;
    entries.reserve(m_uboEntries.size() + m_texEntries.size() +
                    m_sampEntries.size());
    CachedGroup cached;
    cached.key.reserve(m_uboEntries.size());
    for (const auto& u : m_uboEntries)
    {
        cached.key.push_back(u.buffer->current().Get());
        wgpu::BindGroupEntry e{};
        e.binding = u.binding;
        e.buffer = u.buffer->current();
        e.offset = u.offset;
        e.size = u.size;
        entries.push_back(e);
    }
    for (const auto& t : m_texEntries)
    {
        wgpu::BindGroupEntry e{};
        e.binding = t.binding;
        e.textureView = t.view;
        entries.push_back(e);
    }
    for (const auto& s : m_sampEntries)
    {
        wgpu::BindGroupEntry e{};
        e.binding = s.binding;
        e.sampler = s.sampler;
        entries.push_back(e);
    }

    auto* ctx = static_cast<ContextWGPU*>(m_context);
    wgpu::BindGroupDescriptor bgDesc{};
    bgDesc.layout = m_wgpuBGL;
    bgDesc.label = m_label.empty() ? nullptr : m_label.c_str();
    bgDesc.entryCount = static_cast<uint32_t>(entries.size());
    bgDesc.entries = entries.empty() ? nullptr : entries.data();
    cached.bindGroup = ctx->m_wgpuDevice.CreateBindGroup(&bgDesc);
    if (cached.bindGroup == nullptr)
        return m_nullBindGroup; // Don't cache a failed creation; retry later.

    m_cache.push_back(std::move(cached));
    return m_cache.back().bindGroup;
}

} // namespace rive::ore
