/*
 * Copyright 2026 Rive
 */

#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_bind_group_layout.hpp"
#include "rive/renderer/ore/ore_binding_map.hpp"
#include "rive/renderer/ore/ore_context.hpp"
#include "rive/renderer/ore/ore_sampler.hpp"
#include "rive/renderer/ore/ore_script_guards.hpp"
#include "rive/renderer/ore/ore_shader_module.hpp"
#include "rive/renderer/ore/ore_texture.hpp"

#include <algorithm>
#include <iterator>
#include <cstdio>
#include <string>
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

// Per-entry backends hand each stage its own ShaderModule even for a
// combined file, and those two parse the same sidecar. Layout ids are
// content hashes, so matching ids mean the merge would rebuild the map it
// started from — and drop the ids that keep the layout interned.
static bool sameBakedLayout(const BindingMap& a, const BindingMap& b)
{
  if (a.groupLayoutCount() == 0 || a.groupLayoutCount() != b.groupLayoutCount())
  {
    return false;
  }
  for (size_t i = 0; i < a.groupLayoutCount(); ++i)
  {
    const BindingMap::GroupLayout& ga = a.groupLayoutAt(i);
    const BindingMap::GroupLayout& gb = b.groupLayoutAt(i);
    if (ga.group != gb.group || ga.layoutId != gb.layoutId ||
        ga.layoutId == BindingMap::kNoLayoutId)
    {
      return false;
    }
  }
  return true;
}

BindingMap bindingMapForStages(
    const ShaderModule* vertex,
    const ShaderModule* fragment)
{
  if (vertex == nullptr)
    return fragment != nullptr ? fragment->m_bindingMap : BindingMap();
  if (fragment == nullptr || fragment == vertex ||
      sameBakedLayout(vertex->m_bindingMap, fragment->m_bindingMap))
  {
    return vertex->m_bindingMap;
  }
  BindingMap merged = vertex->m_bindingMap;
  merged.replaceStage(fragment->m_bindingMap, BindingMap::Stage::FS);
  return merged;
}

bool entryUseKnown(
    const ShaderModule* vertex,
    const char* vertexEntry,
    const ShaderModule* fragment,
    const char* fragmentEntry)
{
  return vertex != nullptr && vertex->entryBindings(vertexEntry) != nullptr &&
         (fragment == nullptr ||
          fragment->entryBindings(fragmentEntry) != nullptr);
}

// `map` narrowed to what the named entry points bind; unchanged unless both
// stages' modules record it.
static BindingMap narrowToEntries(
    BindingMap map,
    const ShaderModule* vertex,
    const char* vertexEntry,
    const ShaderModule* fragment,
    const char* fragmentEntry)
{
  if (!entryUseKnown(vertex, vertexEntry, fragment, fragmentEntry))
    return map;
  const RstbEntryBindings* vertexUse = vertex->entryBindings(vertexEntry);
  const RstbEntryBindings* fragmentUse = fragment != nullptr
                                             ? fragment->entryBindings(
                                                   fragmentEntry)
                                             : nullptr;
  map.narrow([&](const BindingMap::Entry& e) {
    return vertexUse->uses(e.group, e.binding) ||
           (fragmentUse != nullptr && fragmentUse->uses(e.group, e.binding));
  });
  return map;
}

// `module`'s own map narrowed to what `entry` binds, whole for older bakes.
static BindingMap narrowedToEntry(const ShaderModule* module, const char* entry)
{
  BindingMap map = module->m_bindingMap;
  if (const RstbEntryBindings* use = module->entryBindings(entry))
  {
    map.narrow([&](const BindingMap::Entry& e) {
      return use->uses(e.group, e.binding);
    });
  }
  return map;
}

static bool stagesFromOneFile(
    const ShaderModule* vertex,
    const ShaderModule* fragment)
{
  return vertex == nullptr || fragment == nullptr || vertex == fragment ||
         sameBakedLayout(vertex->m_bindingMap, fragment->m_bindingMap);
}

BindingMap bindingMapForEntries(
    const ShaderModule* vertex,
    const char* vertexEntry,
    const ShaderModule* fragment,
    const char* fragmentEntry)
{
  if (stagesFromOneFile(vertex, fragment))
  {
    return narrowToEntries(
        bindingMapForStages(vertex, fragment),
        vertex,
        vertexEntry,
        fragment,
        fragmentEntry);
  }
  // Narrowed before merging, so a binding one file declares but never uses
  // cannot stand in for what the other file binds there.
  BindingMap merged = narrowedToEntry(vertex, vertexEntry);
  merged.replaceStage(
      narrowedToEntry(fragment, fragmentEntry),
      BindingMap::Stage::FS);
  return merged;
}

uint32_t populateBindGroupLayoutEntries(
    BindGroupLayoutEntry* entries,
    uint32_t maxEntries,
    const BindingMap& bm,
    uint32_t groupIndex,
    const uint32_t* dynamicUBOBindings,
    uint32_t dynamicUBOCount)
{
  auto isDynamic = [&](uint32_t binding) -> bool {
    for (uint32_t i = 0; i < dynamicUBOCount; ++i)
      if (dynamicUBOBindings[i] == binding)
        return true;
    return false;
  };
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
    out.minBindingSize = e.minBindingSize;
    // Pre-resolve native slots from the shader's binding map.
    const uint16_t
        vs = e.backendSlot[static_cast<size_t>(BindingMap::Stage::VS)];
    const uint16_t
        fs = e.backendSlot[static_cast<size_t>(BindingMap::Stage::FS)];
    out.nativeSlotVS = (vs == BindingMap::kAbsent)
                           ? BindGroupLayoutEntry::kNativeSlotAbsent
                           : static_cast<uint32_t>(vs);
    out.nativeSlotFS = (fs == BindingMap::kAbsent)
                           ? BindGroupLayoutEntry::kNativeSlotAbsent
                           : static_cast<uint32_t>(fs);
  }
  return n;
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
  return populateBindGroupLayoutEntries(
      entries,
      maxEntries,
      shader->m_bindingMap,
      groupIndex,
      dynamicUBOBindings,
      dynamicUBOCount);
}

