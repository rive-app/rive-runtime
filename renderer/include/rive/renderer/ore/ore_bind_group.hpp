/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/renderer/gpu_resource.hpp"
#include "utils/lite_rtti.hpp"
#include "rive/renderer/ore/ore_types.hpp"
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_texture.hpp"
#include "rive/renderer/ore/ore_sampler.hpp"
#include "rive/renderer/ore/ore_bind_group_layout.hpp"

#include <vector>

namespace rive::ore
{

class Context;

// Pre-baked resource bindings that can be reused across many draw calls.
// Holds strong rcp<> references to all bound resources (Buffer, TextureView,
// Sampler), ensuring they remain alive for the BindGroup's lifetime.
//
// Created via Context::makeBindGroup(). Bound via RenderPass::setBindGroup().
class BindGroup : public rive::gpu::GPUResource,
                  public ENABLE_LITE_RTTI(BindGroup)
{
public:
    uint32_t dynamicOffsetCount() const { return m_dynamicOffsetCount; }
    uint32_t groupIndex() const
    {
        return m_layoutRef ? m_layoutRef->groupIndex() : 0;
    }
    BindGroupLayout* layout() const { return m_layoutRef.get(); }
    Context* context() const { return m_context; }

    virtual ~BindGroup() = default;

protected:
    friend class Context;
    friend class RenderPass;

    BindGroup() : rive::gpu::GPUResource(nullptr) {}
    BindGroup(rcp<rive::gpu::GPUResourceManager> manager) :
        rive::gpu::GPUResource(std::move(manager))
    {}

    uint32_t m_dynamicOffsetCount = 0;

    // The layout this BindGroup conforms to. Holds the per-backend native
    // layout handle alive for the BindGroup's lifetime — Vulkan's
    // VkDescriptorSetLayout in particular must outlive every VkDescriptorSet
    // allocated from it.
    rcp<BindGroupLayout> m_layoutRef;

    // Lifecycle: hold rcp<> refs to all bound resources so they stay alive
    // even if the caller drops their references before the BindGroup is
    // destroyed.
    std::vector<rcp<Buffer>> m_retainedBuffers;
    std::vector<rcp<TextureView>> m_retainedViews;
    std::vector<rcp<Sampler>> m_retainedSamplers;

    // Context back-pointer set in makeBindGroup(). Used by the Lua GC
    // to call context->deferBindGroupDestroy() instead of dropping the
    // last rcp<> directly, keeping the object alive until endFrame().
    Context* m_context = nullptr;
};

} // namespace rive::ore
