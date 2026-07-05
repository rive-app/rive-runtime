/*
 * Copyright 2025 Rive
 */

#include "ore_bind_group_vulkan.hpp"
#include "ore_buffer_vulkan.hpp"

#include <cassert>

namespace rive::ore
{

// Pool destruction handles every cached set, no per-set free.
BindGroupVulkan::~BindGroupVulkan() = default;

void BindGroupVulkan::markUBOsBound()
{
    for (const auto& w : m_uboWrites)
        w.buffer->markBound();
}

VkDescriptorSet BindGroupVulkan::resolveDescriptorSet()
{
    auto* ctx = static_cast<ContextVulkan*>(m_context);

    // Reuse the cached set if every UBO is on the same backing as last time.
    for (const auto& cached : m_setCache)
    {
        bool match = true;
        for (size_t i = 0; i < m_uboWrites.size(); ++i)
        {
            if (cached.key[i] != m_uboWrites[i].buffer->current())
            {
                match = false;
                break;
            }
        }
        if (match)
            return cached.set;
    }

    auto alloc = ctx->vkAllocateDescriptorSet(m_vkDSL);
    // Only fails on device OOM (the generation auto rolls on capacity). Return
    // null so setBindGroup reports it via setLastError instead of aborting.
    if (alloc.set == VK_NULL_HANDLE)
        return VK_NULL_HANDLE;

    constexpr uint32_t kMaxWrites = 32;
    assert(m_uboWrites.size() + m_imageWrites.size() <= kMaxWrites);
    VkWriteDescriptorSet writes[kMaxWrites] = {};
    VkDescriptorBufferInfo bufferInfos[kMaxWrites] = {};
    VkDescriptorImageInfo imageInfos[kMaxWrites] = {};
    uint32_t writeIdx = 0;

    CachedSet cached;
    cached.key.reserve(m_uboWrites.size());
    for (const auto& u : m_uboWrites)
    {
        VkBuffer vkBuffer = u.buffer->current();
        cached.key.push_back(vkBuffer);

        VkDescriptorBufferInfo& bi = bufferInfos[writeIdx];
        bi.buffer = vkBuffer;
        bi.offset = u.offset;
        bi.range = u.range;

        VkWriteDescriptorSet& w = writes[writeIdx++];
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = alloc.set;
        w.dstBinding = u.dstBinding;
        w.dstArrayElement = 0;
        w.descriptorCount = 1;
        w.descriptorType = u.type;
        w.pBufferInfo = &bi;
    }
    for (const auto& im : m_imageWrites)
    {
        VkDescriptorImageInfo& ii = imageInfos[writeIdx];
        ii.imageView = im.imageView;
        ii.imageLayout = im.imageLayout;
        ii.sampler = im.sampler;

        VkWriteDescriptorSet& w = writes[writeIdx++];
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = alloc.set;
        w.dstBinding = im.dstBinding;
        w.dstArrayElement = 0;
        w.descriptorCount = 1;
        w.descriptorType = im.type;
        w.pImageInfo = &ii;
    }

    if (writeIdx > 0)
        ctx->m_vk->UpdateDescriptorSets(ctx->m_vk->device,
                                        writeIdx,
                                        writes,
                                        0,
                                        nullptr);

    cached.set = alloc.set;
    cached.pool = std::move(alloc.pool);
    m_setCache.push_back(std::move(cached));
    return m_setCache.back().set;
}

} // namespace rive::ore