// The baked id cannot see what the module pairings inferred, so each inferred
// binding salts it and layouts that differ only there intern apart.
static constexpr uint64_t kNonFilteringLayoutIdSalt = 0x9e3779b97f4a7c15ull;
static constexpr uint64_t kUnfilterableLayoutIdSalt = 0xc2b2ae3d27d4eb4full;

static uint64_t saltLayoutId(
    uint64_t layoutId,
    const std::vector<uint32_t>& bindings,
    uint64_t salt)
{
  for (uint32_t binding : bindings)
  {
    layoutId = (layoutId ^ (salt + binding)) * 0x100000001b3ull;
  }
  return layoutId;
}

rcp<BindGroupLayout> makeBindGroupLayoutFromBindingMap(
    Context& ctx,
    const BindingMap& bindingMap,
    uint32_t groupIndex,
    const uint32_t* dynamicUBOBindings,
    uint32_t dynamicUBOCount,
    const ShaderModule* vertexPairSource,
    const ShaderModule* fragmentPairSource,
    const char* vertexEntry,
    const char* fragmentEntry)
{
  // Gathered once for both the intern id and the entry flags.
  const std::vector<TextureSamplerPair>*
      vertexPairs = vertexPairSource != nullptr
                        ? &vertexPairSource->samplerPairs(vertexEntry)
                        : nullptr;
  const std::vector<TextureSamplerPair>*
      fragmentPairs = fragmentPairSource != nullptr
                          ? &fragmentPairSource->samplerPairs(fragmentEntry)
                          : nullptr;
  const std::vector<uint32_t> nonFiltering = depthOnlySamplerBindings(
      bindingMap,
      vertexPairs,
      fragmentPairs,
      groupIndex);
  const std::vector<uint32_t> unfilterable = unsampledFloatTextureBindings(
      bindingMap,
      vertexPairs,
      fragmentPairs,
      groupIndex);

  // The baked id never covers dynamic offsets, so those skip interning.
  // A map merged across two modules carries no ids either, so split-stage
  // pipelines build their layouts fresh.
  uint64_t layoutId = dynamicUBOCount == 0
                          ? bindingMap.layoutIdForGroup(groupIndex)
                          : BindingMap::kNoLayoutId;
  if (layoutId != BindingMap::kNoLayoutId)
  {
    layoutId = saltLayoutId(layoutId, nonFiltering, kNonFilteringLayoutIdSalt);
    layoutId = saltLayoutId(layoutId, unfilterable, kUnfilterableLayoutIdSalt);
  }
  if (layoutId != BindingMap::kNoLayoutId)
  {
    if (rcp<BindGroupLayout> hit = ctx.findInternedBindGroupLayout(layoutId))
      return hit;
  }

  static constexpr uint32_t kInlineEntries = 16;
  BindGroupLayoutEntry inlineEntries[kInlineEntries]{};
  const uint32_t n = populateBindGroupLayoutEntries(
      inlineEntries,
      kInlineEntries,
      bindingMap,
      groupIndex,
      dynamicUBOBindings,
      dynamicUBOCount);

  // Wide groups spill to the heap. Layout creation is setup-time, so the
  // second walk is not worth a cap.
  std::vector<BindGroupLayoutEntry> spilled;
  BindGroupLayoutEntry* entries = inlineEntries;
  if (n > kInlineEntries)
  {
    spilled.resize(n);
    populateBindGroupLayoutEntries(
        spilled.data(),
        n,
        bindingMap,
        groupIndex,
        dynamicUBOBindings,
        dynamicUBOCount);
    entries = spilled.data();
  }

  auto flagged = [](const std::vector<uint32_t>& bindings, uint32_t b) {
    return std::find(bindings.begin(), bindings.end(), b) != bindings.end();
  };
  for (uint32_t i = 0; i < n; ++i)
  {
    BindGroupLayoutEntry& e = entries[i];
    e.samplerNonFiltering = e.kind == BindingKind::sampler &&
                            flagged(nonFiltering, e.binding);
    if (e.kind == BindingKind::sampledTexture &&
        flagged(unfilterable, e.binding))
      e.textureSampleType = BindGroupLayoutEntry::SampleType::floatUnfilterable;
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

rcp<BindGroupLayout> makeBindGroupLayoutFromShader(
    Context& ctx,
    const ShaderModule* shader,
    uint32_t groupIndex,
    const uint32_t* dynamicUBOBindings,
    uint32_t dynamicUBOCount)
{
  static const BindingMap kEmpty;
  return makeBindGroupLayoutFromBindingMap(
      ctx,
      shader != nullptr ? shader->m_bindingMap : kEmpty,
      groupIndex,
      dynamicUBOBindings,
      dynamicUBOCount,
      shader,
      nullptr);
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

static std::string bindingTag(uint32_t group, uint32_t binding)
{
  return "@group(" + std::to_string(group) + ") @binding(" +
         std::to_string(binding) + ")";
}

static std::string hexString(uint32_t value)
{
  char hex[9];
  snprintf(hex, sizeof(hex), "%x", value);
  return hex;
}

bool validateLayoutsAgainstBindingMap(
    const BindingMap& bindingMap,
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

    if (group >= layoutCount || layouts == nullptr || layouts[group] == nullptr)
    {
      return fail(
          bindingTag(group, binding) + ": shader declares " +
          shaderKindName(shaderEntry.kind) +
          " but PipelineDesc::bindGroupLayouts has no entry for "
          "group " +
          std::to_string(group));
    }

    const BindGroupLayout* layout = layouts[group];
    if (layout->groupIndex() != group)
    {
      return fail(
          "PipelineDesc::bindGroupLayouts[" + std::to_string(group) +
          "]->groupIndex == " + std::to_string(layout->groupIndex()) +
          ", expected " + std::to_string(group) +
          " (positional index must match layout's groupIndex)");
    }

    const BindGroupLayoutEntry* layoutEntry = layout->findEntry(binding);
    if (layoutEntry == nullptr)
    {
      return fail(
          bindingTag(group, binding) +
          ": layout has no entry for this binding (shader "
          "expects " +
          shaderKindName(shaderEntry.kind) + ")");
    }

    if (!kindsMatch(layoutEntry->kind, shaderEntry.kind))
    {
      return fail(
          bindingTag(group, binding) + ": layout declares " +
          kindName(layoutEntry->kind) + " but shader declares " +
          shaderKindName(shaderEntry.kind));
    }

    // Visibility narrower than the shader's stageMask is rejected.
    // Layout broader than shader is fine (allowed by WebGPU spec).
    const uint8_t shaderStageMask = shaderEntry.stageMask;
    const uint8_t layoutVisibility = layoutEntry->visibility.mask;
    if ((shaderStageMask & ~layoutVisibility) != 0)
    {
      return fail(
          bindingTag(group, binding) + ": layout visibility 0x" +
          hexString(layoutVisibility) +
          " missing stages required by shader (stageMask=0x" +
          hexString(shaderStageMask) + ")");
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
          !dimsMatch(layoutEntry->textureViewDim, shaderEntry.textureViewDim))
      {
        return fail(
            bindingTag(group, binding) + ": texture view dimension mismatch");
      }
    }
  }
  return true;
}

bool validateLayoutSlots(
    const BindingMap& bindingMap,
    BindGroupLayout* const* layouts,
    uint32_t layoutCount,
    std::string* outError)
{
  constexpr uint32_t kAbsent = BindGroupLayoutEntry::kNativeSlotAbsent;
  for (size_t i = 0; i < bindingMap.size(); ++i)
  {
    const BindingMap::Entry& e = bindingMap.at(i);
    if (e.group >= layoutCount || layouts[e.group] == nullptr)
      continue;
    const BindGroupLayoutEntry* le = layouts[e.group]->findEntry(e.binding);
    if (le == nullptr)
      continue;
    const uint32_t layoutSlots[] = {le->nativeSlotVS, le->nativeSlotFS};
    for (size_t stage = 0; stage < 2; ++stage)
    {
      const uint16_t shaderSlot = e.backendSlot[stage];
      const uint32_t layoutSlot = layoutSlots[stage];
      // A layout without slots falls back to the binding or is refused at
      // makeBindGroup, so only a slot it names can disagree.
      if (shaderSlot == BindingMap::kAbsent || layoutSlot == kAbsent ||
          layoutSlot == shaderSlot)
        continue;
      if (outError != nullptr)
        *outError = bindingTag(e.group, e.binding) +
                    ": the layout binds it at native slot " +
                    std::to_string(layoutSlot) +
                    " but this pipeline's shader reads it from slot " +
                    std::to_string(shaderSlot) +
                    ". The layout was built from a shader that numbers its "
                    "slots differently; build it from this pipeline's own "
                    "shader";
      return false;
    }
  }
  return true;
}

SlotSpace slotSpace(BindingKind kind)
{
  switch (kind)
  {
    case BindingKind::uniformBuffer:
    case BindingKind::storageBufferRO:
    case BindingKind::storageBufferRW:
      return SlotSpace::buffer;
    case BindingKind::sampledTexture:
    case BindingKind::storageTexture:
      return SlotSpace::texture;
    case BindingKind::sampler:
    case BindingKind::comparisonSampler:
      return SlotSpace::sampler;
  }
  return SlotSpace::none;
}

SlotSpace slotSpace(ResourceKind kind)
{
  return slotSpace(bindingKindFromResource(kind));
}

namespace
{
struct SlotUse
{
  uint32_t group;
  uint32_t binding;
  SlotSpace space;
  uint32_t vs;
  uint32_t fs;
};

bool checkSplitStageUses(
    const std::vector<SlotUse>& uses,
    NativeSlotScope scope,
    std::string* outError)
{
  struct Claim
  {
    uint32_t scopeKey;
    uint32_t slot;
    uint8_t group;
    uint8_t binding;
  };
  std::vector<Claim> claimed;
  constexpr uint32_t kAbsent = BindGroupLayoutEntry::kNativeSlotAbsent;

  for (const SlotUse& use : uses)
  {
    // A binding both stages read, numbered differently by each file.
    // Only one number survives, so the other stage's compiled source reads
    // whatever sits at the number it baked.
    if (use.vs != kAbsent && use.fs != kAbsent && use.vs != use.fs)
    {
      if (outError != nullptr)
        *outError = bindingTag(use.group, use.binding) +
                    ": the vertex module put it on native slot " +
                    std::to_string(use.vs) + " and the fragment module on " +
                    std::to_string(use.fs) +
                    ", and this backend shares one slot namespace between the "
                    "stages. Declare the same bindings in both files, or "
                    "compile both stages from one shader";
      return false;
    }
    // Stage-sharing backends bind with the vertex slot when there is one.
    const uint32_t slot = use.vs != kAbsent ? use.vs : use.fs;
    if (slot == kAbsent)
      continue;
    const uint32_t scopeKey = scope == NativeSlotScope::perGroup
                                  ? use.group
                                  : static_cast<uint32_t>(use.space);
    for (const Claim& c : claimed)
    {
      if (c.scopeKey != scopeKey || c.slot != slot)
        continue;
      if (outError != nullptr)
        *outError = bindingTag(use.group, use.binding) + " and " +
                    bindingTag(c.group, c.binding) +
                    " both land on native slot " + std::to_string(slot) +
                    ": the vertex and fragment modules were compiled apart, "
                    "and this backend shares one slot namespace between them. "
                    "Declare the same bindings in both, or compile both stages "
                    "from one shader";
      return false;
    }
    claimed.push_back(
        {scopeKey,
         slot,
         static_cast<uint8_t>(use.group),
         static_cast<uint8_t>(use.binding)});
  }
  return true;
}
} // namespace

bool validateSplitStageSlots(
    bool stagesCompiledApart,
    BindGroupLayout* const* layouts,
    uint32_t layoutCount,
    NativeSlotScope scope,
    std::string* outError)
{
  if (!stagesCompiledApart || scope == NativeSlotScope::perStage)
    return true;
  // The layouts, not the pipeline's map, since backends binding layout slots
  // bind every entry they list.
  std::vector<SlotUse> uses;
  for (uint32_t l = 0; l < layoutCount; ++l)
  {
    if (layouts[l] == nullptr)
      continue;
    for (const BindGroupLayoutEntry& e : layouts[l]->entries())
    {
      uses.push_back(
          {layouts[l]->groupIndex(),
           e.binding,
           slotSpace(e.kind),
           e.nativeSlotVS,
           e.nativeSlotFS});
    }
  }
  return checkSplitStageUses(uses, scope, outError);
}

bool validateSplitStageSlots(
    bool stagesCompiledApart,
    const BindingMap& mergedMap,
    NativeSlotScope scope,
    std::string* outError)
{
  if (!stagesCompiledApart || scope == NativeSlotScope::perStage)
    return true;
  auto widen = [](uint16_t slot) -> uint32_t {
    return slot == BindingMap::kAbsent ? BindGroupLayoutEntry::kNativeSlotAbsent
                                       : slot;
  };
  std::vector<SlotUse> uses;
  uses.reserve(mergedMap.size());
  for (size_t i = 0; i < mergedMap.size(); ++i)
  {
    const BindingMap::Entry& e = mergedMap.at(i);
    uses.push_back(
        {e.group,
         e.binding,
         slotSpace(e.kind),
         widen(e.backendSlot[static_cast<size_t>(BindingMap::Stage::VS)]),
         widen(e.backendSlot[static_cast<size_t>(BindingMap::Stage::FS)])});
  }
  return checkSplitStageUses(uses, scope, outError);
}

bool validateColorRequiresFragment(
    uint32_t colorCount,
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

bool validateStagesAgree(
    const BindingMap& vertexMap,
    const BindingMap& fragmentMap,
    std::string* outError)
{
  auto fail = [&](const BindingMap::Entry& e, const char* what) {
    if (outError != nullptr)
      *outError =
          bindingTag(e.group, e.binding) +
          ": the vertex and fragment files declare it with a different " +
          what +
          ". A pipeline carries one declaration per binding, so the "
          "stage that loses reads what the other one bound";
    return false;
  };

  for (size_t f = 0; f < fragmentMap.size(); ++f)
  {
    const BindingMap::Entry& fs = fragmentMap.at(f);
    for (size_t v = 0; v < vertexMap.size(); ++v)
    {
      const BindingMap::Entry& vs = vertexMap.at(v);
      if (vs.group != fs.group || vs.binding != fs.binding)
        continue;
      // Sampler and comparison sampler are one category to every
      // caller, matching `BindingMap::lookup`.
      const bool bothSamplers = (vs.kind == ResourceKind::Sampler ||
                                 vs.kind == ResourceKind::ComparisonSampler) &&
                                (fs.kind == ResourceKind::Sampler ||
                                 fs.kind == ResourceKind::ComparisonSampler);
      if (vs.kind != fs.kind && !bothSamplers)
        return fail(fs, "kind");
      if (vs.textureViewDim != TextureViewDim::Undefined &&
          fs.textureViewDim != TextureViewDim::Undefined &&
          vs.textureViewDim != fs.textureViewDim)
      {
        return fail(fs, "texture dimension");
      }
      if (vs.textureSampleType != TextureSampleType::Undefined &&
          fs.textureSampleType != TextureSampleType::Undefined &&
          vs.textureSampleType != fs.textureSampleType)
      {
        return fail(fs, "texture sample type");
      }
      if (vs.minBindingSize != 0 && fs.minBindingSize != 0 &&
          vs.minBindingSize != fs.minBindingSize)
      {
        return fail(fs, "uniform block size");
      }
      break;
    }
  }
  return true;
}

bool validateStageLimits(
    BindGroupLayout* const* layouts,
    uint32_t layoutCount,
    std::string* outError)
{
  struct Limit
  {
    const char* what;
    uint32_t max;
  };
  static constexpr Limit kLimits[] = {
      {"uniform buffers", 12},
      {"storage buffers", 8},
      {"sampled textures", 16},
      {"storage textures", 4},
      {"samplers", 16},
  };
  auto limitIndex = [](BindingKind kind) {
    switch (kind)
    {
      case BindingKind::uniformBuffer:
        return 0;
      case BindingKind::storageBufferRO:
      case BindingKind::storageBufferRW:
        return 1;
      case BindingKind::sampledTexture:
        return 2;
      case BindingKind::storageTexture:
        return 3;
      case BindingKind::sampler:
      case BindingKind::comparisonSampler:
        return 4;
    }
    return 0;
  };
  constexpr uint32_t kMaxDynamicUniformBuffers = 8;
  char message[192];
  auto fail = [&]() {
    if (outError != nullptr)
      *outError = message;
    return false;
  };

  uint32_t dynamicUniformBuffers = 0;
  for (uint32_t g = 0; g < layoutCount && layouts != nullptr; ++g)
  {
    if (layouts[g] == nullptr)
      continue;
    for (const BindGroupLayoutEntry& e : layouts[g]->entries())
      dynamicUniformBuffers += e.hasDynamicOffset ? 1 : 0;
  }
  if (dynamicUniformBuffers > kMaxDynamicUniformBuffers)
  {
    snprintf(
        message,
        sizeof(message),
        "the layouts declare %u dynamic uniform buffers, and WebGPU "
        "allows %u per pipeline",
        dynamicUniformBuffers,
        kMaxDynamicUniformBuffers);
    return fail();
  }

  for (uint8_t stage : {StageVisibility::kVertex, StageVisibility::kFragment})
  {
    uint32_t counts[std::size(kLimits)] = {};
    for (uint32_t g = 0; g < layoutCount && layouts != nullptr; ++g)
    {
      if (layouts[g] == nullptr)
        continue;
      for (const BindGroupLayoutEntry& e : layouts[g]->entries())
      {
        if (e.visibility.mask & stage)
          counts[limitIndex(e.kind)]++;
      }
    }
    for (size_t i = 0; i < std::size(kLimits); ++i)
    {
      if (counts[i] <= kLimits[i].max)
        continue;
      snprintf(
          message,
          sizeof(message),
          "the layouts give the %s stage %u %s, and WebGPU allows "
          "%u per stage",
          stage == StageVisibility::kVertex ? "vertex" : "fragment",
          counts[i],
          kLimits[i].what,
          kLimits[i].max);
      return fail();
    }
  }
  return true;
}

std::vector<TextureSamplerPair> pipelineSamplerPairs(const PipelineDesc& desc)
{
  std::vector<TextureSamplerPair> pairs;
  const std::pair<const ShaderModule*, const char*> stages[] = {
      {desc.vertexModule, desc.vertexEntryPoint},
      {desc.fragmentModule, desc.fragmentEntryPoint}};
  for (const auto& [module, entry] : stages)
  {
    if (module == nullptr)
      continue;
    const std::vector<TextureSamplerPair>& stagePairs = module->samplerPairs(
        entry);
    pairs.insert(pairs.end(), stagePairs.begin(), stagePairs.end());
  }
  return pairs;
}

// Only a layout derived elsewhere can trip this: a texture its source never
// samples is unfilterable, and WebGPU refuses one sampled with filtering.
static bool validateUnfilterableSampling(
    const PipelineDesc& desc,
    std::string* outError)
{
  auto entryFor = [&](uint32_t group,
                      uint32_t binding) -> const BindGroupLayoutEntry* {
    if (desc.bindGroupLayouts == nullptr ||
        group >= desc.bindGroupLayoutCount ||
        desc.bindGroupLayouts[group] == nullptr)
      return nullptr;
    return desc.bindGroupLayouts[group]->findEntry(binding);
  };
  for (const TextureSamplerPair& p : pipelineSamplerPairs(desc))
  {
    const BindGroupLayoutEntry* tex = entryFor(
        p.textureGroup,
        p.textureBinding);
    const BindGroupLayoutEntry* sampler = entryFor(
        p.samplerGroup,
        p.samplerBinding);
    if (tex == nullptr || sampler == nullptr ||
        sampler->kind != BindingKind::sampler || sampler->samplerNonFiltering ||
        tex->textureSampleType !=
            BindGroupLayoutEntry::SampleType::floatUnfilterable)
      continue;
    char message[224];
    snprintf(
        message,
        sizeof(message),
        "@group(%u) @binding(%u): the layout declares this "
        "texture unfilterable, but the shader samples it with "
        "filtering sampler @binding(%u), which WebGPU refuses; "
        "derive the layout from this pipeline's entry points",
        p.textureGroup,
        p.textureBinding,
        p.samplerBinding);
    if (outError != nullptr)
      *outError = message;
    return false;
  }
  return true;
}

bool validatePipelineDesc(
    const PipelineDesc& desc,
    const BindingMap& mergedMap,
    NativeSlotScope scope,
    LayoutSlots layoutSlots,
    std::string* outError)
{
  const bool stagesCompiledApart = !stagesFromOneFile(
      desc.vertexModule,
      desc.fragmentModule);
  // Ahead of the layout check: a disagreement here means the merged map
  // itself is wrong, and a layout complaint would only describe the
  // symptom.
  if (stagesCompiledApart &&
      !validateStagesAgree(
          narrowedToEntry(desc.vertexModule, desc.vertexEntryPoint),
          narrowedToEntry(desc.fragmentModule, desc.fragmentEntryPoint),
          outError))
  {
    return false;
  }
  return validateLayoutsAgainstBindingMap(
             mergedMap,
             desc.bindGroupLayouts,
             desc.bindGroupLayoutCount,
             outError) &&
         (layoutSlots == LayoutSlots::ignored || validateLayoutSlots(
                                                     mergedMap,
                                                     desc.bindGroupLayouts,
                                                     desc.bindGroupLayoutCount,
                                                     outError)) &&
         validateStageLimits(
             desc.bindGroupLayouts,
             desc.bindGroupLayoutCount,
             outError) &&
         validateUnfilterableSampling(desc, outError) &&
         validateColorRequiresFragment(
             desc.colorCount,
             desc.fragmentModule != nullptr,
             outError) &&
         (layoutSlots == LayoutSlots::bound ? validateSplitStageSlots(
                                                  stagesCompiledApart,
                                                  desc.bindGroupLayouts,
                                                  desc.bindGroupLayoutCount,
                                                  scope,
                                                  outError)
                                            : validateSplitStageSlots(
                                                  stagesCompiledApart,
                                                  mergedMap,
                                                  scope,
                                                  outError));
}

} // namespace rive::ore

namespace rive::ore
{

namespace
{
using SampleType = BindGroupLayoutEntry::SampleType;

// No WebGPU host of ours requests float32-filterable, so 32 bit float formats
// never filter there.
bool viewSupportsSampleType(const TextureView& view, SampleType type)
{
  switch (view.texture()->format())
  {
    case TextureFormat::depth24plusStencil8:
    case TextureFormat::depth32floatStencil8:
      if (view.aspect() == TextureAspect::stencilOnly)
        return type == SampleType::uint;
      return type == SampleType::depth || type == SampleType::floatUnfilterable;
    case TextureFormat::depth16unorm:
    case TextureFormat::depth32float:
      return type == SampleType::depth || type == SampleType::floatUnfilterable;
    case TextureFormat::rgba32float:
    case TextureFormat::rg32float:
    case TextureFormat::r32float:
      return type == SampleType::floatUnfilterable;
    default:
      return type == SampleType::floatFilterable ||
             type == SampleType::floatUnfilterable;
  }
}

const char* sampleTypeName(SampleType type)
{
  switch (type)
  {
    case SampleType::floatFilterable:
      return "float";
    case SampleType::floatUnfilterable:
      return "unfilterable float";
    case SampleType::depth:
      return "depth";
    case SampleType::sint:
      return "sint";
    case SampleType::uint:
      return "uint";
  }
  return "?";
}

const char* viewDimensionName(TextureViewDimension dimension)
{
  switch (dimension)
  {
    case TextureViewDimension::texture2D:
      return "2d";
    case TextureViewDimension::cube:
      return "cube";
    case TextureViewDimension::texture3D:
      return "3d";
    case TextureViewDimension::array2D:
      return "2d array";
    case TextureViewDimension::cubeArray:
      return "cube array";
  }
  return "?";
}

bool isBufferKind(BindingKind kind)
{
  return kind == BindingKind::uniformBuffer ||
         kind == BindingKind::storageBufferRO ||
         kind == BindingKind::storageBufferRW;
}

bool isTextureKind(BindingKind kind)
{
  return kind == BindingKind::sampledTexture ||
         kind == BindingKind::storageTexture;
}

bool isSamplerKind(BindingKind kind)
{
  return kind == BindingKind::sampler || kind == BindingKind::comparisonSampler;
}

const char* bindingNoun(BindingKind kind)
{
  switch (kind)
  {
    case BindingKind::uniformBuffer:
      return "uniform buffer";
    case BindingKind::storageBufferRO:
    case BindingKind::storageBufferRW:
      return "storage buffer";
    case BindingKind::sampledTexture:
      return "texture";
    case BindingKind::storageTexture:
      return "storage texture";
    case BindingKind::sampler:
      return "sampler";
    case BindingKind::comparisonSampler:
      return "comparison sampler";
  }
  return "binding";
}

template <typename Entry>
uint32_t countSlot(const Entry* entries, uint32_t count, uint32_t slot)
{
  uint32_t n = 0;
  for (uint32_t i = 0; i < count; ++i)
    n += entries[i].slot == slot ? 1 : 0;
  return n;
}

constexpr uint32_t kWebGpuUniformOffsetAlignment = 256;
} // namespace

// snprintf, not a stream: the wasm module lane links this too, and libc++
// stream formatting drags locale imports the host does not provide.
bool validateBindGroupDesc(const BindGroupDesc& desc, std::string* outError)
{
  // A null layout is the backend's own error to name.
  if (desc.layout == nullptr)
    return true;
  const uint32_t group = desc.layout->groupIndex();
  char message[256];
  auto fail = [&]() {
    if (outError != nullptr)
      *outError = message;
    return false;
  };
  auto failEmpty = [&](uint32_t slot, const char* noun) {
    snprintf(
        message,
        sizeof(message),
        "@group(%u) @binding(%u): binds no %s",
        group,
        slot,
        noun);
    return fail();
  };

  for (const BindGroupLayoutEntry& e : desc.layout->entries())
  {
    const uint32_t
        n = isBufferKind(e.kind)
                ? countSlot(desc.ubos, desc.uboCount, e.binding)
            : isTextureKind(e.kind)
                ? countSlot(desc.textures, desc.textureCount, e.binding)
                : countSlot(desc.samplers, desc.samplerCount, e.binding);
    if (n != 1)
    {
      snprintf(
          message,
          sizeof(message),
          n == 0 ? "@group(%u) @binding(%u): the bind group leaves "
                   "this %s empty, and WebGPU requires every entry "
                   "of the layout"
                 : "@group(%u) @binding(%u): the bind group binds "
                   "this %s more than once",
          group,
          e.binding,
          bindingNoun(e.kind));
      return fail();
    }
  }

  for (uint32_t i = 0; i < desc.uboCount; ++i)
  {
    const BindGroupDesc::UBOEntry& ubo = desc.ubos[i];
    const BindGroupLayoutEntry* entry = desc.layout->findEntry(ubo.slot);
    if (entry == nullptr || entry->kind != BindingKind::uniformBuffer)
    {
      snprintf(
          message,
          sizeof(message),
          "@group(%u) @binding(%u): the layout has no uniform "
          "buffer here",
          group,
          ubo.slot);
      return fail();
    }
    if (ubo.buffer == nullptr)
      return failEmpty(ubo.slot, "buffer");
    if (ubo.buffer->usage() != BufferUsage::uniform)
    {
      snprintf(
          message,
          sizeof(message),
          "@group(%u) @binding(%u): binds a buffer made without "
          "uniform usage",
          group,
          ubo.slot);
      return fail();
    }
    if (ubo.offset % kWebGpuUniformOffsetAlignment != 0)
    {
      snprintf(
          message,
          sizeof(message),
          "@group(%u) @binding(%u): offset %u is not a multiple of "
          "%u, which WebGPU requires of uniform buffers",
          group,
          ubo.slot,
          ubo.offset,
          kWebGpuUniformOffsetAlignment);
      return fail();
    }
    const uint64_t bufferSize = ubo.buffer->size();
    if (ubo.offset > bufferSize || uint64_t(ubo.offset) + ubo.size > bufferSize)
    {
      snprintf(
          message,
          sizeof(message),
          "@group(%u) @binding(%u): offset %u + size %u exceeds "
          "the %llu byte buffer",
          group,
          ubo.slot,
          ubo.offset,
          ubo.size,
          static_cast<unsigned long long>(bufferSize));
      return fail();
    }
    const uint64_t bound = ubo.size != 0 ? ubo.size : bufferSize - ubo.offset;
    if (bound < entry->minBindingSize)
    {
      snprintf(
          message,
          sizeof(message),
          "@group(%u) @binding(%u): binds %llu bytes but the "
          "shader's uniform block needs %u",
          group,
          ubo.slot,
          static_cast<unsigned long long>(bound),
          entry->minBindingSize);
      return fail();
    }
  }

  for (uint32_t i = 0; i < desc.textureCount; ++i)
  {
    const BindGroupDesc::TexEntry& tex = desc.textures[i];
    const BindGroupLayoutEntry* entry = desc.layout->findEntry(tex.slot);
    if (entry == nullptr || !isTextureKind(entry->kind))
    {
      snprintf(
          message,
          sizeof(message),
          "@group(%u) @binding(%u): the layout has no texture here",
          group,
          tex.slot);
      return fail();
    }
    if (tex.view == nullptr)
      return failEmpty(tex.slot, "texture view");
    // Wrapped canvas views carry no texture to check.
    if (entry->kind != BindingKind::sampledTexture ||
        tex.view->texture() == nullptr)
      continue;
    const TextureView& view = *tex.view;
    if (view.dimension() != entry->textureViewDim)
    {
      snprintf(
          message,
          sizeof(message),
          "@group(%u) @binding(%u): binds a %s view where the "
          "shader declares a %s texture",
          group,
          tex.slot,
          viewDimensionName(view.dimension()),
          viewDimensionName(entry->textureViewDim));
      return fail();
    }
    if ((view.texture()->sampleCount() > 1) != entry->textureMultisampled)
    {
      snprintf(
          message,
          sizeof(message),
          entry->textureMultisampled
              ? "@group(%u) @binding(%u): the shader declares a "
                "multisampled texture but the view has 1 sample"
              : "@group(%u) @binding(%u): binds a multisampled "
                "view where the shader declares a single sampled "
                "texture",
          group,
          tex.slot);
      return fail();
    }
    const TextureFormat format = view.texture()->format();
    if ((format == TextureFormat::depth24plusStencil8 ||
         format == TextureFormat::depth32floatStencil8) &&
        view.aspect() == TextureAspect::all)
    {
      snprintf(
          message,
          sizeof(message),
          "@group(%u) @binding(%u): the view selects both depth "
          "and stencil, and WebGPU binds one aspect; make it depth "
          "only",
          group,
          tex.slot);
      return fail();
    }
    if (!viewSupportsSampleType(view, entry->textureSampleType))
    {
      snprintf(
          message,
          sizeof(message),
          "@group(%u) @binding(%u): WebGPU cannot read this "
          "view's format as a %s texture",
          group,
          tex.slot,
          sampleTypeName(entry->textureSampleType));
      return fail();
    }
  }

  for (uint32_t i = 0; i < desc.samplerCount; ++i)
  {
    const BindGroupDesc::SampEntry& samp = desc.samplers[i];
    const BindGroupLayoutEntry* entry = desc.layout->findEntry(samp.slot);
    if (entry == nullptr || !isSamplerKind(entry->kind))
    {
      snprintf(
          message,
          sizeof(message),
          "@group(%u) @binding(%u): the layout has no sampler here",
          group,
          samp.slot);
      return fail();
    }
    if (samp.sampler == nullptr)
      return failEmpty(samp.slot, "sampler");
    if (!samp.sampler->described())
      continue;
    const bool wantsComparison = entry->kind == BindingKind::comparisonSampler;
    if (samp.sampler->comparison() != wantsComparison)
    {
      snprintf(
          message,
          sizeof(message),
          wantsComparison ? "@group(%u) @binding(%u): the shader declares a "
                            "sampler_comparison but the sampler has no compare "
                            "function"
                          : "@group(%u) @binding(%u): binds a comparison "
                            "sampler where the shader declares a plain sampler",
          group,
          samp.slot);
      return fail();
    }
    if (entry->samplerNonFiltering && samp.sampler->filtering())
    {
      snprintf(
          message,
          sizeof(message),
          "@group(%u) @binding(%u): this sampler only reads depth "
          "textures, so WebGPU needs it non-filtering; give it "
          "nearest filters",
          group,
          samp.slot);
      return fail();
    }
  }
  return true;
}

void BindGroup::recordDynamicRanges(const BindGroupDesc& desc)
{
  m_dynamicRanges.clear();
  if (desc.layout == nullptr)
    return;
  for (uint32_t i = 0; i < desc.uboCount; ++i)
  {
    const BindGroupDesc::UBOEntry& ubo = desc.ubos[i];
    if (ubo.buffer != nullptr && desc.layout->hasDynamicOffset(ubo.slot))
    {
      const uint64_t bufferSize = ubo.buffer->size();
      // Size 0 spans the rest of the buffer.
      const uint64_t size = ubo.size != 0             ? ubo.size
                            : bufferSize > ubo.offset ? bufferSize - ubo.offset
                                                      : 0;
      m_dynamicRanges.push_back(
          {ubo.slot, ubo.offset, (uint32_t)size, bufferSize});
    }
  }
  std::sort(
      m_dynamicRanges.begin(),
      m_dynamicRanges.end(),
      [](const DynamicUBORange& a, const DynamicUBORange& b) {
        return a.slot < b.slot;
      });
}

bool validateSetBindGroup(
    uint32_t groupIndex,
    const BindGroup* group,
    const uint32_t* offsets,
    uint32_t offsetCount,
    uint32_t alignment,
    std::string* outError)
{
  char message[192];
  auto fail = [&]() {
    if (outError != nullptr)
      *outError = message;
    return false;
  };
  if (groupIndex >= kMaxBindGroups)
  {
    snprintf(
        message,
        sizeof(message),
        "setBindGroup: groupIndex must be in [0, %u) (got %u)",
        kMaxBindGroups,
        groupIndex);
    return fail();
  }
  if (offsetCount > kMaxDynamicOffsets)
  {
    snprintf(
        message,
        sizeof(message),
        kGuardDynamicOffsetCountFormat,
        offsetCount,
        kMaxDynamicOffsets);
    return fail();
  }
  if (group == nullptr)
    return true;
  if (group->layout() != nullptr && group->groupIndex() != groupIndex)
  {
    snprintf(
        message,
        sizeof(message),
        "setBindGroup: BindGroup was made for group %u, not %u",
        group->groupIndex(),
        groupIndex);
    return fail();
  }
  if (offsetCount != group->dynamicOffsetCount())
  {
    snprintf(
        message,
        sizeof(message),
        "setBindGroup: dynamicOffsets count %u does not match the "
        "BindGroup's declared dynamic UBO count %u",
        offsetCount,
        group->dynamicOffsetCount());
    return fail();
  }
  // A group missing a dynamic UBO its layout declares cannot pair ranges
  // with offsets, so only the alignment is checked then.
  const auto& ranges = group->dynamicRanges();
  const bool paired = ranges.size() == offsetCount;
  for (uint32_t i = 0; i < offsetCount; ++i)
  {
    if (alignment != 0 && offsets[i] % alignment != 0)
    {
      snprintf(
          message,
          sizeof(message),
          "setBindGroup: dynamicOffsets[%u] = %u is not a "
          "multiple of %u (alignment requirement)",
          i,
          offsets[i],
          alignment);
      return fail();
    }
    if (paired)
    {
      const auto& range = ranges[i];
      if (uint64_t(offsets[i]) + range.offset + range.size > range.bufferSize)
      {
        snprintf(
            message,
            sizeof(message),
            "setBindGroup: dynamicOffsets[%u] = %u moves "
            "@binding(%u) past the end of its %llu byte buffer",
            i,
            offsets[i],
            range.slot,
            static_cast<unsigned long long>(range.bufferSize));
        return fail();
      }
    }
  }
  return true;
}

uint32_t scriptDynamicOffsetAlignment(const Context* context)
{
  return context != nullptr && context->featuresKnown()
             ? context->features().minUniformBufferOffsetAlignment
             : Features{}.minUniformBufferOffsetAlignment;
}

static bool scriptGuardFailed(const char* message, std::string* outError)
{
  if (outError != nullptr)
  {
    *outError = message;
  }
  return false;
}

bool validateSampleCount(
    const Context* context,
    uint32_t sampleCount,
    std::string* outError)
{
  char message[192];
  if (sampleCount > 1 && (sampleCount & (sampleCount - 1)) != 0)
  {
    snprintf(
        message,
        sizeof(message),
        "sampleCount must be a power of two (got %u)",
        sampleCount);
    return scriptGuardFailed(message, outError);
  }
  if (sampleCount > 1 && context != nullptr && context->featuresKnown() &&
      sampleCount > context->features().maxSamples)
  {
    snprintf(
        message,
        sizeof(message),
        "sampleCount %u exceeds device maximum of %u; query "
        "features().maxSamples before creating MSAA textures",
        sampleCount,
        context->features().maxSamples);
    return scriptGuardFailed(message, outError);
  }
  return true;
}

bool validateTextureFormat(
    const Context* context,
    TextureFormat format,
    std::string* outError)
{
  if (textureFormatIsBC(format) && context != nullptr &&
      context->featuresKnown() && !context->features().bc)
  {
    return scriptGuardFailed(
        "GPUTexture.new: BC formats require the bc "
        "feature, which the active backend does not "
        "support",
        outError);
  }
  return true;
}

bool validateSamplerDesc(
    const Context* context,
    const SamplerDesc& desc,
    std::string* outError)
{
  char message[192];
  if (desc.minLod > desc.maxLod)
  {
    snprintf(
        message,
        sizeof(message),
        "GPUSampler.new: minLod (%g) > maxLod (%g)",
        desc.minLod,
        desc.maxLod);
    return scriptGuardFailed(message, outError);
  }
  // Every backend caps anisotropy at 16 and takes only powers of two.
  const uint32_t a = desc.maxAnisotropy;
  if (a < 1 || a > 16 || (a & (a - 1)) != 0)
  {
    snprintf(
        message,
        sizeof(message),
        "GPUSampler.new: maxAnisotropy must be a power of two in "
        "[1, 16] (got %u)",
        a);
    return scriptGuardFailed(message, outError);
  }
  // WebGPU refuses anisotropy unless every filter is linear.
  if (a > 1 &&
      (desc.minFilter != Filter::linear || desc.magFilter != Filter::linear ||
       desc.mipmapFilter != Filter::linear))
  {
    snprintf(
        message,
        sizeof(message),
        "GPUSampler.new: maxAnisotropy=%u needs linear min, mag and "
        "mipmap filters",
        a);
    return scriptGuardFailed(message, outError);
  }
  if (a > 1 && context != nullptr && context->featuresKnown() &&
      !context->features().anisotropicFiltering)
  {
    snprintf(
        message,
        sizeof(message),
        "GPUSampler.new: maxAnisotropy=%u requires "
        "anisotropicFiltering feature, which the active backend does "
        "not support",
        a);
    return scriptGuardFailed(message, outError);
  }
  return true;
}

bool validateVertexLayout(
    const Context* context,
    const PipelineDesc& desc,
    std::string* outError)
{
  char message[192];
  if (desc.vertexBufferCount > kMaxVertexBufferSlots)
  {
    snprintf(
        message,
        sizeof(message),
        kGuardVertexLayoutCountFormat,
        desc.vertexBufferCount,
        kMaxVertexBufferSlots);
    return scriptGuardFailed(message, outError);
  }
  // A module cannot see the device; its host checks the slots against it.
#ifndef RIVE_WASM_MODULE
  // Backends index a fixed attribute table by slot, so an unknown device
  // still gets the portable count rather than no check.
  const uint32_t maxAttributes = context != nullptr && context->featuresKnown()
                                     ? context->features().maxVertexAttributes
                                     : Features{}.maxVertexAttributes;
  for (uint32_t b = 0; b < desc.vertexBufferCount; ++b)
  {
    const VertexBufferLayout& buffer = desc.vertexBuffers[b];
    for (uint32_t i = 0; i < buffer.attributeCount; ++i)
    {
      if (buffer.attributes[i].shaderSlot >= maxAttributes)
      {
        snprintf(
            message,
            sizeof(message),
            "GPUPipeline.new: attribute slot must be 0-%u (got "
            "%u)",
            maxAttributes - 1,
            buffer.attributes[i].shaderSlot);
        return scriptGuardFailed(message, outError);
      }
    }
  }
#endif
  return true;
}

} // namespace rive::ore
