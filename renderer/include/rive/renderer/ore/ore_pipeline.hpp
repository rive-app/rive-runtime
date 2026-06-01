/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/renderer/gpu_resource.hpp"
#include "utils/lite_rtti.hpp"
#include "rive/renderer/ore/ore_types.hpp"
#include "rive/renderer/ore/ore_bind_group_layout.hpp"
#include "rive/renderer/ore/ore_shader_module.hpp"

#include <vector>

namespace rive::ore
{

class Context;

class Pipeline : public rive::gpu::GPUResource,
                 public ENABLE_LITE_RTTI(Pipeline)
{
public:
    const PipelineDesc& desc() const { return m_desc; }

    // (group, binding) → per-backend native slot map for this pipeline's
    // resources. Copied from the vertex / fragment ShaderModule at
    // construction. Consumed by each backend's `makeBindGroup` to translate
    // a `BindGroupDesc::*Entry::slot` (= WGSL `@binding`) into the backend's
    // native slot.
    BindingMap m_bindingMap;

    // Layouts the pipeline was created with — one per `@group(N)`. Keeps
    // them alive (the per-backend native handles inside the layouts are
    // referenced by the pipeline's compiled state). Used by `setBindGroup`
    // to verify `bg->layout() == m_layouts[g]` (pointer-equality check
    // matching WebGPU's exact-layout requirement).
    rcp<BindGroupLayout> m_layouts[kMaxBindGroups];

    virtual ~Pipeline() = default;

protected:
    friend class Context;
    friend class RenderPass;

    Pipeline(const PipelineDesc& desc) :
        rive::gpu::GPUResource(nullptr), m_desc(desc)
    {
        // Propagate the binding map from the VS module (or FS if VS is
        // absent, e.g. blit-only pipelines). The two modules are
        // compiled from a single WGSL source so their maps agree, and
        // every module is required to carry a populated map.
        if (desc.vertexModule != nullptr)
        {
            m_bindingMap = desc.vertexModule->m_bindingMap;
        }
        else if (desc.fragmentModule != nullptr)
        {
            m_bindingMap = desc.fragmentModule->m_bindingMap;
        }

        // Stash the user-supplied layouts. Backends overwrite NULL
        // entries (groups the shader doesn't bind) with a no-op
        // BindGroupLayout if needed for empty-set semantics.
        const uint32_t count = std::min(desc.bindGroupLayoutCount,
                                        static_cast<uint32_t>(kMaxBindGroups));
        for (uint32_t i = 0; i < count; ++i)
        {
            m_layouts[i] = ref_rcp(desc.bindGroupLayouts[i]);
        }
    }

    Pipeline(rcp<rive::gpu::GPUResourceManager> manager,
             const PipelineDesc& desc) :
        rive::gpu::GPUResource(std::move(manager)), m_desc(desc)
    {
        // Propagate the binding map from the VS module (or FS if VS is
        // absent, e.g. blit-only pipelines). The two modules are
        // compiled from a single WGSL source so their maps agree, and
        // every module is required to carry a populated map.
        if (desc.vertexModule != nullptr)
        {
            m_bindingMap = desc.vertexModule->m_bindingMap;
        }
        else if (desc.fragmentModule != nullptr)
        {
            m_bindingMap = desc.fragmentModule->m_bindingMap;
        }

        // Stash the user-supplied layouts. Backends overwrite NULL
        // entries (groups the shader doesn't bind) with a no-op
        // BindGroupLayout if needed for empty-set semantics.
        const uint32_t count = std::min(desc.bindGroupLayoutCount,
                                        static_cast<uint32_t>(kMaxBindGroups));
        for (uint32_t i = 0; i < count; ++i)
        {
            m_layouts[i] = ref_rcp(desc.bindGroupLayouts[i]);
        }
    }

    PipelineDesc m_desc;
};

} // namespace rive::ore
