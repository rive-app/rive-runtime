/*
 * Copyright 2026 Rive
 */
// Derived layouts hold only what a pipeline's entry points bind, as WebGPU's
// layout 'auto' does. The deferred context validates without a GPU.
#include "rive/renderer/ore/cmd/ore_deferred_context.hpp"
#include "rive/renderer/ore/ore_bind_group_layout.hpp"
#include "rive/renderer/ore/ore_draw_bindings.hpp"
#include "rive/renderer/ore/ore_rstb_entry_bindings.hpp"
#include "rive/renderer/ore/ore_shader_asset_modules.hpp"
#include <algorithm>
#include <catch.hpp>
#include <string>
#include <vector>

using namespace rive;
using namespace rive::ore;
using namespace rive::ore::cmd;

namespace
{
// What real bakes mark every row with.
constexpr uint8_t kAll = BindingMap::kStageVertex | BindingMap::kStageFragment |
                         BindingMap::kStageCompute;

BindingMap::Entry row(
    uint8_t group,
    uint8_t binding,
    ResourceKind kind,
    uint8_t stageMask = kAll)
{
  BindingMap::Entry e{};
  e.group = group;
  e.binding = binding;
  e.kind = kind;
  e.stageMask = stageMask;
  e.backendSlot[0] = binding;
  e.backendSlot[1] = binding;
  if (kind == ResourceKind::SampledTexture)
  {
    e.textureViewDim = TextureViewDim::D2;
    e.textureSampleType = TextureSampleType::Float;
  }
  if (kind == ResourceKind::UniformBuffer)
    e.minBindingSize = 16;
  return e;
}

RstbEntryBindings uses(
    uint8_t stage,
    const char* name,
    std::vector<uint16_t> bindings)
{
  RstbEntryBindings e;
  e.stage = stage;
  e.name = name;
  e.bindings = std::move(bindings);
  return e;
}

// The repro shader: a uniform at @binding(0) and a texture at @binding(1),
// one fragment entry point per use, and one using neither.
struct Shader : ShaderModule
{
  Shader(bool withUse = true)
  {
    m_bindingMap.push(row(0, 0, ResourceKind::UniformBuffer));
    m_bindingMap.push(row(0, 1, ResourceKind::SampledTexture));
    m_bindingMap.finalize();
    m_bindingMap.computeLayoutIds();
    if (withUse)
    {
      m_entryBindings = {
          uses(0, "vs_tri", {}),
          uses(1, "fs_flat", {}),
          uses(1, "fs_picture", {0x0001}),
          uses(1, "fs_tint", {0x0000}),
          uses(1, "fs_both", {0x0000, 0x0001})};
    }
  }
};

rcp<TextureView> textureView(Context& ctx)
{
  TextureDesc td;
  td.width = 4;
  td.height = 4;
  td.format = TextureFormat::rgba8unorm;
  rcp<Texture> tex = ctx.makeTexture(td);
  TextureViewDesc vd;
  vd.texture = tex.get();
  return ctx.makeTextureView(vd);
}

bool contains(const std::string& s, const char* part)
{
  return s.find(part) != std::string::npos;
}
} // namespace

TEST_CASE("entry bindings encode and decode", "[ore][entry_layout]")
{
  std::vector<RstbEntryBindings> sent = {
      uses(0, "vs_main", {}),
      uses(1, "fs_main", {0x0001, 0x0102})};
  std::vector<uint8_t> bytes = encodeEntryBindings(sent);
  std::vector<RstbEntryBindings> got;
  REQUIRE(decodeEntryBindings(bytes.data(), bytes.size(), got));
  REQUIRE(got.size() == 2);
  CHECK(got[0].name == "vs_main");
  CHECK(got[0].bindings.empty());
  CHECK(got[1].stage == 1);
  CHECK(got[1].uses(1, 2));
  CHECK(!got[1].uses(0, 0));

  // A truncated section reads as no per entry use, never as a partial one.
  CHECK(!decodeEntryBindings(bytes.data(), bytes.size() - 1, got));
  CHECK(got.empty());
  CHECK(!decodeEntryBindings(nullptr, 0, got));
}

