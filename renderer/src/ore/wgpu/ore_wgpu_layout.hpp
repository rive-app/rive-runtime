/*
 * Copyright 2026 Rive
 *
 * Private helpers for building explicit `wgpu::BindGroupLayout` +
 * `wgpu::PipelineLayout` from the pipeline's `ore::BindingMap`
 * (RFC v5 §3.2.2 / §14.1 — "Commit 7c").
 *
 * Why explicit layout over WebGPU's `layout: "auto"`:
 *   - Auto-layout numbering is implementation-visible and has diverged
 *     across Chrome/Firefox in the past (RFC v5 §14.1). Ore's stability
 *     contract promises that a WGSL shader's on-disk RSTB keeps
 *     rendering the same forever; auto-layout breaks that promise if
 *     Dawn/wgpu ever retune.
 *   - Explicit layout lets callers declare `hasDynamicOffset` per UBO
 *     at pipeline-creation time (same plumbing as
 *     `PipelineDesc::dynamicUBOs` on Vulkan — see
 *     ore_vulkan_dsl.hpp).
 *   - Aligns the WebGPU backend with Vulkan/D3D12 — every backend
 *     builds its layout from the binding map.
 */

#pragma once

#include "rive/renderer/ore/ore_binding_map.hpp"
#include "rive/renderer/ore/ore_types.hpp"

#include <webgpu/webgpu_cpp.h>

#include <array>
#include <cassert>
#include <cstdint>

