/*
 * Copyright 2026 Rive
 */

#pragma once

// Portable C++ binding map matching the on-disk RSTB schema (RFC §3.5,
// §14.5–6). Runtime parses one of these out of an RSTB sidecar via
// `fromBlob`; the editor-side toolchain produces the blob via `toBlob`.
//
// Usage pattern:
//   ore::BindingMap bm;
//   ore::BindingMap::fromBlob(bytes, size, &bm);
//   uint32_t metalSlot = bm.lookup(group, binding,
//                                  ore::ResourceKind::UniformBuffer,
//                                  ore::BindingMap::Stage::VS);
//
// BindingMap is owned by `ore::Pipeline` — populated at pipeline creation
// from the RSTB sidecar, consumed by each flatten backend's
// `makeBindGroup`. Vulkan/WebGPU construct it for reflection parity but
// never read it at bind time.

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>

namespace rive::ore
{

// ----------------------------------------------------------------------------
// Reserved native slots
//
// Per RFC §6.6, Ore reserves a small number of native slots in each
// backend's slot namespace for its own internal machinery (push-constant
// emulation, dynamic-sized storage-buffer length arrays). The binding-map
// allocator must never hand out a user binding at one of these slots, even
// though no current Ore feature consumes them — they are forward-compat
// scaffolding so that adding push constants or compute storage buffers
// later cannot collide with a slot a user shader has already allocated.
//
// Values must agree with the RSTB-emit path in scripting_workspace.
// ----------------------------------------------------------------------------

// Metal `[[buffer(30)]]` reservation. Metal's hardware buffer table is 31
// slots (`MTLBufferTableSize`); reserving the topmost matches Dawn's MSL
// backend convention (`kImmediateBlockBufferSlot`). Targets:
//   1. Push-constant emulation (Vulkan native, D3D12 root constants — Metal
//      has no equivalent, must be emulated as a buffer bind).
//   2. Sizes buffer for dynamic-sized storage-buffer `arrayLength()`
//      lookups (compute path, currently disabled — see
//      `Features::storageBuffers`).
constexpr uint32_t kMetalReservedBufferSlot = 30;

// Maximum buffer slot the binding-map allocator may assign to a user
// binding on Metal. Allocator stops at this; overflow is a hard error.
constexpr uint32_t kMetalMaxUserBufferSlot = kMetalReservedBufferSlot - 1;

// ----------------------------------------------------------------------------
// ResourceKind
//
// Numeric values are the frozen on-disk RSTB schema (RFC §14.5). Never
// renumber existing variants; new variants append at the next integer.
// ----------------------------------------------------------------------------

enum class ResourceKind : uint8_t
{
    UniformBuffer = 0,
    StorageBufferRO = 1,
    StorageBufferRW = 2,
    SampledTexture = 3,
    StorageTexture = 4,
    Sampler = 5,
    ComparisonSampler = 6,
};

// ----------------------------------------------------------------------------
// TextureViewDim / TextureSampleType
//
// Mirror Dawn's `wgpu::TextureViewDimension` and `wgpu::TextureSampleType`
// (matching numeric values) so the WebGPU backend can cast straight into
// the Dawn enums when building `BindGroupLayoutEntry.texture`. The values
// are what Dawn's frontend validator compares against the shader's reflected
// `BindingInfo` (see `ValidateCompatibilityOfSingleBindingWithLayout` in
// Dawn's `ShaderModule.cpp`).
//
// Carried per-entry in `BindingMap::Entry` for every backend — non-WebGPU
// backends ignore these fields since VK/D3D/Metal descriptor types are
// dimension-agnostic.
//
// Numeric values are frozen on-disk RSTB schema (RFC §14.5).
// ----------------------------------------------------------------------------

enum class TextureViewDim : uint8_t
{
    Undefined = 0,
    D1 = 1,
    D2 = 2,
    D2Array = 3,
    Cube = 4,
    CubeArray = 5,
    D3 = 6,
};

enum class TextureSampleType : uint8_t
{
    Undefined = 0,
    Float = 1,
    UnfilterableFloat = 2,
    Depth = 3,
    Sint = 4,
    Uint = 5,
};

// ----------------------------------------------------------------------------
// BindingMap
// ----------------------------------------------------------------------------

class BindingMap
{
public:
    // Shader stage index for per-stage slot lookup. Order matches the
    // fixed-width per-stage backend-slot fields in the on-disk RSTB row.
    enum class Stage : uint8_t
    {
        VS = 0,
        FS = 1,
        CS = 2,
    };