TEST_CASE(
    "each entry's sampling pairs follow its bindings",
    "[ore][entry_layout]")
{
  std::vector<RstbEntryBindings> sent = {
      uses(0, "vs_main", {}),
      uses(1, "fs_main", {0x0001, 0x0002})};
  const std::vector<uint8_t> bindingsOnly = encodeEntryBindings(sent);
  sent[1].pairs = {{0, 1, 0, 2}};
  for (RstbEntryBindings& e : sent)
    e.pairsKnown = true;
  std::vector<uint8_t> bytes = encodeEntryBindings(sent);
  std::vector<RstbEntryBindings> got;
  REQUIRE(decodeEntryBindings(bytes.data(), bytes.size(), got));
  CHECK(got[0].pairsKnown);
  CHECK(got[0].pairs.empty());
  REQUIRE(got[1].pairs.size() == 1);
  CHECK(got[1].pairs[0] == TextureSamplerPair{0, 1, 0, 2});

  // Older readers stop where a bake without pairs ends.
  REQUIRE(bytes.size() > bindingsOnly.size());
  CHECK(std::equal(bindingsOnly.begin(), bindingsOnly.end(), bytes.begin()));
  REQUIRE(decodeEntryBindings(bindingsOnly.data(), bindingsOnly.size(), got));
  CHECK(!got[1].pairsKnown);

  // A cut pair block keeps the bindings and leaves every entry's pairs
  // unknown.
  REQUIRE(decodeEntryBindings(bytes.data(), bytes.size() - 1, got));
  CHECK(got[1].uses(0, 2));
  CHECK(!got[0].pairsKnown);
  CHECK(!got[1].pairsKnown);

  // Pairs go out for every entry or none.
  sent[0].pairsKnown = false;
  CHECK(encodeEntryBindings(sent) == bindingsOnly);

  // Pairs that would overflow the section's u16 length are left out.
  std::vector<RstbEntryBindings> wide(128, uses(1, "e", {}));
  for (RstbEntryBindings& e : wide)
  {
    e.bindings.assign(250, 0x0001);
    e.pairs.assign(2, {0, 1, 0, 2});
    e.pairsKnown = true;
  }
  std::vector<uint8_t> wideBytes = encodeEntryBindings(wide);
  REQUIRE(wideBytes.size() <= 0xFFFF);
  REQUIRE(decodeEntryBindings(wideBytes.data(), wideBytes.size(), got));
  CHECK(got.size() == 128);
  CHECK(!got[0].pairsKnown);
}

struct TestPipeline : Pipeline
{
  explicit TestPipeline(const PipelineDesc& desc) : Pipeline(desc) {}
};

TEST_CASE(
    "narrowing keeps the rows the entry points use",
    "[ore][entry_layout]")
{
  Shader shader;
  BindingMap narrowed = bindingMapForEntries(
      &shader,
      "vs_tri",
      &shader,
      "fs_picture");
  REQUIRE(narrowed.size() == 1);
  CHECK(narrowed.at(0).binding == 1);
  CHECK(narrowed.at(0).stageMask == kAll);
  // Its own identity: it must not intern as the whole module's layout.
  CHECK(narrowed.layoutIdForGroup(0) != BindingMap::kNoLayoutId);
  CHECK(
      narrowed.layoutIdForGroup(0) != shader.m_bindingMap.layoutIdForGroup(0));

  // Another pipeline binding the same agrees on it.
  Shader other;
  CHECK(
      bindingMapForEntries(&other, "vs_tri", &other, "fs_picture")
          .layoutIdForGroup(0) == narrowed.layoutIdForGroup(0));

  // Nothing bound leaves no group at all.
  BindingMap flat = bindingMapForEntries(&shader, "vs_tri", &shader, "fs_flat");
  CHECK(flat.empty());
  CHECK(flat.groupLayoutCount() == 0);
}

