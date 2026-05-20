/*
 * Copyright 2025 Rive
 */

#include "ore_sampler_vulkan.hpp"
#include "rive/renderer/ore/ore_context_vulkan.hpp"

namespace rive::ore
{

void SamplerVulkan::onRefCntReachedZero() const
{
    VkDevice dev = m_vkDevice;
    VkSampler samp = m_vkSampler;
    auto destroyFn = m_vkDestroySampler;
    ContextVulkan* ctx = m_vkOreContext;

    auto destroy = [=]() {
        if (samp != VK_NULL_HANDLE && destroyFn != nullptr)
            destroyFn(dev, samp, nullptr);
    };

    delete this;

    if (ctx != nullptr)
        ctx->vkDeferDestroy(std::move(destroy));
    else
        destroy();
}

} // namespace rive::ore
