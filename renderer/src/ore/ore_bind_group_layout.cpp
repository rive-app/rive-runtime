/*
 * Copyright 2026 Rive
 */

#include "rive/renderer/ore/ore_bind_group_layout.hpp"
#include "rive/renderer/ore/ore_binding_map.hpp"
#include "rive/renderer/ore/ore_context.hpp"
#include "rive/renderer/ore/ore_shader_module.hpp"

#include <sstream>
#include <vector>

namespace rive::ore
{

const BindGroupLayoutEntry* BindGroupLayout::findEntry(uint32_t binding) const
{
    for (const auto& e : m_entries)
    {
        if (e.binding == binding)
        {
            return &e;
        }
    }
    return nullptr;
}

bool BindGroupLayout::hasDynamicOffset(uint32_t binding) const
{
    const BindGroupLayoutEntry* e = findEntry(binding);
    return e != nullptr && e->kind == BindingKind::uniformBuffer &&
           e->hasDynamicOffset;
}

// Map binding-map types to layout-entry types.
static BindingKind bindingKindFromResource(ResourceKind k)
{
    switch (k)
    {
        case ResourceKind::UniformBuffer:
            return BindingKind::uniformBuffer;
        case ResourceKind::StorageBufferRO:
            return BindingKind::storageBufferRO;
        case ResourceKind::StorageBufferRW:
            return BindingKind::storageBufferRW;
        case ResourceKind::SampledTexture:
            return BindingKind::sampledTexture;
        case ResourceKind::StorageTexture:
            return BindingKind::storageTexture;
        case ResourceKind::Sampler:
            return BindingKind::sampler;
        case ResourceKind::ComparisonSampler:
            return BindingKind::comparisonSampler;
    }
    return BindingKind::uniformBuffer;
}

static TextureViewDimension viewDimFromBindingMap(TextureViewDim d)
{
    switch (d)
    {
        case TextureViewDim::Cube:
            return TextureViewDimension::cube;
        case TextureViewDim::CubeArray:
            return TextureViewDimension::cubeArray;
        case TextureViewDim::D3:
            return TextureViewDimension::texture3D;
        case TextureViewDim::D2Array:
            return TextureViewDimension::array2D;
        case TextureViewDim::D1:
        case TextureViewDim::D2:
        case TextureViewDim::Undefined:
            return TextureViewDimension::texture2D;
    }
    return TextureViewDimension::texture2D;
}

static BindGroupLayoutEntry::SampleType sampleTypeFromBindingMap(
    TextureSampleType s)
{
    switch (s)
    {
        case TextureSampleType::UnfilterableFloat:
            return BindGroupLayoutEntry::SampleType::floatUnfilterable;
        case TextureSampleType::Depth:
            return BindGroupLayoutEntry::SampleType::depth;
        case TextureSampleType::Sint:
            return BindGroupLayoutEntry::SampleType::sint;
        case TextureSampleType::Uint:
            return BindGroupLayoutEntry::SampleType::uint;
        case TextureSampleType::Float:
        case TextureSampleType::Undefined:
            return BindGroupLayoutEntry::SampleType::floatFilterable;
    }
    return BindGroupLayoutEntry::SampleType::floatFilterable;
}

uint32_t populateBindGroupLayoutEntriesFromShader(
    BindGroupLayoutEntry* entries,
    uint32_t maxEntries,
    const ShaderModule* shader,
    uint32_t groupIndex,
    const uint32_t* dynamicUBOBindings,
    uint32_t dynamicUBOCount)
{
    if (shader == nullptr)
        return 0;
    auto isDynamic = [&](uint32_t binding) -> bool {
        for (uint32_t i = 0; i < dynamicUBOCount; ++i)
            if (dynamicUBOBindings[i] == binding)
                return true;
        return false;
    };
    const BindingMap& bm = shader->m_bindingMap;
    uint32_t n = 0;
    for (size_t i = 0; i < bm.size(); ++i)
    {
        const BindingMap::Entry& e = bm.at(i);
        if (e.group != groupIndex)
            continue;
        // Keep counting past the buffer so the caller can size up instead
        // of quietly losing bindings.
        const uint32_t index = n++;
        if (index >= maxEntries)
            continue;
        BindGroupLayoutEntry& out = entries[index];
        out.binding = e.binding;
        out.kind = bindingKindFromResource(e.kind);
        // Mirror the shader's declared visibility — narrower than this
        // would be rejected by validateLayoutsAgainstBindingMap.
        uint8_t vis = 0;
        if (e.stageMask & BindingMap::kStageVertex)
            vis |= StageVisibility::kVertex;
        if (e.stageMask & BindingMap::kStageFragment)
            vis |= StageVisibility::kFragment;
        if (e.stageMask & BindingMap::kStageCompute)
            vis |= StageVisibility::kCompute;
        out.visibility.mask = vis;
        out.hasDynamicOffset =
            (out.kind == BindingKind::uniformBuffer && isDynamic(e.binding));
        // Texture reflection — required for validation to accept cube /
        // 3D / array textures that don't match the texture2D default.
        out.textureViewDim = viewDimFromBindingMap(e.textureViewDim);
        out.textureSampleType = sampleTypeFromBindingMap(e.textureSampleType);
        out.textureMultisampled = e.textureMultisampled;
        // Pre-resolve native slots from the shader's binding map.
        const uint16_t vs =
            e.backendSlot[static_cast<size_t>(BindingMap::Stage::VS)];
        const uint16_t fs =
            e.backendSlot[static_cast<size_t>(BindingMap::Stage::FS)];
        out.nativeSlotVS = (vs == BindingMap::kAbsent)
                               ? BindGroupLayoutEntry::kNativeSlotAbsent
                               : static_cast<uint32_t>(vs);
        out.nativeSlotFS = (fs == BindingMap::kAbsent)
                               ? BindGroupLayoutEntry::kNativeSlotAbsent
                               : static_cast<uint32_t>(fs);
    }
    return n;
}

rcp<BindGroupLayout> makeBindGroupLayoutFromShader(
    Context& ctx,
    const ShaderModule* shader,
    uint32_t groupIndex,
    const uint32_t* dynamicUBOBindings,
    uint32_t dynamicUBOCount)
{
    // The baked id never covers dynamic offsets, so those skip interning.
    const uint64_t layoutId =
        (shader != nullptr && dynamicUBOCount == 0)
            ? shader->m_bindingMap.layoutIdForGroup(groupIndex)
            : BindingMap::kNoLayoutId;
    if (layoutId != BindingMap::kNoLayoutId)
    {
        if (rcp<BindGroupLayout> hit =
                ctx.findInternedBindGroupLayout(layoutId))
            return hit;
    }

    static constexpr uint32_t kInlineEntries = 16;
    BindGroupLayoutEntry inlineEntries[kInlineEntries]{};
    const uint32_t n =
        populateBindGroupLayoutEntriesFromShader(inlineEntries,
                                                 kInlineEntries,
                                                 shader,
                                                 groupIndex,
                                                 dynamicUBOBindings,
                                                 dynamicUBOCount);

    // Wide groups spill to the heap. Layout creation is setup-time, so the
    // second walk is not worth a cap.
    std::vector<BindGroupLayoutEntry> spilled;
    const BindGroupLayoutEntry* entries = inlineEntries;
    if (n > kInlineEntries)
    {
        spilled.resize(n);
        populateBindGroupLayoutEntriesFromShader(spilled.data(),
                                                 n,
                                                 shader,
                                                 groupIndex,
                                                 dynamicUBOBindings,
                                                 dynamicUBOCount);
        entries = spilled.data();
    }

    BindGroupLayoutDesc desc;
    desc.groupIndex = groupIndex;
    desc.entries = entries;
    desc.entryCount = n;
    rcp<BindGroupLayout> layout = ctx.makeBindGroupLayout(desc);
    if (layout != nullptr && layoutId != BindingMap::kNoLayoutId)
        ctx.internBindGroupLayout(layoutId, layout);
    return layout;
}

// Map ore::BindingKind (public layout API) ↔ ore::ResourceKind (binding-map
// internal). Kept private to this TU.
static bool kindsMatch(BindingKind layoutKind, ResourceKind shaderKind)
{
    switch (layoutKind)
    {
        case BindingKind::uniformBuffer:
            return shaderKind == ResourceKind::UniformBuffer;
        case BindingKind::storageBufferRO:
            return shaderKind == ResourceKind::StorageBufferRO;
        case BindingKind::storageBufferRW:
            return shaderKind == ResourceKind::StorageBufferRW;
        case BindingKind::sampledTexture:
            return shaderKind == ResourceKind::SampledTexture;
        case BindingKind::storageTexture:
            return shaderKind == ResourceKind::StorageTexture;
        case BindingKind::sampler:
            // Sampler / ComparisonSampler are interchangeable on the
            // bind-API side — matches the BindingMap::lookup collapse
            // (ore_binding_map.hpp:201-208). Layout is allowed to declare
            // either; runtime treats them as one bind-time category.
            return shaderKind == ResourceKind::Sampler ||
                   shaderKind == ResourceKind::ComparisonSampler;
        case BindingKind::comparisonSampler:
            return shaderKind == ResourceKind::Sampler ||
                   shaderKind == ResourceKind::ComparisonSampler;
    }
    return false;
}

static const char* kindName(BindingKind k)
{
    switch (k)
    {
        case BindingKind::uniformBuffer:
            return "uniformBuffer";
        case BindingKind::storageBufferRO:
            return "storageBufferRO";
        case BindingKind::storageBufferRW:
            return "storageBufferRW";
        case BindingKind::sampledTexture:
            return "sampledTexture";
        case BindingKind::storageTexture:
            return "storageTexture";
        case BindingKind::sampler:
            return "sampler";
        case BindingKind::comparisonSampler:
            return "comparisonSampler";
    }
    return "?";
}

static const char* shaderKindName(ResourceKind k)
{
    switch (k)
    {
        case ResourceKind::UniformBuffer:
            return "uniformBuffer";
        case ResourceKind::StorageBufferRO:
            return "storageBufferRO";
        case ResourceKind::StorageBufferRW:
            return "storageBufferRW";
        case ResourceKind::SampledTexture:
            return "sampledTexture";
        case ResourceKind::StorageTexture:
            return "storageTexture";
        case ResourceKind::Sampler:
            return "sampler";
        case ResourceKind::ComparisonSampler:
            return "comparisonSampler";
    }
    return "?";
}

bool validateLayoutsAgainstBindingMap(const BindingMap& bindingMap,
                                      BindGroupLayout* const* layouts,
                                      uint32_t layoutCount,
                                      std::string* outError)
{
    auto fail = [&](const std::string& msg) {
        if (outError != nullptr)
        {
            *outError = msg;
        }
        return false;
    };

    // Every binding the shader references must have a corresponding layout
    // entry. Unused layout entries (declared but not referenced by the shader)
    // are allowed — Dawn permits this and it lets pipelines reuse a more
    // permissive layout than the shader strictly needs.
    for (size_t i = 0; i < bindingMap.size(); ++i)
    {
        const BindingMap::Entry& shaderEntry = bindingMap.at(i);
        const uint32_t group = shaderEntry.group;
        const uint32_t binding = shaderEntry.binding;

        if (group >= layoutCount || layouts == nullptr ||
            layouts[group] == nullptr)
        {
            std::ostringstream oss;
            oss << "@group(" << group << ") @binding(" << binding
                << "): shader declares " << shaderKindName(shaderEntry.kind)
                << " but PipelineDesc::bindGroupLayouts has no entry for "
                   "group "
                << group;
            return fail(oss.str());
        }

        const BindGroupLayout* layout = layouts[group];
        if (layout->groupIndex() != group)
        {
            std::ostringstream oss;
            oss << "PipelineDesc::bindGroupLayouts[" << group
                << "]->groupIndex == " << layout->groupIndex() << ", expected "
                << group
                << " (positional index must match layout's groupIndex)";
            return fail(oss.str());
        }

        const BindGroupLayoutEntry* layoutEntry = layout->findEntry(binding);
        if (layoutEntry == nullptr)
        {
            std::ostringstream oss;
            oss << "@group(" << group << ") @binding(" << binding
                << "): layout has no entry for this binding (shader expects "
                << shaderKindName(shaderEntry.kind) << ")";
            return fail(oss.str());
        }

        if (!kindsMatch(layoutEntry->kind, shaderEntry.kind))
        {
            std::ostringstream oss;
            oss << "@group(" << group << ") @binding(" << binding
                << "): layout declares " << kindName(layoutEntry->kind)
                << " but shader declares " << shaderKindName(shaderEntry.kind);
            return fail(oss.str());
        }

        // Visibility narrower than the shader's stageMask is rejected.
        // Layout broader than shader is fine (allowed by WebGPU spec).
        const uint8_t shaderStageMask = shaderEntry.stageMask;
        const uint8_t layoutVisibility = layoutEntry->visibility.mask;
        if ((shaderStageMask & ~layoutVisibility) != 0)
        {
            std::ostringstream oss;
            oss << "@group(" << group << ") @binding(" << binding
                << "): layout visibility 0x" << std::hex
                << static_cast<uint32_t>(layoutVisibility)
                << " missing stages required by shader (stageMask=0x"
                << static_cast<uint32_t>(shaderStageMask) << ")";
            return fail(oss.str());
        }

        // Texture dimension/sampleType compatibility (texture kinds only).
        if (layoutEntry->kind == BindingKind::sampledTexture ||
            layoutEntry->kind == BindingKind::storageTexture)
        {
            // Map TextureViewDim (binding-map) ↔ TextureViewDimension
            // (public layout API). The binding-map enum has D1/D2/D2Array/
            // Cube/CubeArray/D3; the public enum has texture2D/cube/
            // texture3D/array2D/cubeArray.
            auto dimsMatch = [](TextureViewDimension a, TextureViewDim b) {
                switch (a)
                {
                    case TextureViewDimension::texture2D:
                        return b == TextureViewDim::D2;
                    case TextureViewDimension::cube:
                        return b == TextureViewDim::Cube;
                    case TextureViewDimension::texture3D:
                        return b == TextureViewDim::D3;
                    case TextureViewDimension::array2D:
                        return b == TextureViewDim::D2Array;
                    case TextureViewDimension::cubeArray:
                        return b == TextureViewDim::CubeArray;
                }
                return false;
            };

            // Shader's textureViewDim is Undefined for non-texture kinds
            // and may also be Undefined for textures the shader compiler
            // didn't reflect a dim for. Skip the check when shader side
            // is Undefined.
            if (shaderEntry.textureViewDim != TextureViewDim::Undefined &&
                !dimsMatch(layoutEntry->textureViewDim,
                           shaderEntry.textureViewDim))
            {
                std::ostringstream oss;
                oss << "@group(" << group << ") @binding(" << binding
                    << "): texture view dimension mismatch";
                return fail(oss.str());
            }
        }
    }
    return true;
}

bool validateColorRequiresFragment(uint32_t colorCount,
                                   bool hasFragmentModule,
                                   std::string* outError)
{
    if (colorCount > 0 && !hasFragmentModule)
    {
        if (outError != nullptr)
            *outError = "pipeline declares color outputs but has no fragment "
                        "shader; supply `fragment`, or omit `colorTargets` "
                        "for a depth-only pipeline";
        return false;
    }
    return true;
}

} // namespace rive::ore