TEST_CASE(
    "a narrowing that keeps every row keeps the module's layout",
    "[ore][entry_layout]")
{
  Shader shader;
  BindingMap both = bindingMapForEntries(&shader, "vs_tri", &shader, "fs_both");
  CHECK(both.size() == 2);
  CHECK(both.layoutIdForGroup(0) == shader.m_bindingMap.layoutIdForGroup(0));

  // So it shares one layout with the whole shader's.
  DeferredOreContext ctx(nullptr);
  CHECK(
      makeBindGroupLayoutFromBindingMap(ctx, both, 0, nullptr, 0, &shader)
          .get() == makeBindGroupLayoutFromShader(ctx, &shader, 0).get());
}

TEST_CASE("narrowing follows each stage's own module", "[ore][entry_layout]")
{
  // The vertex stage's file binds a uniform; the fragment's file a texture.
  struct Vertex : ShaderModule
  {
    Vertex()
    {
      m_bindingMap.push(
          row(0, 0, ResourceKind::UniformBuffer, BindingMap::kStageVertex));
      m_bindingMap.finalize();
      m_entryBindings = {uses(0, "vs_main", {0x0000})};
    }
  } vertex;
  struct Fragment : ShaderModule
  {
    Fragment()
    {
      m_bindingMap.push(
          row(0, 1, ResourceKind::SampledTexture, BindingMap::kStageFragment));
      m_bindingMap.finalize();
      m_entryBindings = {uses(1, "fs_flat", {}), uses(1, "fs_main", {0x0001})};
    }
  } fragment;

  BindingMap used = bindingMapForEntries(
      &vertex,
      "vs_main",
      &fragment,
      "fs_main");
  REQUIRE(used.size() == 2);
  BindingMap flat = bindingMapForEntries(
      &vertex,
      "vs_main",
      &fragment,
      "fs_flat");
  REQUIRE(flat.size() == 1);
  CHECK(flat.at(0).binding == 0);

  // A uniform the vertex file declares but never uses gives way to the
  // texture the fragment file binds there.
  struct Unused : ShaderModule
  {
    Unused()
    {
      m_bindingMap.push(
          row(0, 1, ResourceKind::UniformBuffer, BindingMap::kStageVertex));
      m_bindingMap.finalize();
      m_entryBindings = {uses(0, "vs_main", {})};
    }
  } unused;
  BindingMap texture = bindingMapForEntries(
      &unused,
      "vs_main",
      &fragment,
      "fs_main");
  REQUIRE(texture.size() == 1);
  CHECK(texture.at(0).kind == ResourceKind::SampledTexture);

  DeferredOreContext ctx(nullptr);
  auto layout = makeBindGroupLayoutFromBindingMap(ctx, texture, 0);
  BindGroupLayout* layouts[] = {layout.get()};
  PipelineDesc desc;
  desc.vertexModule = &unused;
  desc.vertexEntryPoint = "vs_main";
  desc.fragmentModule = &fragment;
  desc.fragmentEntryPoint = "fs_main";
  desc.bindGroupLayouts = layouts;
  desc.bindGroupLayoutCount = 1;
  std::string error;
  CHECK(validatePipelineDesc(desc, texture, NativeSlotScope::perStage, &error));
  CHECK(error.empty());
}

TEST_CASE(
    "a sampler narrowed to its depth texture is non-filtering",
    "[ore][entry_layout]")
{
  // One sampler reads a depth texture in fs_depth and a float one in
  // fs_color, so only the whole module must keep it filtering.
  struct Pairs : ShaderModule
  {
    Pairs()
    {
      BindingMap::Entry depth = row(0, 0, ResourceKind::SampledTexture);
      depth.textureSampleType = TextureSampleType::Depth;
      m_bindingMap.push(depth);
      m_bindingMap.push(row(0, 1, ResourceKind::SampledTexture));
      m_bindingMap.push(row(0, 2, ResourceKind::Sampler));
      m_bindingMap.finalize();
      m_bindingMap.computeLayoutIds();
      m_textureSamplerPairs = {{0, 0, 0, 2}, {0, 1, 0, 2}};
      m_entryBindings = {
          uses(0, "vs_main", {}),
          uses(1, "fs_depth", {0x0000, 0x0002}),
          uses(1, "fs_color", {0x0001, 0x0002})};
    }
  } shader;
  DeferredOreContext ctx(nullptr);
  auto samplerOf = [&](const BindingMap& map) {
    auto layout = makeBindGroupLayoutFromBindingMap(
        ctx,
        map,
        0,
        nullptr,
        0,
        &shader);
    return layout->findEntry(2)->samplerNonFiltering;
  };
  CHECK(
      samplerOf(bindingMapForEntries(&shader, "vs_main", &shader, "fs_depth")));
  CHECK(!samplerOf(
      bindingMapForEntries(&shader, "vs_main", &shader, "fs_color")));
  CHECK(!samplerOf(shader.m_bindingMap));
}