namespace rive::ore
{

// WebGPU-side alias for Ore's central `kMaxBindGroups`
// (RFC v4 §9.1 / §14.2). Single source of truth in `ore_types.hpp`.
constexpr uint32_t kWGPUMaxGroups = kMaxBindGroups;

// Map `ore::TextureViewDim` → `wgpu::TextureViewDimension`. Numeric
// values match so we could `static_cast`, but an explicit switch is
// robust against either side reordering.
inline wgpu::TextureViewDimension toWGPUViewDim(TextureViewDim d)
{
    switch (d)
    {
        case TextureViewDim::Undefined:
            // Shader reflection didn't populate this field (the entry
            // came from a non-WebGPU allocator path that doesn't carry
            // dimension metadata). `e2D` is the safe default — Dawn's
            // frontend validator only checks the value when the shader
            // also declares a dimension, so this only matters for
            // entries the shader doesn't actually use.
            return wgpu::TextureViewDimension::e2D;
        case TextureViewDim::D1:
            return wgpu::TextureViewDimension::e1D;
        case TextureViewDim::D2:
            return wgpu::TextureViewDimension::e2D;
        case TextureViewDim::D2Array:
            return wgpu::TextureViewDimension::e2DArray;
        case TextureViewDim::Cube:
            return wgpu::TextureViewDimension::Cube;
        case TextureViewDim::CubeArray:
            return wgpu::TextureViewDimension::CubeArray;
        case TextureViewDim::D3:
            return wgpu::TextureViewDimension::e3D;
    }
    return wgpu::TextureViewDimension::e2D;
}

// Map `ore::TextureSampleType` → `wgpu::TextureSampleType`. Dawn's
// `ValidateCompatibilityOfSingleBindingWithLayout` only accepts a
// shader `Depth` binding against a layout `Depth` or `UnfilterableFloat`;
// `Float` is rejected — so we must emit `Depth` when the shader reflects
// it.
inline wgpu::TextureSampleType toWGPUSampleType(TextureSampleType s)
{
    switch (s)
    {
        case TextureSampleType::Undefined:
        case TextureSampleType::Float:
            return wgpu::TextureSampleType::Float;
        case TextureSampleType::UnfilterableFloat:
            return wgpu::TextureSampleType::UnfilterableFloat;
        case TextureSampleType::Depth:
            return wgpu::TextureSampleType::Depth;
        case TextureSampleType::Sint:
            return wgpu::TextureSampleType::Sint;
        case TextureSampleType::Uint:
            return wgpu::TextureSampleType::Uint;
    }
    return wgpu::TextureSampleType::Float;
}

// Map an `ore::ResourceKind` plus texture reflection onto a
// `wgpu::BindGroupLayoutEntry`. UBOs with `hasDynamicOffset=true` are
// the declared dynamic-offset case from `PipelineDesc::dynamicUBOs`.
//
// `textureViewDim`, `textureSampleType`, and `textureMultisampled` are
// the shader-reflected metadata from the `BindingMap::Entry`. Dawn compares
// these against its own Tint-reflected view of the shader module in
// `ValidateCompatibilityOfSingleBindingWithLayout` (see
// src/dawn/native/ShaderModule.cpp) — mismatches produce
// "The shader's binding dimension (...) doesn't match the layout's"
// errors at pipeline-creation time.
inline wgpu::BindGroupLayoutEntry makeWGPUBGLEntry(
    uint32_t binding,
    ResourceKind kind,
    bool hasDynamicOffset,
    wgpu::ShaderStage visibility,
    TextureViewDim textureViewDim = TextureViewDim::Undefined,
    TextureSampleType textureSampleType = TextureSampleType::Undefined,
    bool textureMultisampled = false)
{
    wgpu::BindGroupLayoutEntry e{};
    e.binding = binding;
    e.visibility = visibility;

    switch (kind)
    {
        case ResourceKind::UniformBuffer:
            e.buffer.type = wgpu::BufferBindingType::Uniform;
            e.buffer.hasDynamicOffset = hasDynamicOffset;
            break;
        case ResourceKind::StorageBufferRO:
            e.buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
            break;
        case ResourceKind::StorageBufferRW:
            e.buffer.type = wgpu::BufferBindingType::Storage;
            break;
        case ResourceKind::SampledTexture:
            e.texture.sampleType = toWGPUSampleType(textureSampleType);
            e.texture.viewDimension = toWGPUViewDim(textureViewDim);
            e.texture.multisampled = textureMultisampled;
            break;
        case ResourceKind::StorageTexture:
            // Ore doesn't ship storage textures on day 1 (RFC §13).
            // When it does, `format` and `access` come from WGSL
            // reflection — stubbed here for forward-compat. Using the
            // C enum constant through a cast because Dawn spells this
            // `Rgba8Unorm` while wagyu spells it `RGBA8Unorm`; the
            // underlying `WGPUTextureFormat_RGBA8Unorm` is stable
            // across both.
            e.storageTexture.access = wgpu::StorageTextureAccess::WriteOnly;
            e.storageTexture.format =
                static_cast<wgpu::TextureFormat>(WGPUTextureFormat_RGBA8Unorm);
            e.storageTexture.viewDimension = toWGPUViewDim(textureViewDim);
            break;
        case ResourceKind::Sampler:
            e.sampler.type = wgpu::SamplerBindingType::Filtering;
            break;
        case ResourceKind::ComparisonSampler:
            e.sampler.type = wgpu::SamplerBindingType::Comparison;
            break;
    }
    return e;
}

// (Legacy `buildWGPUBindGroupLayout` driven by the pipeline's BindingMap
// was removed in Phase E. Layouts are now built from public
// `BindGroupLayoutDesc` via `buildWGPUBindGroupLayoutFromDesc` below; the
// pipeline composes them into a `wgpu::PipelineLayout` directly in
// `Context::makePipeline`.)

// Map `ore::BindingKind` (public layout API) → wgpu BGL entry shape.
// Mirror of `makeWGPUBGLEntry` for the public-API path. Used by
// `buildWGPUBindGroupLayoutFromDesc` (below) which builds from a
// caller-supplied `BindGroupLayoutDesc` rather than the binding map.
inline wgpu::BindGroupLayoutEntry makeWGPUBGLEntryFromDesc(
    const BindGroupLayoutEntry& src)
{
    wgpu::BindGroupLayoutEntry e{};
    e.binding = src.binding;

    wgpu::ShaderStage vis = wgpu::ShaderStage::None;
    if (src.visibility.mask & StageVisibility::kVertex)
        vis |= wgpu::ShaderStage::Vertex;
    if (src.visibility.mask & StageVisibility::kFragment)
        vis |= wgpu::ShaderStage::Fragment;
    if (src.visibility.mask & StageVisibility::kCompute)
        vis |= wgpu::ShaderStage::Compute;
    e.visibility = vis;

    auto toViewDim = [](TextureViewDimension d) {
        switch (d)
        {
            case TextureViewDimension::texture2D:
                return wgpu::TextureViewDimension::e2D;
            case TextureViewDimension::cube:
                return wgpu::TextureViewDimension::Cube;
            case TextureViewDimension::texture3D:
                return wgpu::TextureViewDimension::e3D;
            case TextureViewDimension::array2D:
                return wgpu::TextureViewDimension::e2DArray;
            case TextureViewDimension::cubeArray:
                return wgpu::TextureViewDimension::CubeArray;
        }
        return wgpu::TextureViewDimension::e2D;
    };

    auto toSampleType = [](BindGroupLayoutEntry::SampleType s) {
        using ST = BindGroupLayoutEntry::SampleType;
        switch (s)
        {
            case ST::floatFilterable:
                return wgpu::TextureSampleType::Float;
            case ST::floatUnfilterable:
                return wgpu::TextureSampleType::UnfilterableFloat;
            case ST::depth:
                return wgpu::TextureSampleType::Depth;
            case ST::sint:
                return wgpu::TextureSampleType::Sint;
            case ST::uint:
                return wgpu::TextureSampleType::Uint;
        }
        return wgpu::TextureSampleType::Float;
    };

    switch (src.kind)
    {
        case BindingKind::uniformBuffer:
            e.buffer.type = wgpu::BufferBindingType::Uniform;
            e.buffer.hasDynamicOffset = src.hasDynamicOffset;
            e.buffer.minBindingSize = src.minBindingSize;
            break;
        case BindingKind::storageBufferRO:
            e.buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
            e.buffer.minBindingSize = src.minBindingSize;
            break;
        case BindingKind::storageBufferRW:
            e.buffer.type = wgpu::BufferBindingType::Storage;
            e.buffer.minBindingSize = src.minBindingSize;
            break;
        case BindingKind::sampledTexture:
            e.texture.sampleType = toSampleType(src.textureSampleType);
            e.texture.viewDimension = toViewDim(src.textureViewDim);
            e.texture.multisampled = src.textureMultisampled;
            break;
        case BindingKind::storageTexture:
            e.storageTexture.access = wgpu::StorageTextureAccess::WriteOnly;
            e.storageTexture.format =
                static_cast<wgpu::TextureFormat>(WGPUTextureFormat_RGBA8Unorm);
            e.storageTexture.viewDimension = toViewDim(src.textureViewDim);
            break;
        case BindingKind::sampler:
            e.sampler.type = wgpu::SamplerBindingType::Filtering;
            break;
        case BindingKind::comparisonSampler:
            e.sampler.type = wgpu::SamplerBindingType::Comparison;
            break;
    }
    return e;
}

// Build a `wgpu::BindGroupLayout` from the public `BindGroupLayoutDesc` —
// the path called from `Context::makeBindGroupLayout`. Each entry's
// visibility and texture metadata come from the caller's desc, not from
// shader reflection.
inline wgpu::BindGroupLayout buildWGPUBindGroupLayoutFromDesc(
    const wgpu::Device& device,
    const BindGroupLayoutDesc& desc)
{
    constexpr uint32_t kMaxEntriesPerGroup = 16;
    wgpu::BindGroupLayoutEntry entries[kMaxEntriesPerGroup];
    const uint32_t n =
        std::min(desc.entryCount, static_cast<uint32_t>(kMaxEntriesPerGroup));
    for (uint32_t i = 0; i < n; ++i)
    {
        entries[i] = makeWGPUBGLEntryFromDesc(desc.entries[i]);
    }

    wgpu::BindGroupLayoutDescriptor bglDesc{};
    bglDesc.label = desc.label ? desc.label : "";
    bglDesc.entryCount = n;
    bglDesc.entries = n > 0 ? entries : nullptr;
    return device.CreateBindGroupLayout(&bglDesc);
}

// (Legacy `buildWGPUPipelineLayout` removed in Phase E — pipelines now
// take pre-built `wgpu::BindGroupLayout`s from each user-supplied
// `BindGroupLayout::m_wgpuBGL` and compose them inline.)

} // namespace rive::ore
