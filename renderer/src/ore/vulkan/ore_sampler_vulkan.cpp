/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/ore/ore_sampler.hpp"
#include "rive/renderer/ore/ore_context.hpp"

namespace rive::ore
{

#if !defined(ORE_BACKEND_GL)

void Sampler::onRefCntReachedZero() const
{
    VkDevice dev = m_vkDevice;
    VkSampler samp = m_vkSampler;
    auto destroyFn = m_vkDestroySampler;
    Context* ctx = m_vkOreContext;

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

#endif // !ORE_BACKEND_GL

} // namespace rive::ore