TEST_CASE(
    "narrowing leaves older bakes and unknown entries whole",
    "[ore][entry_layout]")
{
  Shader old(false);
  CHECK(bindingMapForEntries(&old, "vs_tri", &old, "fs_flat").size() == 2);

  Shader shader;
  BindingMap unknown = bindingMapForEntries(
      &shader,
      "vs_tri",
      &shader,
      "fs_missing");
  CHECK(unknown.size() == 2);
  CHECK(unknown.layoutIdForGroup(0) == shader.m_bindingMap.layoutIdForGroup(0));

  // A module compiled for one entry point answers for it by any name, as
  // GL compiles every entry point as main.
  Shader single;
  single.m_entryBindings = {uses(1, "fs_picture", {0x0001})};
  CHECK(single.entryBindings("main") == &single.m_entryBindings[0]);
}

TEST_CASE(
    "a derived layout binds only what the entry points bind",
    "[ore][entry_layout]")
{
  Shader shader;
  DeferredOreContext ctx(nullptr);
  auto layout = makeBindGroupLayoutFromBindingMap(
      ctx,
      bindingMapForEntries(&shader, "vs_tri", &shader, "fs_picture"),
      0);
  REQUIRE(layout != nullptr);
  CHECK(layout->findEntry(0) == nullptr);
  REQUIRE(layout->findEntry(1) != nullptr);

  // The repro's group, only the texture fs_picture reads, is accepted.
  auto view = textureView(ctx);
  BindGroupDesc::TexEntry tex{1, view.get()};
  BindGroupDesc desc;
  desc.layout = layout.get();
  desc.textures = &tex;
  desc.textureCount = 1;
  CHECK(ctx.makeBindGroup(desc) != nullptr);

  // Against the whole module it is still refused, as WebGPU would.
  auto whole = makeBindGroupLayoutFromShader(ctx, &shader, 0);
  desc.layout = whole.get();
  ctx.clearLastError();
  CHECK(ctx.makeBindGroup(desc) == nullptr);
  CHECK(contains(ctx.lastError(), "leaves this uniform buffer empty"));

  // Equal narrowings share one interned layout.
  auto again = makeBindGroupLayoutFromBindingMap(
      ctx,
      bindingMapForEntries(&shader, "vs_tri", &shader, "fs_picture"),
      0);
  CHECK(again.get() == layout.get());
}

TEST_CASE(
    "a pipeline needs layouts only for what its entry points bind",
    "[ore][entry_layout]")
{
  Shader shader;
  DeferredOreContext ctx(nullptr);
  auto validate = [&](const char* fragmentEntry,
                      BindGroupLayout* layout) -> std::string {
    BindGroupLayout* layouts[] = {layout};
    PipelineDesc desc;
    desc.vertexModule = &shader;
    desc.vertexEntryPoint = "vs_tri";
    desc.fragmentModule = &shader;
    desc.fragmentEntryPoint = fragmentEntry;
    desc.bindGroupLayouts = layout != nullptr ? layouts : nullptr;
    desc.bindGroupLayoutCount = layout != nullptr ? 1 : 0;
    std::string error;
    validatePipelineDesc(
        desc,
        bindingMapForEntries(&shader, "vs_tri", &shader, fragmentEntry),
        NativeSlotScope::perStage,
        &error);
    return error;
  };
  auto picture = makeBindGroupLayoutFromBindingMap(
      ctx,
      bindingMapForEntries(&shader, "vs_tri", &shader, "fs_picture"),
      0);
  auto whole = makeBindGroupLayoutFromShader(ctx, &shader, 0);

  CHECK(validate("fs_flat", nullptr).empty());
  CHECK(validate("fs_picture", picture.get()).empty());
  // A layout covering more than the entry points bind is fine.
  CHECK(validate("fs_picture", whole.get()).empty());
  // One missing what an entry point binds is not.
  CHECK(contains(
      validate("fs_tint", picture.get()),
      "layout has no entry for this binding"));
  CHECK(!validate("fs_picture", nullptr).empty());
}

