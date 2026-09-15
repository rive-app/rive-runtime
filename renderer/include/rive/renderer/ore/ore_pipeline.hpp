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
    // resources. Merged from the vertex and fragment ShaderModules at
    // construction, each stage taken from the module that compiled it.
    // Consumed by each backend's `makeBindGroup` to translate a
    // `BindGroupDesc::*Entry::slot` (= WGSL `@binding`) into the backend's
    // native slot.
    BindingMap m_bindingMap;

    // Which sampler serves which texture. GL needs it: GLSL folds the two
    // into one uniform at the texture's unit. Empty on other backends.
    std::vector<ShaderModule::TextureSamplerPair> m_textureSamplerPairs;

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

    Pipeline(const PipelineDesc& desc) : Pipeline(nullptr, desc) {}

    Pipeline(rcp<rive::gpu::GPUResourceManager> manager,
             const PipelineDesc& desc) :
        rive::gpu::GPUResource(std::move(manager)), m_desc(desc)
    {
        m_bindingMap =
            bindingMapForStages(desc.vertexModule, desc.fragmentModule);
        // Unioned: each GLSL entry point knows only its own pairs.
        for (const ShaderModule* shaderModule :
             {desc.vertexModule, desc.fragmentModule})
        {
            if (shaderModule == nullptr)
                continue;
            m_textureSamplerPairs.insert(
                m_textureSamplerPairs.end(),
                shaderModule->m_textureSamplerPairs.begin(),
                shaderModule->m_textureSamplerPairs.end());
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
        ownVertexLayout();
    }

    PipelineDesc m_desc;

private:
    // The desc's vertex layout points into caller memory the deferred replay
    // frees right after makePipeline, so deep copy it into owned storage.
    std::vector<VertexBufferLayout> m_ownedVertexBuffers;
    std::vector<VertexAttribute> m_ownedAttributes;

    void ownVertexLayout()
    {
        if (m_desc.vertexBufferCount == 0 || m_desc.vertexBuffers == nullptr)
        {
            m_desc.vertexBuffers = nullptr;
            m_desc.vertexBufferCount = 0;
            return;
        }
        // Reserve up front so the vector never reallocates while we repoint
        // into it.
        size_t total = 0;
        for (uint32_t i = 0; i < m_desc.vertexBufferCount; ++i)
        {
            total += m_desc.vertexBuffers[i].attributeCount;
        }
        m_ownedAttributes.reserve(total);
        m_ownedVertexBuffers.assign(m_desc.vertexBuffers,
                                    m_desc.vertexBuffers +
                                        m_desc.vertexBufferCount);
        for (uint32_t i = 0; i < m_desc.vertexBufferCount; ++i)
        {
            const VertexBufferLayout& src = m_desc.vertexBuffers[i];
            size_t start = m_ownedAttributes.size();
            if (src.attributes != nullptr && src.attributeCount > 0)
            {
                m_ownedAttributes.insert(m_ownedAttributes.end(),
                                         src.attributes,
                                         src.attributes + src.attributeCount);
            }
            m_ownedVertexBuffers[i].attributes =
                src.attributeCount > 0 ? &m_ownedAttributes[start] : nullptr;
        }
        m_desc.vertexBuffers = m_ownedVertexBuffers.data();
    }
};

} // namespace rive::ore
