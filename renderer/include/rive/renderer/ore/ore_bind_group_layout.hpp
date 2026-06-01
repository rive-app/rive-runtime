/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/renderer/gpu_resource.hpp"
#include "utils/lite_rtti.hpp"
#include "rive/renderer/ore/ore_types.hpp"
#include "rive/renderer/ore/ore_binding_map.hpp"

#include <string>
#include <vector>

namespace rive::ore
{

class Context;
class ContextMetal;
class ContextGL;
class ContextD3D11;

// Public Ore type — created via `Context::makeBindGroupLayout`. Carries the
// user-supplied entries plus per-backend baked layout handles.
//
// Lifetime: outlives any `Pipeline` or `BindGroup` that references it.
// `Pipeline` holds `rcp<BindGroupLayout> m_layouts[kMaxBindGroups]`;
// `BindGroup` holds `rcp<BindGroupLayout> m_layoutRef`.
class BindGroupLayout : public rive::gpu::GPUResource,
                        public ENABLE_LITE_RTTI(BindGroupLayout)
{
public:
    uint32_t groupIndex() const { return m_groupIndex; }
    const std::vector<BindGroupLayoutEntry>& entries() const
    {
        return m_entries;
    }

    // True if entry for `binding` (within this layout's group) is a UBO
    // declared with `hasDynamicOffset = true`.
    bool hasDynamicOffset(uint32_t binding) const;

    // Find the entry for a given binding. Returns nullptr if not present.
    const BindGroupLayoutEntry* findEntry(uint32_t binding) const;

    virtual ~BindGroupLayout() = default;

protected:
    friend class Context;
    friend class ContextMetal;
    friend class ContextGL;
    friend class ContextD3D11;

    BindGroupLayout() : rive::gpu::GPUResource(nullptr) {}

    BindGroupLayout(rcp<rive::gpu::GPUResourceManager> manager) :
        rive::gpu::GPUResource(std::move(manager))
    {}

    uint32_t m_groupIndex = 0;
    std::vector<BindGroupLayoutEntry> m_entries;

    // Context back-pointer for deferred-destruction routing. Weak ref.
    Context* m_context = nullptr;
};

// Validate user-supplied layouts against the shader's reflected binding map.
//
// Walks every entry in `bindingMap` and confirms the layout for that group
// declares a matching entry: same WGSL @binding, kind, visibility >=
// shader's stageMask, texture dim/sample type compatible.
//
// Returns true on success. On failure, populates `*outError` with a human-
// readable diagnostic and returns false. Never asserts.
bool validateLayoutsAgainstBindingMap(const BindingMap& bindingMap,
                                      BindGroupLayout* const* layouts,
                                      uint32_t layoutCount,
                                      std::string* outError);

// Color outputs require a fragment shader.
bool validateColorRequiresFragment(uint32_t colorCount,
                                   bool hasFragmentModule,
                                   std::string* outError);

} // namespace rive::ore