TEST_CASE(
    "each module carries its entries under their physical names",
    "[ore][entry_layout]")
{
  std::vector<RstbEntryBindings> asset = {
      uses(0, "vertex", {0x0000}),
      uses(1, "fs_main", {0x0001, 0x0002})};
  asset[1].pairs = {{0, 1, 0, 2}};
  for (RstbEntryBindings& e : asset)
    e.pairsKnown = true;
  std::vector<RstbEntryView> entries(2);
  entries[0].stage = 0;
  entries[0].logical = "vertex";
  entries[0].physical = "vertex0";
  entries[1].stage = 1;
  entries[1].logical = "fs_main";
  entries[1].physical = "fs_main";

  std::vector<uint8_t> bytes = moduleEntryBindingBytes(asset, entries, 0, 2);
  std::vector<RstbEntryBindings> got;
  REQUIRE(decodeEntryBindings(bytes.data(), bytes.size(), got));
  REQUIRE(got.size() == 2);
  CHECK(got[0].name == "vertex0");
  CHECK(got[0].uses(0, 0));

  // A per entry module carries only its own, pairs included.
  bytes = moduleEntryBindingBytes(asset, entries, 1, 1);
  REQUIRE(decodeEntryBindings(bytes.data(), bytes.size(), got));
  REQUIRE(got.size() == 1);
  CHECK(got[0].name == "fs_main");
  CHECK(got[0].pairsKnown);
  CHECK(got[0].pairs == asset[1].pairs);

  // An entry the section does not name leaves the module whole.
  entries[1].logical = "fs_other";
  CHECK(moduleEntryBindingBytes(asset, entries, 0, 2).empty());
}