    // Stage bitmask bits — frozen RSTB schema (RFC §14.5).
    static constexpr uint32_t kStageVertex = 1u << 0;
    static constexpr uint32_t kStageFragment = 1u << 1;
    static constexpr uint32_t kStageCompute = 1u << 2;

    // Sentinel for a per-stage slot that the resource is not visible to.
    // The 16-bit width fits every real slot on every backend
    // (Metal: 31, D3D11: 128, GL: ~64).
    static constexpr uint16_t kAbsent = UINT16_MAX;

    // RSTB blob version byte. Bumped when the on-disk schema changes in a
    // way that renders old blobs unreadable; a mismatch on load is a loud
    // error, never a silent misbind (RFC §14.4).
    static constexpr uint8_t kBlobVersion = 2;

    // Allocator version currently supported. Pipelines load with this
    // value; any blob stamped with a different version fails `fromBlob`
    // with a clear error (RFC §14.4).
    //
    // WebGPU-aligned global-counter-per-kind allocation per RFC §3.2.
    static constexpr uint8_t kAllocatorVersion = 1;

    // One row of the binding map. Layout matches the on-disk RSTB row
    // but packed tighter (fewer bits where the semantics allow).
    struct Entry
    {
        uint8_t group;
        uint8_t binding;
        ResourceKind kind;
        uint8_t stageMask;    // Bitwise-OR of kStage* bits.
        uint8_t backendSpace; // D3D12 register space / Vulkan set = group.
        uint16_t backendSlot[3] = {kAbsent, kAbsent, kAbsent}; // [VS, FS, CS]
        // Texture reflection — populated for SampledTexture /
        // StorageTexture kinds; `Undefined` / `false` elsewhere. Consumed
        // by the WebGPU backend's BGL builder to feed Dawn's frontend
        // shader-vs-layout compatibility check. Ignored by VK / D3D /
        // Metal which bind textures via dimension-agnostic descriptors.
        TextureViewDim textureViewDim = TextureViewDim::Undefined;
        TextureSampleType textureSampleType = TextureSampleType::Undefined;
        bool textureMultisampled = false;
    };

    BindingMap() = default;

    // Parse a blob produced by `toBlob` (or by the RSTB-emit path in
    // scripting_workspace). Returns a populated `BindingMap` + `true` on
    // success; empty map + `false` on any size/version mismatch. The
    // caller is expected to surface a clear error on failure — never
    // fall back to a different layout silently.
    //
    // First two bytes of the blob are `kBlobVersion` and
    // `kAllocatorVersion`; mismatch either and parse fails loudly per
    // RFC §14.4. Serialization (`toBlob`) lives in the tooling-gated
    // portion of the API.
    static bool fromBlob(const uint8_t* data, size_t size, BindingMap* out);

    // Per-stage backend-slot lookup. Returns kAbsent when the resource
    // is not in the map, or not visible to the requested stage, or the
    // map entry's kind doesn't match the caller's expected kind.
    //
    // `Sampler` and `ComparisonSampler` are treated as interchangeable
    // — the runtime binding API (both Luau-facing `GPUSampler` and the
    // C++ `BindGroupDesc::SampEntry`) is a single "sampler" category,
    // so the caller can't distinguish them at bind time. The allocator
    // stores whichever kind was declared in WGSL; this helper accepts
    // either side's query.
    //
    // The single hot path the flatten-backend `makeBindGroup` calls at
    // pipeline-binding time.
    uint16_t lookup(uint32_t group,
                    uint32_t binding,
                    ResourceKind kind,
                    Stage stage) const
    {
        const Entry* e = findEntry(group, binding);
        if (e == nullptr)
            return kAbsent;
        const bool kindMatches =
            e->kind == kind ||
            // Collapse sampler / comparison-sampler into one bind-API
            // category. See the docstring above for rationale.
            ((kind == ResourceKind::Sampler ||
              kind == ResourceKind::ComparisonSampler) &&
             (e->kind == ResourceKind::Sampler ||
              e->kind == ResourceKind::ComparisonSampler));
        if (!kindMatches)
            return kAbsent;
        const uint32_t stageBit = 1u << static_cast<uint32_t>(stage);
        if ((e->stageMask & stageBit) == 0)
            return kAbsent;
        return e->backendSlot[static_cast<size_t>(stage)];
    }

