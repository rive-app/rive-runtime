/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_context.hpp"

namespace rive::ore
{

#if !defined(ORE_BACKEND_GL)

void BindGroup::onRefCntReachedZero() const
{
    // Free the VkDescriptorSet back to the persistent pool. Without
    // this the pool grows unbounded — long editor sessions hit
    // `VK_ERROR_OUT_OF_POOL_MEMORY` on subsequent makeBindGroup calls,
    // which Context returns as a silent nullptr (post-Phase-1 we now
    // also setLastError, but the leak is the underlying problem). The
    // pool was created with `VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT`
    // exactly to support per-set free here.
    //
    // Deferred destruction is handled at the Lua GC level via
    // Context::deferBindGroupDestroy(), which keeps the rcp<> alive
    // until after endFrame(). By the time refcount reaches zero here,
    // the GPU is guaranteed to be done.
    if (m_vkDescriptorSet != VK_NULL_HANDLE && m_context != nullptr &&
        m_context->m_vkPersistentDescriptorPool != VK_NULL_HANDLE)
    {
        m_context->m_vk.FreeDescriptorSets(
            m_context->m_vkDevice,
            m_context->m_vkPersistentDescriptorPool,
            1,
            &m_vkDescriptorSet);
    }
    delete this;
}

#endif // !ORE_BACKEND_GL

} // namespace rive::ore