TEST_CASE(
    "a draw needs bound groups matching the pipeline's layouts",
    "[ore][entry_layout]")
{
  Shader shader;
  DeferredOreContext ctx(nullptr);
  auto picture = makeBindGroupLayoutFromBindingMap(
      ctx,
      bindingMapForEntries(&shader, "vs_tri", &shader, "fs_picture"),
      0);
  BindGroupLayout* layouts[] = {picture.get()};
  PipelineDesc desc;
  desc.vertexModule = &shader;
  desc.vertexEntryPoint = "vs_tri";
  desc.fragmentModule = &shader;
  desc.fragmentEntryPoint = "fs_picture";
  desc.bindGroupLayouts = layouts;
  desc.bindGroupLayoutCount = 1;
  auto pipeline = make_rcp<TestPipeline>(desc);

  auto view = textureView(ctx);
  BindGroupDesc::TexEntry tex{1, view.get()};
  BufferDesc bufferDesc{BufferUsage::uniform, 16};
  auto buffer = ctx.makeBuffer(bufferDesc);
  BindGroupDesc::UBOEntry ubo{0, buffer.get(), 0, 16};
  auto groupOf = [&](BindGroupLayout* layout) {
    BindGroupDesc group;
    group.layout = layout;
    group.textures = &tex;
    group.textureCount = 1;
    if (layout->findEntry(0) != nullptr)
    {
      group.ubos = &ubo;
      group.uboCount = 1;
    }
    return ctx.makeBindGroup(group);
  };

  DrawBindings bindings;
  bindings.setPipeline(pipeline.get());
  std::string error;
  CHECK(!bindings.validate(&error));
  CHECK(contains(error, "@group(0): the pipeline binds this group"));

  // A group built for the whole module's layout is refused, as in WebGPU.
  auto whole = makeBindGroupLayoutFromShader(ctx, &shader, 0);
  bindings.setBindGroup(0, groupOf(whole.get()).get());
  CHECK(!bindings.validate(&error));
  CHECK(contains(error, "does not match the pipeline's"));

  bindings.setBindGroup(0, groupOf(picture.get()).get());
  CHECK(bindings.validate(&error));

  // A layout built apart with equal entries matches.
  BindGroupLayoutDesc copy;
  copy.groupIndex = 0;
  copy.entries = picture->entries().data();
  copy.entryCount = static_cast<uint32_t>(picture->entries().size());
  auto twin = ctx.makeBindGroupLayout(copy);
  REQUIRE(twin.get() != picture.get());
  bindings.setBindGroup(0, groupOf(twin.get()).get());
  CHECK(bindings.validate(&error));

  // A pipeline taking no layout for the group needs none bound.
  PipelineDesc flat = desc;
  flat.bindGroupLayouts = nullptr;
  flat.bindGroupLayoutCount = 0;
  DrawBindings none;
  none.setPipeline(make_rcp<TestPipeline>(flat).get());
  CHECK(none.validate(&error));

  // An older bake's whole file layout may list a group its entry points
  // never read, so leaving it unset still draws.
  Shader old(false);
  auto oldWhole = makeBindGroupLayoutFromShader(ctx, &old, 0);
  BindGroupLayout* oldLayouts[] = {oldWhole.get()};
  PipelineDesc oldDesc = desc;
  oldDesc.vertexModule = &old;
  oldDesc.fragmentModule = &old;
  oldDesc.fragmentEntryPoint = "fs_flat";
  oldDesc.bindGroupLayouts = oldLayouts;
  DrawBindings older;
  older.setPipeline(make_rcp<TestPipeline>(oldDesc).get());
  CHECK(older.validate(&error));
  older.setBindGroup(0, groupOf(picture.get()).get());
  CHECK(!older.validate(&error));
}

TEST_CASE(
    "split files are checked for slot clashes over what gets bound",
    "[ore][entry_layout]")
{
  // Each file numbers its slots from zero, so the vertex file's unused u
  // and the fragment file's v both sit on slot 1.
  auto file =
      [](uint8_t stageMask, uint8_t secondBinding, std::vector<uint16_t> used) {
        struct File : ShaderModule
        {};
        auto* module = new File();
        const size_t stage = stageMask == BindingMap::kStageVertex ? 0 : 1;
        for (uint8_t binding : {uint8_t(0), secondBinding})
        {
          BindingMap::Entry e = row(
              0,
              binding,
              ResourceKind::UniformBuffer,
              stageMask);
          e.backendSlot[0] = e.backendSlot[1] = BindingMap::kAbsent;
          e.backendSlot[stage] = binding == 0 ? 0 : 1;
          module->m_bindingMap.push(e);
        }
        module->m_bindingMap.finalize();
        module->m_entryBindings = {
            uses(static_cast<uint8_t>(stage), "main", std::move(used))};
        return rcp<ShaderModule>(module);
      };
  auto vertex = file(BindingMap::kStageVertex, 1, {0x0000});
  auto fragment = file(BindingMap::kStageFragment, 2, {0x0000, 0x0002});

  DeferredOreContext ctx(nullptr);
  auto validate = [&](const BindingMap& map) {
    auto layout = makeBindGroupLayoutFromBindingMap(ctx, map, 0);
    BindGroupLayout* layouts[] = {layout.get()};
    PipelineDesc desc;
    desc.vertexModule = vertex.get();
    desc.vertexEntryPoint = "main";
    desc.fragmentModule = fragment.get();
    desc.fragmentEntryPoint = "main";
    desc.bindGroupLayouts = layouts;
    desc.bindGroupLayoutCount = 1;
    std::string error;
    validatePipelineDesc(
        desc,
        bindingMapForEntries(vertex.get(), "main", fragment.get(), "main"),
        NativeSlotScope::perKind,
        &error);
    return error;
  };
  CHECK(validate(
            bindingMapForEntries(vertex.get(), "main", fragment.get(), "main"))
            .empty());
  // A wider explicit layout binds u too, so it clashes with v.
  CHECK(contains(
      validate(bindingMapForStages(vertex.get(), fragment.get())),
      "both land on native slot 1"));
}