    size_t size() const { return m_entries.size(); }
    bool empty() const { return m_entries.empty(); }

    // Iteration accessor. Used by every flatten backend's layout
    // builder at pipeline creation time (Vulkan DSL, D3D12 root sig,
    // WebGPU BGL) to walk the map's entries and emit one layout
    // entry per binding. Runtime-available because those builders
    // ship in non-tooling runtimes (`rive_native` without
    // `WITH_RIVE_TOOLS`).
    const Entry& at(size_t i) const { return m_entries[i]; }

    // ----------------------------------------------------------------
    // Tooling-only API. Compiled only in builds that define
    // WITH_RIVE_TOOLS (editor, scripting_workspace, unit_tests). The
    // shipped runtime binary has none of this — no std::sort, no
    // serialization, no state flag.
    // ----------------------------------------------------------------
#ifdef WITH_RIVE_TOOLS
    // Serialize to the on-disk RSTB sidecar format. Consumed by the
    // RSTB emit path in scripting_workspace; mirrored by fromBlob
    // which is always available at runtime.
    std::vector<uint8_t> toBlob() const;

    // Test / tooling construction: push entries in any order, then
    // finalize to sort into the canonical (group, binding) order.
    void push(const Entry& e)
    {
        m_entries.push_back(e);
        m_finalized = false;
    }

    void finalize();

    bool isFinalized() const { return m_finalized; }

    // Iteration accessor returning the backing vector — tooling-only
    // because raw vector access is used by RSTB emit / inspectors
    // that may modify via const_cast patterns. Runtime code uses
    // `at(i)` + `size()` above.
    const std::vector<Entry>& entries() const { return m_entries; }

    // Slot-unaware lookup for tools that want the full entry.
    const Entry* lookupEntry(uint32_t group, uint32_t binding) const
    {
        return findEntry(group, binding);
    }
#endif

private:
    // Internal binary search — not tooling-gated because lookup() uses it
    // on the hot runtime path.
    const Entry* findEntry(uint32_t group, uint32_t binding) const
    {
#ifdef WITH_RIVE_TOOLS
        assert(m_finalized && "BindingMap::lookup before finalize");
#endif
        auto it = std::lower_bound(
            m_entries.begin(),
            m_entries.end(),
            std::pair<uint32_t, uint32_t>{group, binding},
            [](const Entry& e, const std::pair<uint32_t, uint32_t>& key) {
                return std::pair<uint32_t, uint32_t>{e.group, e.binding} < key;
            });
        if (it == m_entries.end() || it->group != group ||
            it->binding != binding)
        {
            return nullptr;
        }
        return &*it;
    }

    std::vector<Entry> m_entries;
#ifdef WITH_RIVE_TOOLS
    bool m_finalized = false;
#endif
};

// Shared helper for flatten-backend `makeBindGroup` implementations.
// Asserts on missing binding in debug builds; returns kAbsent in release
// so the caller can recover / skip the invalid bind (matches how D3D12
// handles "slot not set" via its slot-mask bitmap).
inline uint16_t lookupBackendSlot(const BindingMap& map,
                                  uint32_t group,
                                  uint32_t binding,
                                  ResourceKind kind,
                                  BindingMap::Stage stage)
{
    uint16_t slot = map.lookup(group, binding, kind, stage);
    assert(slot != BindingMap::kAbsent &&
           "BindingMap lookup failed for (group, binding, kind, stage)");
    return slot;
}

} // namespace rive::ore
