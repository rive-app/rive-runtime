/*
 * Copyright 2026 Rive
 */

// BindingMap::fromFFI — the only piece of BindingMap that depends on the
// naga_ffi Rust library. Split into its own translation unit so linkers
// can dead-strip it in builds (e.g. unit_tests) that don't link the
// naga_ffi static library.
//
// Forward-declares the full C ABI layout we consume. Must stay in lockstep
// with packages/scripting_workspace/naga_ffi/naga_ffi.h — Rust-side
// `naga_compiled_binding_map_entry` writes the complete struct, so a
// smaller forward decl here would be overwritten past its end. Add new
// fields here (at the end) whenever the FFI struct grows per RFC §14.6.

#include "rive/renderer/ore/ore_binding_map.hpp"

#include <algorithm>

extern "C"
{
    struct NagaBindingMapEntry
    {
        uint32_t group;
        uint32_t binding;
        uint32_t binding_index;
        uint32_t kind;
        uint32_t stage_mask;
        uint32_t backend_slot_vs;
        uint32_t backend_slot_fs;
        uint32_t backend_slot_cs;
        uint32_t backend_space;
        // Texture reflection carried for the WebGPU BGL builder; see
        // `NAGA_VIEW_DIM_*` / `NAGA_SAMPLE_TYPE_*` in naga_ffi.h.
        uint32_t texture_view_dim;
        uint32_t texture_sample_type;
        bool texture_multisampled;
    };
    uint32_t naga_compiled_allocator_version(const NagaCompiledShader* shader);
    uint32_t naga_compiled_binding_map_count(const NagaCompiledShader* shader);
    bool naga_compiled_binding_map_entry(const NagaCompiledShader* shader,
                                         uint32_t index,
                                         NagaBindingMapEntry* out);
}

namespace rive::ore
{

BindingMap BindingMap::fromFFI(const NagaCompiledShader* shader)
{
    BindingMap map;
    if (shader == nullptr)
        return map;

    // Version gate: bail cleanly if the FFI side advances its version
    // independently of the runtime. Currently both are 0.
    const uint32_t allocVer = naga_compiled_allocator_version(shader);
    if (allocVer != kAllocatorVersion)
        return map;

    const uint32_t count = naga_compiled_binding_map_count(shader);
    // Populate m_entries directly — entries arrive from naga_ffi already
    // sorted by (group, binding) (Rust-side `binding_alloc::BindingMap`
    // finalizes before returning). No sort needed here.
    map.m_entries.reserve(count);
    for (uint32_t i = 0; i < count; ++i)
    {
        NagaBindingMapEntry src{};
        if (!naga_compiled_binding_map_entry(shader, i, &src))
            continue;

        Entry e{};
        e.group = static_cast<uint8_t>(src.group);
        e.binding = static_cast<uint8_t>(src.binding);
        e.kind = static_cast<ResourceKind>(src.kind);
        e.stageMask = static_cast<uint8_t>(src.stage_mask);
        e.backendSpace = static_cast<uint8_t>(src.backend_space);
        // NAGA_SLOT_ABSENT (= UINT32_MAX) collapses to our kAbsent sentinel.
        auto toSlot = [](uint32_t s) -> uint16_t {
            return s == UINT32_MAX ? BindingMap::kAbsent
                                   : static_cast<uint16_t>(
                                         std::min<uint32_t>(s, UINT16_MAX - 1));
        };
        e.backendSlot[0] = toSlot(src.backend_slot_vs);
        e.backendSlot[1] = toSlot(src.backend_slot_fs);
        e.backendSlot[2] = toSlot(src.backend_slot_cs);
        e.textureViewDim = static_cast<TextureViewDim>(
            static_cast<uint8_t>(src.texture_view_dim));
        e.textureSampleType = static_cast<TextureSampleType>(
            static_cast<uint8_t>(src.texture_sample_type));
        e.textureMultisampled = src.texture_multisampled;
        map.m_entries.push_back(e);
    }
#ifdef WITH_RIVE_TOOLS
    // Flip the finalized flag so tooling-build lookups satisfy their assert.
    // The naga_ffi side guarantees sorted output; no std::sort call.
    map.m_finalized = true;
#endif
    return map;
}

} // namespace rive::ore