TEST_CASE(
    "a texture is judged sampled by the pipeline's own entry points",
    "[ore][entry_layout]")
{
  // fs_sample samples t with s. fs_load only loads t, and samples u with s.
  struct Sampling : ShaderModule
  {
    Sampling(bool pairsKnown)
    {
      m_bindingMap.push(row(0, 0, ResourceKind::SampledTexture));
      m_bindingMap.push(row(0, 1, ResourceKind::SampledTexture));
      m_bindingMap.push(row(0, 2, ResourceKind::Sampler));
      m_bindingMap.finalize();
      m_bindingMap.computeLayoutIds();
      m_textureSamplerPairs = {{0, 0, 0, 2}, {0, 1, 0, 2}};
      m_entryBindings = {
          uses(0, "vs", {}),
          uses(1, "fs_sample", {0x0000, 0x0002}),
          uses(1, "fs_load", {0x0000, 0x0001, 0x0002})};
      m_entryBindings[1].pairs = {{0, 0, 0, 2}};
      m_entryBindings[2].pairs = {{0, 1, 0, 2}};
      for (RstbEntryBindings& e : m_entryBindings)
        e.pairsKnown = pairsKnown;
    }
  };
  using SampleType = BindGroupLayoutEntry::SampleType;
  DeferredOreContext ctx(nullptr);
  auto textureType = [&](const Sampling& shader, const char* fragment) {
    auto layout = makeBindGroupLayoutFromBindingMap(
        ctx,
        bindingMapForEntries(&shader, "vs", &shader, fragment),
        0,
        nullptr,
        0,
        &shader,
        &shader,
        "vs",
        fragment);
    return layout->findEntry(0)->textureSampleType;
  };
  Sampling shader(true);
  CHECK(textureType(shader, "fs_sample") == SampleType::floatFilterable);
  CHECK(textureType(shader, "fs_load") == SampleType::floatUnfilterable);
  // Older bakes only know the file's pairs.
  CHECK(textureType(Sampling(false), "fs_load") == SampleType::floatFilterable);

  // GL binds s only to the texture the entry point samples with it.
  PipelineDesc desc;
  desc.vertexModule = &shader;
  desc.vertexEntryPoint = "vs";
  desc.fragmentModule = &shader;
  desc.fragmentEntryPoint = "fs_load";
  auto pipeline = make_rcp<TestPipeline>(desc);
  REQUIRE(pipeline->m_textureSamplerPairs.size() == 1);
  CHECK(pipeline->m_textureSamplerPairs[0] == TextureSamplerPair{0, 1, 0, 2});

  // A layout declaring t unfilterable only refuses the entry that samples t.
  BindGroupLayoutEntry entries[3]{};
  populateBindGroupLayoutEntries(entries, 3, shader.m_bindingMap, 0);
  entries[0].textureSampleType = SampleType::floatUnfilterable;
  BindGroupLayoutDesc layoutDesc;
  layoutDesc.entries = entries;
  layoutDesc.entryCount = 3;
  auto layout = ctx.makeBindGroupLayout(layoutDesc);
  BindGroupLayout* layouts[] = {layout.get()};
  desc.bindGroupLayouts = layouts;
  desc.bindGroupLayoutCount = 1;
  auto validate = [&](const char* fragment) {
    desc.fragmentEntryPoint = fragment;
    std::string error;
    validatePipelineDesc(desc, BindingMap(), NativeSlotScope::perStage, &error);
    return error;
  };
  CHECK(validate("fs_load").empty());
  CHECK(contains(
      validate("fs_sample"),
      "the layout declares this texture unfilterable"));
}
