/*
 * Copyright 2026 Rive
 */
// The compute pipeline and pass types, checked without a GPU: what a device
// must accept before a backend builds anything.
#include "rive/renderer/ore/cmd/ore_command_silver.hpp"
#include "rive/renderer/ore/cmd/ore_deferred_compute_pass.hpp"
#include "rive/renderer/ore/cmd/ore_deferred_context.hpp"
#include "rive/renderer/ore/cmd/ore_replay.hpp"
#include "rive/renderer/ore/ore_rstb_entry_bindings.hpp"
#include "rive/renderer/ore/ore_context.hpp"
#include "rive/renderer/ore/ore_draw_bindings.hpp"
#include <catch.hpp>
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

using namespace rive;
using namespace rive::ore;

namespace
{
constexpr uint8_t kAll = BindingMap::kStageVertex | BindingMap::kStageFragment |
                         BindingMap::kStageCompute;

BindingMap::Entry row(
    uint8_t binding,
    ResourceKind kind,
    uint8_t stageMask,
    uint16_t slot)
{
  BindingMap::Entry e{};
  e.binding = binding;
  e.kind = kind;
  e.stageMask = stageMask;
  for (int stage = 0; stage < 3; ++stage)
  {
    e.backendSlot[stage] = (stageMask & (1u << stage)) != 0
                               ? slot
                               : BindingMap::kAbsent;
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

// A uniform every stage sees, a storage buffer the fragment and compute
// entries use, and one only fs_main reads. cs_empty binds nothing.
struct Shader : ShaderModule
{
  Shader()
  {
    m_bindingMap.push(row(0, ResourceKind::UniformBuffer, kAll, 0));
    m_bindingMap.push(
        row(1,
            ResourceKind::StorageBufferRW,
            BindingMap::kStageFragment | BindingMap::kStageCompute,
            1));
    m_bindingMap.push(
        row(2, ResourceKind::StorageBufferRO, BindingMap::kStageFragment, 2));
    m_bindingMap.finalize();
    m_bindingMap.computeLayoutIds();
    m_entryBindings = {
        uses(1, "fs_main", {0x0000, 0x0001, 0x0002}),
        uses(2, "cs_main", {0x0000, 0x0001}),
        uses(2, "cs_empty", {})};
    RstbWorkgroup workgroup;
    workgroup.name = "cs_main";
    workgroup.size[0] = 8;
    workgroup.size[1] = 4;
    workgroup.memory = {256, 4};
    RstbWorkgroup empty;
    empty.name = "cs_empty";
    m_workgroups = {workgroup, empty};
  }
};

struct TestComputePipeline : ComputePipeline
{
  TestComputePipeline(const ComputePipelineDesc& desc)
      : ComputePipeline(nullptr, desc)
  {}
};

struct TestComputePass : ComputePass
{
  explicit TestComputePass(Context* context) : ComputePass(context) {}

  void setBindGroup(uint32_t, BindGroup*, const uint32_t*, uint32_t) override {}
  void applyPipeline(ComputePipeline*) override {}
  void dispatch(uint32_t x, uint32_t y, uint32_t z) override
  {
    dispatched.push_back({x, y, z});
  }
  void end() override { ended++; }

  std::vector<std::vector<uint32_t>> dispatched;
  int ended = 0;
};

// What a backend with compute overrides; every other factory is unreachable.
class ComputeContext : public Context
{
public:
  explicit ComputeContext(bool backendBuilds = true)
      : Context(nullptr)
      , m_backendBuilds(backendBuilds)
  {
    m_features.computeShaders = true;
    m_features.maxComputeWorkgroupSizeX = 256;
    m_features.maxComputeWorkgroupSizeY = 256;
    m_features.maxComputeWorkgroupSizeZ = 64;
    m_features.maxComputeInvocationsPerWorkgroup = 256;
    m_features.maxComputeWorkgroupStorageSize = 16384;
    m_features.maxComputeWorkgroupsPerDimension = 65535;
  }

  Features& editableFeatures() { return m_features; }

  // False stands for a recording context that has yet to meet its device.
  bool deviceKnown = true;
  bool featuresKnown() const override { return deviceKnown; }

  rcp<ComputePipeline> makeComputePipelineImpl(
      const ComputePipelineDesc& desc,
      std::string* outError) override
  {
    if (!m_backendBuilds)
      return Context::makeComputePipelineImpl(desc, outError);
    return make_rcp<TestComputePipeline>(desc);
  }

  rcp<Buffer> makeBuffer(const BufferDesc&) override { return nullptr; }
  rcp<Texture> makeTexture(const TextureDesc&) override { return nullptr; }
  rcp<TextureView> makeTextureViewImpl(const TextureViewDesc&) override
  {
    return nullptr;
  }
  rcp<Sampler> makeSampler(const SamplerDesc&) override { return nullptr; }
  rcp<ShaderModule> makeShaderModule(const ShaderModuleDesc&) override
  {
    return nullptr;
  }
  rcp<BindGroupLayout> makeBindGroupLayout(const BindGroupLayoutDesc&) override
  {
    return nullptr;
  }
  rcp<Pipeline> makePipeline(const PipelineDesc&, std::string*) override
  {
    return nullptr;
  }
  rcp<BindGroup> makeBindGroup(const BindGroupDesc&) override
  {
    return nullptr;
  }
  std::unique_ptr<RenderPass> beginRenderPass(
      const RenderPassDesc&,
      std::string*) override
  {
    return nullptr;
  }
  void beginFrame(const FrameDescriptor&) override {}
  void endFrame() override {}
  void waitForGPU() override {}
  rcp<TextureView> wrapCanvasTexture(gpu::RenderCanvas*) override
  {
    return nullptr;
  }
  rcp<TextureView> wrapRiveTexture(gpu::Texture*, uint32_t, uint32_t) override
  {
    return nullptr;
  }
  ShaderTarget shaderTarget() const override { return ShaderTarget::msl; }

private:
  bool m_backendBuilds;
};

// The error makeComputePipeline gives `desc`, empty when it builds.
std::string refusal(ComputeContext& ctx, const ComputePipelineDesc& desc)
{
  std::string error;
  rcp<ComputePipeline> pipeline = ctx.makeComputePipeline(desc, &error);
  CHECK((pipeline != nullptr) == error.empty());
  return error;
}

bool contains(const std::string& s, const char* part)
{
  return s.find(part) != std::string::npos;
}
} // namespace

TEST_CASE("a compute entry's map holds what it binds", "[ore][compute]")
{
  Shader shader;
  const BindingMap map = bindingMapForComputeEntry(&shader, "cs_main");

  // The uniform and the storage buffer cs_main uses, at compute slots only.
  REQUIRE(map.size() == 2);
  for (size_t i = 0; i < map.size(); ++i)
  {
    const BindingMap::Entry& e = map.at(i);
    CHECK(e.stageMask == BindingMap::kStageCompute);
    CHECK(e.backendSlot[0] == BindingMap::kAbsent);
    CHECK(e.backendSlot[1] == BindingMap::kAbsent);
    CHECK(e.backendSlot[2] == e.binding);
  }
  CHECK(
      map.lookup(0, 1, ResourceKind::StorageBufferRW, BindingMap::Stage::CS) ==
      1);

  // Never the layout the whole file interns.
  CHECK(map.layoutIdForGroup(0) != BindingMap::kNoLayoutId);
  CHECK(map.layoutIdForGroup(0) != shader.m_bindingMap.layoutIdForGroup(0));
}

TEST_CASE("clamping a map to a stage", "[ore][compute]")
{
  Shader shader;
  BindingMap clamped = shader.m_bindingMap;
  clamped.clampToStage(BindingMap::Stage::CS);

  // The buffer only fs_main's stage reads is gone.
  REQUIRE(clamped.size() == 2);
  CHECK(clamped.at(0).binding == 0);
  CHECK(clamped.at(1).binding == 1);

  // A map that loses no row still stops interning as its source: its rows
  // now name one stage.
  BindingMap computeOnly;
  computeOnly.push(
      row(0, ResourceKind::UniformBuffer, BindingMap::kStageCompute, 0));
  computeOnly.finalize();
  computeOnly.computeLayoutIds();
  const uint64_t before = computeOnly.layoutIdForGroup(0);
  computeOnly.clampToStage(BindingMap::Stage::CS);
  CHECK(computeOnly.size() == 1);
  CHECK(computeOnly.layoutIdForGroup(0) != before);
  CHECK(computeOnly.layoutIdForGroup(0) != BindingMap::kNoLayoutId);
}

TEST_CASE("a cut workgroups section decodes to nothing", "[ore][compute]")
{
  RstbWorkgroup first;
  first.name = "a";
  RstbWorkgroup second;
  second.name = "b";
  second.memory = {64};
  std::vector<uint8_t> bytes = encodeWorkgroups({first, second});
  std::vector<RstbWorkgroup> decoded;
  REQUIRE(decodeWorkgroups(bytes.data(), bytes.size(), decoded));
  CHECK(decoded.size() == 2);

  CHECK(!decodeWorkgroups(bytes.data(), bytes.size() - 1, decoded));
  CHECK(decoded.empty());
}

TEST_CASE("a row only compute reads carries its compute slot", "[ore][compute]")
{
  Shader shader;
  BindGroupLayoutEntry entries[3]{};

  // Rows a draw stage reads keep the bytes they had, compute slot absent.
  REQUIRE(
      populateBindGroupLayoutEntries(entries, 3, shader.m_bindingMap, 0) == 3);
  for (const BindGroupLayoutEntry& entry : entries)
    CHECK(entry.nativeSlotCS == BindGroupLayoutEntry::kNativeSlotAbsent);

  const BindingMap map = bindingMapForComputeEntry(&shader, "cs_main");
  REQUIRE(populateBindGroupLayoutEntries(entries, 3, map, 0) == 2);
  for (int i = 0; i < 2; ++i)
  {
    CHECK(entries[i].visibility.mask == StageVisibility::kCompute);
    CHECK(entries[i].nativeSlotVS == BindGroupLayoutEntry::kNativeSlotAbsent);
    CHECK(entries[i].nativeSlotFS == BindGroupLayoutEntry::kNativeSlotAbsent);
    CHECK(entries[i].nativeSlotCS == entries[i].binding);
  }

  // Entries filled for compute and then for a draw carry no compute slot.
  REQUIRE(
      populateBindGroupLayoutEntries(entries, 3, shader.m_bindingMap, 0) == 3);
  for (const BindGroupLayoutEntry& entry : entries)
    CHECK(entry.nativeSlotCS == BindGroupLayoutEntry::kNativeSlotAbsent);
}

TEST_CASE("a compute pipeline is checked before a backend", "[ore][compute]")
{
  Shader shader;
  ComputeContext ctx;
  ComputePipelineDesc desc;
  desc.module = &shader;
  desc.entryPoint = "cs_empty";
  CHECK(refusal(ctx, desc).empty());

  SECTION("a device without compute")
  {
    ctx.editableFeatures().computeShaders = false;
    CHECK(contains(refusal(ctx, desc), "not supported on this device"));
    // Without a place for the text it is the context's last error.
    CHECK(ctx.makeComputePipeline(desc) == nullptr);
    CHECK(contains(ctx.lastError(), "makeComputePipeline: compute shaders"));
  }

  SECTION("a device not met yet")
  {
    ctx.editableFeatures() = Features{};
    ctx.deviceKnown = false;
    CHECK(refusal(ctx, desc).empty());
    // Its limits wait for the device; what the desc itself lacks does not.
    desc.entryPoint = "cs_main";
    CHECK(contains(refusal(ctx, desc), "@group(0)"));
    desc.entryPoint = "fs_main";
    CHECK(contains(refusal(ctx, desc), "no compute entry point 'fs_main'"));
  }

  SECTION("a layout count without layouts")
  {
    desc.bindGroupLayoutCount = 1;
    CHECK(contains(refusal(ctx, desc), "layouts it was not given"));
  }

  SECTION("a backend that never implemented it")
  {
    ComputeContext claims(false);
    CHECK(contains(refusal(claims, desc), "does not run compute"));
    std::string error;
    CHECK(claims.beginComputePass({}, &error) == nullptr);
    CHECK(contains(error, "does not run compute"));
  }

  desc.entryPoint = "cs_main";

  SECTION("an entry point that is not compute")
  {
    desc.entryPoint = "fs_main";
    CHECK(contains(refusal(ctx, desc), "no compute entry point 'fs_main'"));
    desc.entryPoint = nullptr;
    CHECK(contains(refusal(ctx, desc), "needs a shader module"));
  }

  SECTION("a workgroup past the device")
  {
    ctx.editableFeatures().maxComputeWorkgroupSizeY = 2;
    CHECK(contains(refusal(ctx, desc), "@workgroup_size y=4 exceeds"));
    ctx.editableFeatures().maxComputeWorkgroupSizeY = 256;
    ctx.editableFeatures().maxComputeInvocationsPerWorkgroup = 16;
    CHECK(contains(refusal(ctx, desc), "32 invocations"));
    ctx.editableFeatures().maxComputeInvocationsPerWorkgroup = 256;
    // 256 and 4 bytes, the second counted as 16.
    ctx.editableFeatures().maxComputeWorkgroupStorageSize = 271;
    CHECK(contains(refusal(ctx, desc), "272 bytes of workgroup variables"));
    ctx.editableFeatures().maxComputeWorkgroupStorageSize = 272;
    CHECK(!contains(refusal(ctx, desc), "workgroup variables"));
    shader.m_workgroups[0].size[2] = 0;
    CHECK(contains(refusal(ctx, desc), "@workgroup_size z is 0"));
  }

  SECTION("layouts that leave out a binding")
  {
    // No layouts for an entry point that binds two things.
    const std::string error = refusal(ctx, desc);
    INFO(error);
    CHECK(contains(error, "@group(0)"));

    // The layout its own map derives covers both.
    ore::cmd::DeferredOreContext layouts(nullptr);
    BindGroupLayoutEntry entries[2]{};
    REQUIRE(
        populateBindGroupLayoutEntries(
            entries,
            2,
            bindingMapForComputeEntry(&shader, "cs_main"),
            0) == 2);
    BindGroupLayoutDesc layoutDesc;
    layoutDesc.entries = entries;
    layoutDesc.entryCount = 2;
    rcp<BindGroupLayout> layout = layouts.makeBindGroupLayout(layoutDesc);
    BindGroupLayout* group0[] = {layout.get()};
    desc.bindGroupLayouts = group0;
    desc.bindGroupLayoutCount = 1;
    CHECK(refusal(ctx, desc).empty());

    layoutDesc.entryCount = 1;
    layout = layouts.makeBindGroupLayout(layoutDesc);
    group0[0] = layout.get();
    CHECK(contains(refusal(ctx, desc), "@binding(1)"));
  }

  SECTION("layouts past WebGPU's count for the compute stage")
  {
    ore::cmd::DeferredOreContext layouts(nullptr);
    BindGroupLayoutEntry entries[9]{};
    for (uint32_t i = 0; i < 9; ++i)
    {
      entries[i].binding = i;
      entries[i].kind = BindingKind::storageBufferRO;
      entries[i].visibility.mask = StageVisibility::kCompute;
    }
    BindGroupLayoutDesc layoutDesc;
    layoutDesc.entries = entries;
    layoutDesc.entryCount = 9;
    rcp<BindGroupLayout> layout = layouts.makeBindGroupLayout(layoutDesc);
    REQUIRE(layout != nullptr);
    BindGroupLayout* group0[] = {layout.get()};
    desc.bindGroupLayouts = group0;
    desc.bindGroupLayoutCount = 1;
    CHECK(contains(refusal(ctx, desc), "compute stage 9 storage buffers"));
  }
}

TEST_CASE("a compute pipeline keeps what the pass needs", "[ore][compute]")
{
  Shader shader;
  ComputePipelineDesc desc;
  desc.module = &shader;
  std::string entry = "cs_main";
  desc.entryPoint = entry.c_str();
  auto pipeline = make_rcp<TestComputePipeline>(desc);
  // The desc's strings belong to the caller.
  entry = "gone";

  CHECK(pipeline->entryPoint() == "cs_main");
  CHECK(pipeline->workgroup().size[0] == 8);
  CHECK(pipeline->workgroup().memory == std::vector<uint32_t>{256, 4});
  CHECK(pipeline->m_bindingMap.size() == 2);
}

TEST_CASE("a compute pass refuses what it cannot dispatch", "[ore][compute]")
{
  Shader shader;
  ComputeContext ctx;
  ComputePipelineDesc desc;
  desc.module = &shader;
  desc.entryPoint = "cs_main";
  auto pipeline = make_rcp<TestComputePipeline>(desc);
  TestComputePass pass(&ctx);

  pass.dispatchWorkgroups(1);
  CHECK(contains(ctx.lastError(), "no pipeline is set"));
  CHECK(pass.dispatched.empty());
  ctx.clearLastError();

  pass.setPipeline(pipeline.get());
  pass.dispatchWorkgroups(4, 2);
  REQUIRE(pass.dispatched.size() == 1);
  CHECK(pass.dispatched[0] == std::vector<uint32_t>{4, 2, 1});

  pass.dispatchWorkgroups(1, 70000, 1);
  CHECK(contains(ctx.lastError(), "workgroup count y=70000 exceeds"));
  ctx.clearLastError();
  CHECK(pass.dispatched.size() == 1);

  // A zero count is no error and runs nothing.
  pass.dispatchWorkgroups(0, 5, 5);
  CHECK(ctx.lastError().empty());
  CHECK(pass.dispatched.size() == 1);

  // A device not met yet has no count to hold the dispatch to.
  ctx.deviceKnown = false;
  pass.dispatchWorkgroups(1, 70000, 1);
  CHECK(ctx.lastError().empty());
  CHECK(pass.dispatched.size() == 2);

  pass.finish();
  pass.finish();
  CHECK(pass.ended == 1);
  CHECK(pass.isFinished());
  pass.dispatchWorkgroups(1);
  CHECK(pass.dispatched.size() == 2);

  // Nowhere to report to, so nothing runs.
  TestComputePass orphan(nullptr);
  orphan.setPipeline(pipeline.get());
  orphan.dispatchWorkgroups(1);
  CHECK(orphan.dispatched.empty());
}

namespace
{
// A recording context makes buffers and layouts without a device.
struct StorageGroup
{
  ore::cmd::DeferredOreContext ctx{nullptr};
  Shader shader;
  rcp<Buffer> uniform;
  rcp<Buffer> storage;
  rcp<BindGroupLayout> layout;
  BindGroupDesc::UBOEntry ubo{};
  BindGroupDesc::StorageEntry bound{};
  BindGroupDesc desc;

  StorageGroup()
  {
    BufferDesc bufferDesc;
    bufferDesc.usage = BufferUsage::uniform;
    bufferDesc.size = 16;
    uniform = ctx.makeBuffer(bufferDesc);
    bufferDesc.usage = BufferUsage::storage;
    bufferDesc.size = 1024;
    storage = ctx.makeBuffer(bufferDesc);

    BindGroupLayoutEntry entries[2]{};
    REQUIRE(
        populateBindGroupLayoutEntries(
            entries,
            2,
            bindingMapForComputeEntry(&shader, "cs_main"),
            0) == 2);
    entries[1].minBindingSize = 16;
    BindGroupLayoutDesc layoutDesc;
    layoutDesc.entries = entries;
    layoutDesc.entryCount = 2;
    layout = ctx.makeBindGroupLayout(layoutDesc);

    ubo.slot = 0;
    ubo.buffer = uniform.get();
    bound.slot = 1;
    bound.buffer = storage.get();
    desc.layout = layout.get();
    desc.ubos = &ubo;
    desc.uboCount = 1;
    desc.storageBuffers = &bound;
    desc.storageBufferCount = 1;
  }

  // What a backend that binds storage buffers says of the group.
  std::string refusal() const
  {
    std::string error;
    validateBindGroupDesc(desc, &error, StorageBinding::bound);
    return error;
  }
};
} // namespace

TEST_CASE(
    "a storage buffer binds only where a backend takes it",
    "[ore][compute]")
{
  StorageGroup group;
  std::string error;
  CHECK(!validateBindGroupDesc(group.desc, &error));
  CHECK(contains(error, "binding storage buffers is not supported"));
  CHECK(group.refusal().empty());

  // A recording context refuses for a device it knows binds none.
  CHECK(group.ctx.makeBindGroup(group.desc) != nullptr);
  ReplayCaps caps;
  caps.featuresKnown = true;
  ore::cmd::DeferredOreContext without(caps);
  BufferDesc storageDesc;
  storageDesc.usage = BufferUsage::storage;
  storageDesc.size = 64;
  CHECK(without.makeBuffer(storageDesc) == nullptr);
  CHECK(contains(without.lastError(), "no storage buffers"));
  CHECK(without.makeBindGroup(group.desc) == nullptr);
  CHECK(contains(
      without.lastError(),
      "binding storage buffers is not supported"));
}

TEST_CASE("a storage binding follows WebGPU's range rules", "[ore][compute]")
{
  StorageGroup group;

  SECTION("left empty")
  {
    group.desc.storageBufferCount = 0;
    CHECK(contains(group.refusal(), "leaves this storage buffer empty"));
  }

  SECTION("a buffer of the other usage")
  {
    group.bound.buffer = group.uniform.get();
    CHECK(contains(group.refusal(), "made without storage usage"));
    group.bound.buffer = group.storage.get();
    group.ubo.buffer = group.storage.get();
    CHECK(contains(group.refusal(), "made without uniform usage"));
  }

  SECTION("bound as the other kind")
  {
    // Every binding is filled, and one more entry names the wrong kind.
    const BindGroupDesc::StorageEntry storages[] = {
        group.bound,
        {0, group.storage.get(), 0, 0}};
    group.desc.storageBuffers = storages;
    group.desc.storageBufferCount = 2;
    CHECK(contains(group.refusal(), "the layout has no storage buffer here"));

    // A uniform entry on the storage binding reads as it always has.
    group.desc.storageBufferCount = 0;
    const BindGroupDesc::UBOEntry ubos[] = {
        group.ubo,
        {1, group.uniform.get(), 0, 0}};
    group.desc.ubos = ubos;
    group.desc.uboCount = 2;
    std::string error;
    CHECK(!validateBindGroupDesc(group.desc, &error));
    CHECK(contains(error, "the layout has no uniform buffer here"));

    group.desc.storageBufferCount = 1;
    CHECK(
        contains(group.refusal(), "binds this storage buffer more than once"));
  }

  SECTION("its range")
  {
    group.bound.offset = 100;
    CHECK(contains(group.refusal(), "requires of storage buffers"));
    group.bound.offset = 768;
    group.bound.size = 512;
    CHECK(contains(group.refusal(), "exceeds the 1024 byte buffer"));
    group.bound.size = 8;
    CHECK(contains(group.refusal(), "shader's storage buffer needs 16"));
    group.bound.size = 18;
    CHECK(contains(group.refusal(), "a multiple of 4 of storage buffers"));
    group.bound.size = 20;
    CHECK(group.refusal().empty());
  }

  SECTION("past the size WebGPU binds")
  {
    constexpr uint32_t kLimit = 128 * 1024 * 1024;
    BufferDesc bufferDesc;
    bufferDesc.usage = BufferUsage::storage;
    bufferDesc.size = kLimit + 4;
    rcp<Buffer> large = group.ctx.makeBuffer(bufferDesc);
    group.bound.buffer = large.get();
    CHECK(contains(group.refusal(), "over the 134217728 WebGPU allows"));
    group.bound.size = kLimit;
    CHECK(group.refusal().empty());
  }

  SECTION("no bytes, under a layout that asks for none")
  {
    BindGroupLayoutEntry entries[2]{};
    REQUIRE(
        populateBindGroupLayoutEntries(
            entries,
            2,
            bindingMapForComputeEntry(&group.shader, "cs_main"),
            0) == 2);
    REQUIRE(entries[1].minBindingSize == 0);
    BindGroupLayoutDesc layoutDesc;
    layoutDesc.entries = entries;
    layoutDesc.entryCount = 2;
    rcp<BindGroupLayout> loose = group.ctx.makeBindGroupLayout(layoutDesc);
    group.desc.layout = loose.get();
    group.bound.offset = 1024;
    CHECK(contains(group.refusal(), "binds 0 bytes"));
    group.bound.offset = 768;
    CHECK(group.refusal().empty());
  }
}

namespace
{
// A group of `layout` binding `first` at binding 0 and `second` at binding 1.
rcp<BindGroup> storagePair(
    Context& ctx,
    BindGroupLayout* layout,
    BindGroupDesc::StorageEntry first,
    BindGroupDesc::StorageEntry second)
{
  first.slot = 0;
  second.slot = 1;
  const BindGroupDesc::StorageEntry entries[] = {first, second};
  BindGroupDesc desc;
  desc.layout = layout;
  desc.storageBuffers = entries;
  desc.storageBufferCount = 2;
  return ctx.makeBindGroup(desc);
}

BindGroupDesc::StorageEntry whole(const rcp<Buffer>& buffer)
{
  return {0, buffer.get(), 0, 0};
}

// Two storage buffers for the compute stage at bindings 0 and 1.
rcp<BindGroupLayout> storageLayout(
    Context& ctx,
    uint32_t group,
    BindingKind first,
    BindingKind second)
{
  BindGroupLayoutEntry entries[2]{};
  entries[0].binding = 0;
  entries[0].kind = first;
  entries[0].visibility.mask = StageVisibility::kCompute;
  entries[1].binding = 1;
  entries[1].kind = second;
  entries[1].visibility.mask = StageVisibility::kCompute;
  BindGroupLayoutDesc desc;
  desc.groupIndex = group;
  desc.entries = entries;
  desc.entryCount = 2;
  return ctx.makeBindGroupLayout(desc);
}
} // namespace

TEST_CASE("a storage buffer is written from one place", "[ore][compute]")
{
  ore::cmd::DeferredOreContext ctx{nullptr};
  BufferDesc bufferDesc;
  bufferDesc.usage = BufferUsage::storage;
  bufferDesc.size = 512;
  rcp<Buffer> a = ctx.makeBuffer(bufferDesc);
  rcp<Buffer> b = ctx.makeBuffer(bufferDesc);
  rcp<Buffer> c = ctx.makeBuffer(bufferDesc);
  rcp<Buffer> d = ctx.makeBuffer(bufferDesc);

  // Each of these groups reads binding 0 and writes binding 1.
  const BindingKind read = BindingKind::storageBufferRO;
  const BindingKind write = BindingKind::storageBufferRW;
  rcp<BindGroupLayout> firstLayout = storageLayout(ctx, 0, read, write);
  rcp<BindGroupLayout> secondLayout = storageLayout(ctx, 1, read, write);
  BindGroupLayout* first = firstLayout.get();
  BindGroupLayout* second = secondLayout.get();
  BindGroupLayout* both[] = {first, second};

  Shader shader;
  ComputePipelineDesc desc;
  desc.module = &shader;
  desc.entryPoint = "cs_main";
  desc.bindGroupLayouts = both;
  desc.bindGroupLayoutCount = 2;
  auto pipeline = make_rcp<TestComputePipeline>(desc);
  std::string error;

  SECTION("in a draw or dispatch")
  {
    DrawBindings bindings;
    bindings.setPipeline(pipeline.get());
    bindings.setBindGroup(0, storagePair(ctx, first, whole(a), whole(b)).get());
    bindings.setBindGroup(
        1,
        storagePair(ctx, second, whole(a), whole(c)).get());
    CHECK(bindings.validate(&error));

    // Read and written by one group.
    bindings.setBindGroup(0, storagePair(ctx, first, whole(b), whole(b)).get());
    CHECK(!bindings.validate(&error));
    CHECK(contains(
        error,
        "@group(0) @binding(1): a storage buffer bound read_write is "
        "also bound read only"));

    // Written by one group and read by the other.
    bindings.setBindGroup(0, storagePair(ctx, first, whole(c), whole(d)).get());
    CHECK(!bindings.validate(&error));
    CHECK(contains(error, "@group(1) @binding(1)"));
    CHECK(contains(error, "also bound read only"));

    // Written by both.
    bindings.setBindGroup(0, storagePair(ctx, first, whole(a), whole(c)).get());
    CHECK(!bindings.validate(&error));
    CHECK(contains(
        error,
        "@group(0) @binding(1): a storage buffer is bound read_write "
        "twice in one draw or dispatch"));

    // And by both over separate bytes.
    bindings.setBindGroup(
        0,
        storagePair(ctx, first, whole(a), {0, c.get(), 0, 256}).get());
    bindings.setBindGroup(
        1,
        storagePair(ctx, second, whole(a), {0, c.get(), 256, 256}).get());
    error.clear();
    CHECK(!bindings.validate(&error));
    CHECK(contains(error, "@group(0) @binding(1)"));
    CHECK(contains(error, "bound read_write twice"));

    // A group the pipeline has no layout for stays out of the dispatch.
    desc.bindGroupLayoutCount = 1;
    auto single = make_rcp<TestComputePipeline>(desc);
    bindings.setPipeline(single.get());
    CHECK(bindings.validate(&error));
  }

  // WebGPU allows it, but Direct3D 11 then drops the writes without a word.
  SECTION("not twice, even where the two ranges share no byte")
  {
    rcp<BindGroupLayout> writes = storageLayout(ctx, 0, write, write);
    BindGroupLayout* writeLayouts[] = {writes.get()};
    desc.bindGroupLayouts = writeLayouts;
    desc.bindGroupLayoutCount = 1;
    auto writer = make_rcp<TestComputePipeline>(desc);
    DrawBindings bindings;
    bindings.setPipeline(writer.get());

    bindings.setBindGroup(
        0,
        storagePair(
            ctx,
            writes.get(),
            {0, a.get(), 0, 256},
            {0, a.get(), 256, 256})
            .get());
    CHECK(!bindings.validate(&error));
    CHECK(contains(
        error,
        "@group(0) @binding(0): a storage buffer is bound read_write "
        "twice in one draw or dispatch"));

    bindings.setBindGroup(
        0,
        storagePair(ctx, writes.get(), whole(a), whole(b)).get());
    CHECK(bindings.validate(&error));
  }

  SECTION("in a render pass, over every group it sets")
  {
    DrawBindings pass;
    CHECK(pass.joinRenderPass(
        storagePair(ctx, first, whole(a), whole(b)).get(),
        &error));
    // Reads may repeat, and so may writes.
    CHECK(pass.joinRenderPass(
        storagePair(ctx, second, whole(a), whole(c)).get(),
        &error));
    CHECK(pass.joinRenderPass(
        storagePair(ctx, first, whole(d), whole(b)).get(),
        &error));

    CHECK(!pass.joinRenderPass(
        storagePair(ctx, first, whole(b), whole(c)).get(),
        &error));
    CHECK(contains(
        error,
        "@group(0) @binding(0): a render pass cannot bind one storage "
        "buffer both read_write and read only"));
    CHECK(!pass.joinRenderPass(
        storagePair(ctx, second, whole(d), whole(a)).get(),
        &error));
    CHECK(contains(error, "@group(1) @binding(1)"));

    // A refused group leaves the pass as it was.
    CHECK(pass.joinRenderPass(
        storagePair(ctx, first, whole(a), whole(c)).get(),
        &error));

    DrawBindings alone;
    CHECK(!alone.joinRenderPass(
        storagePair(ctx, first, whole(a), whole(a)).get(),
        &error));
  }
}

TEST_CASE("a recorded bind group keeps its storage buffers", "[ore][compute]")
{
  using namespace rive::ore::cmd;
  BindGroupDesc::StorageEntry entry{};
  entry.slot = 3;
  entry.offset = 256;
  entry.size = 64;
  BindGroupDesc desc;
  desc.storageBuffers = &entry;
  desc.storageBufferCount = 1;
  const ResourceHandle buffer[1] = {9};

  OreCommandBuffer cb;
  recordMakeBindGroup(
      cb,
      0,
      0,
      desc,
      4,
      Span<const ResourceHandle>(nullptr, 0),
      Span<const ResourceHandle>(nullptr, 0),
      Span<const ResourceHandle>(nullptr, 0),
      Span<const ResourceHandle>(buffer, 1));

  OreCommandReader reader(cb.commandBytes(), cb.blobBytes());
  CommandType type;
  REQUIRE(reader.next(type));
  REQUIRE(type == CommandType::makeBindGroup);
  reader.read<MakeResourcePOD>();
  const auto pod = reader.read<BindGroupDescPOD>();
  REQUIRE(pod.storageBufferCount == 1);
  REQUIRE(pod.storageBuffers.size == sizeof(UBOEntryPOD));
  UBOEntryPOD recorded;
  memcpy(
      &recorded,
      reader.blobAt(pod.storageBuffers.offset, pod.storageBuffers.size).data(),
      sizeof(recorded));
  CHECK(recorded.slot == 3);
  CHECK(recorded.buffer == 9);
  CHECK(recorded.offset == 256);
  CHECK(recorded.size == 64);
}

namespace
{
struct StubBuffer : Buffer
{
  explicit StubBuffer(const BufferDesc& desc) : Buffer(desc.size, desc.usage) {}
  void update(const void*, uint32_t, uint32_t) override {}
};

struct StubLayout : BindGroupLayout
{
  explicit StubLayout(const BindGroupLayoutDesc& desc)
  {
    m_groupIndex = desc.groupIndex;
    m_entries.assign(desc.entries, desc.entries + desc.entryCount);
  }
};

struct StubBindGroup : BindGroup
{};

struct StubPipeline : Pipeline
{
  explicit StubPipeline(const PipelineDesc& desc) : Pipeline(desc) {}
};

// A device's draw pipeline with `colors` color targets. One fits a pass
// begun from a default RenderPassDesc.
rcp<Pipeline> devicePipeline(Shader& shader, uint32_t colors)
{
  PipelineDesc desc;
  desc.vertexModule = &shader;
  desc.vertexEntryPoint = "vs_main";
  desc.fragmentModule = &shader;
  desc.fragmentEntryPoint = "fs_main";
  for (uint32_t i = 0; i < colors; ++i)
    desc.colorTargets[i].format = TextureFormat{};
  desc.colorCount = colors;
  return make_rcp<StubPipeline>(desc);
}

// Makes what replay asks for and keeps what the bind group was made from.
class ReplayTarget : public ComputeContext
{
public:
  rcp<Buffer> makeBuffer(const BufferDesc& desc) override
  {
    return make_rcp<StubBuffer>(desc);
  }
  rcp<BindGroupLayout> makeBindGroupLayout(
      const BindGroupLayoutDesc& desc) override
  {
    return make_rcp<StubLayout>(desc);
  }
  rcp<BindGroup> makeBindGroup(const BindGroupDesc& desc) override
  {
    uniforms.assign(desc.ubos, desc.ubos + desc.uboCount);
    storage.assign(
        desc.storageBuffers,
        desc.storageBuffers + desc.storageBufferCount);
    return make_rcp<StubBindGroup>();
  }

  std::vector<BindGroupDesc::UBOEntry> uniforms;
  std::vector<BindGroupDesc::StorageEntry> storage;
};
} // namespace

TEST_CASE("storage buffers survive record and replay", "[ore][compute]")
{
  using namespace rive::ore::cmd;
  StorageGroup group;
  group.bound.offset = 256;
  group.bound.size = 64;
  REQUIRE(group.ctx.makeBindGroup(group.desc) != nullptr);

  ReplayTarget target;
  OreResident table;
  const OreCommandBuffer& stream = group.ctx.stream();
  replayOreStream(target, stream.commandBytes(), stream.blobBytes(), table);

  REQUIRE(target.uniforms.size() == 1);
  REQUIRE(target.storage.size() == 1);
  const BindGroupDesc::StorageEntry& replayed = target.storage[0];
  CHECK(replayed.slot == 1);
  CHECK(replayed.offset == 256);
  CHECK(replayed.size == 64);
  // The buffer replay made for the storage one, not the uniform's.
  REQUIRE(replayed.buffer != nullptr);
  CHECK(replayed.buffer->usage() == BufferUsage::storage);
  CHECK(replayed.buffer->size() == 1024);
  CHECK(target.uniforms[0].buffer->usage() == BufferUsage::uniform);
}

namespace
{
struct StubModule : ShaderModule
{};

// Names each call into the list of the device that began it.
struct LoggedComputePass : ComputePass
{
  LoggedComputePass(Context* context, std::vector<std::string>* calls)
      : ComputePass(context)
      , m_calls(calls)
  {
    m_calls->push_back("begin");
  }
  ~LoggedComputePass() override { finish(); }

  void setBindGroup(uint32_t index, BindGroup*, const uint32_t*, uint32_t)
      override
  {
    m_calls->push_back("group " + std::to_string(index));
  }
  void applyPipeline(ComputePipeline* pipeline) override
  {
    m_calls->push_back("pipeline " + pipeline->entryPoint());
  }
  void dispatch(uint32_t x, uint32_t y, uint32_t z) override
  {
    m_calls->push_back(
        "dispatch " + std::to_string(x) + " " + std::to_string(y) + " " +
        std::to_string(z));
  }
  void end() override { m_calls->push_back("end"); }

  std::vector<std::string>* m_calls;
};

// Names a storage copy in its device's list, with the buffer's size and the
// first value copied.
struct LoggedBuffer : StubBuffer
{
  LoggedBuffer(const BufferDesc& desc, std::vector<std::string>* calls)
      : StubBuffer(desc)
      , m_calls(calls)
  {}
  void update(const void* data, uint32_t size, uint32_t) override
  {
    if (usage() != BufferUsage::storage)
      return;
    uint32_t first = 0;
    memcpy(&first, data, std::min<size_t>(size, sizeof(first)));
    m_calls->push_back(
        "copy " + std::to_string(this->size()) + ":" + std::to_string(first));
  }

  std::vector<std::string>* m_calls;
};

// A device that runs compute, as the list of what its passes were asked. A
// render pass only leaves its place in that list, and so does a storage copy.
class ComputeTarget : public ReplayTarget
{
public:
  rcp<Buffer> makeBuffer(const BufferDesc& desc) override
  {
    return make_rcp<LoggedBuffer>(desc, &calls);
  }
  rcp<ShaderModule> makeShaderModule(const ShaderModuleDesc& desc) override
  {
    auto module = make_rcp<StubModule>();
    module->applyBindingMapFromDesc(desc);
    return module;
  }
  std::unique_ptr<ComputePass> beginComputePass(
      const ComputePassDesc&,
      std::string*) override
  {
    return std::make_unique<LoggedComputePass>(this, &calls);
  }
  std::unique_ptr<RenderPass> beginRenderPass(
      const RenderPassDesc&,
      std::string*) override
  {
    calls.push_back("render");
    return nullptr;
  }
  bool usesDeferredFrameReplay() const override { return frameReplay; }

  bool frameReplay = false;
  std::vector<std::string> calls;
};

using Calls = std::vector<std::string>;

// A stream's pass commands in order, which leaves out the makes, with those
// of the `also` types among them.
std::vector<ore::cmd::CommandType> passCommandsIn(
    const ore::cmd::OreCommandBuffer& stream,
    const std::vector<ore::cmd::CommandType>& also = {})
{
  ore::cmd::OreCommandReader reader(stream.commandBytes(), stream.blobBytes());
  std::vector<ore::cmd::CommandType> types;
  ore::cmd::CommandType type;
  while (reader.next(type))
  {
    if (ore::cmd::isPassCommand(type) ||
        std::find(also.begin(), also.end(), type) != also.end())
      types.push_back(type);
    reader.skip(ore::cmd::orePayloadSizeOf(type));
  }
  return types;
}

// StorageGroup's layout and buffers behind a recorded compute pipeline and
// bind group, with the shader recorded too.
struct RecordedCompute : StorageGroup
{
  rcp<ShaderModule> module;
  rcp<ComputePipeline> pipeline;
  rcp<BindGroup> bindGroup;

  RecordedCompute()
  {
    const std::vector<uint8_t> map = shader.m_bindingMap.toBlob();
    const std::vector<uint8_t> entries = encodeEntryBindings(
        shader.m_entryBindings);
    const std::vector<uint8_t> workgroups = encodeWorkgroups(
        shader.m_workgroups);
    ShaderModuleDesc moduleDesc{};
    moduleDesc.bindingMapBytes = map.data();
    moduleDesc.bindingMapSize = static_cast<uint32_t>(map.size());
    moduleDesc.entryBindingBytes = entries.data();
    moduleDesc.entryBindingSize = static_cast<uint32_t>(entries.size());
    moduleDesc.workgroupBytes = workgroups.data();
    moduleDesc.workgroupBytesSize = static_cast<uint32_t>(workgroups.size());
    module = ctx.makeShaderModule(moduleDesc);

    BindGroupLayout* layouts[] = {layout.get()};
    ComputePipelineDesc pipelineDesc;
    pipelineDesc.module = module.get();
    pipelineDesc.entryPoint = "cs_main";
    pipelineDesc.bindGroupLayouts = layouts;
    pipelineDesc.bindGroupLayoutCount = 1;
    std::string error;
    pipeline = ctx.makeComputePipeline(pipelineDesc, &error);
    INFO("makeComputePipeline: " << error);
    REQUIRE(pipeline != nullptr);
    bindGroup = ctx.makeBindGroup(desc);
    REQUIRE(bindGroup != nullptr);
  }

  // A pass that dispatches `x` workgroups, left open.
  std::unique_ptr<ComputePass> dispatching(uint32_t x)
  {
    auto pass = ctx.beginComputePass({});
    REQUIRE(pass != nullptr);
    pass->setPipeline(pipeline.get());
    pass->setBindGroup(0, bindGroup.get());
    pass->dispatchWorkgroups(x);
    return pass;
  }

  // What `target`'s passes are asked when the stream replays on it.
  Calls replayOn(ComputeTarget& target)
  {
    ore::cmd::OreResident table;
    const ore::cmd::OreCommandBuffer& stream = ctx.stream();
    ore::cmd::replayOreStream(
        target,
        stream.commandBytes(),
        stream.blobBytes(),
        table);
    return target.calls;
  }

  std::vector<ore::cmd::CommandType> passCommands(
      const std::vector<ore::cmd::CommandType>& also = {}) const
  {
    return passCommandsIn(ctx.stream(), also);
  }

  size_t recorded(ore::cmd::CommandType wanted) const
  {
    const ore::cmd::OreCommandBuffer& stream = ctx.stream();
    ore::cmd::OreCommandReader reader(
        stream.commandBytes(),
        stream.blobBytes());
    size_t count = 0;
    ore::cmd::CommandType type;
    while (reader.next(type))
    {
      count += type == wanted ? 1 : 0;
      reader.skip(ore::cmd::orePayloadSizeOf(type));
    }
    return count;
  }
};

using Command = ore::cmd::CommandType;
using Commands = std::vector<Command>;

// What RecordedCompute::dispatching records before its finish.
const Commands kDispatching = {
    Command::beginComputePass,
    Command::setComputePipeline,
    Command::setBindGroup,
    Command::dispatchWorkgroups};

Commands operator+(Commands a, const Commands& b)
{
  a.insert(a.end(), b.begin(), b.end());
  return a;
}

const Calls kDispatch4 =
    {"begin", "pipeline cs_main", "group 0", "dispatch 4 2 1", "end"};

// What a device is asked for RecordedCompute::dispatching(4) and its finish.
const Calls kDispatched4 =
    {"begin", "pipeline cs_main", "group 0", "dispatch 4 1 1", "end"};

Calls operator+(Calls a, const Calls& b)
{
  a.insert(a.end(), b.begin(), b.end());
  return a;
}
} // namespace

TEST_CASE("a recorded compute pass replays what it was asked", "[ore][compute]")
{
  RecordedCompute scene;
  int runs = 0;
  scene.ctx.segmentObserver =
      [&](size_t, ore::cmd::DeferredOreContext::SegmentDeps&&) { runs++; };

  auto pass = scene.ctx.beginComputePass({});
  REQUIRE(pass != nullptr);
  pass->setPipeline(scene.pipeline.get());
  pass->setBindGroup(0, scene.bindGroup.get());
  pass->dispatchWorkgroups(4, 2);
  // Refused here, so replay never sees it.
  pass->dispatchWorkgroups(1, 1, 0);
  CHECK(runs == 0);
  pass->finish();
  CHECK(runs == 1);
  CHECK(scene.ctx.lastError().empty());

  ComputeTarget target;
  CHECK(scene.replayOn(target) == kDispatch4);
  CHECK(target.lastError().empty());

  // A device that turns out to have no compute builds no pipeline, and the
  // pass drops what needed it.
  ComputeTarget without;
  without.editableFeatures().computeShaders = false;
  CHECK(scene.replayOn(without) == Calls{"begin", "end"});
}

TEST_CASE("a pass begun inside another replays ahead of it", "[ore][compute]")
{
  RecordedCompute scene;
  ComputeTarget target;
  Calls expected;
  Commands commands;

  SECTION("compute inside a render pass")
  {
    auto draw = scene.ctx.beginRenderPass(RenderPassDesc{});
    REQUIRE(draw != nullptr);
    draw->draw(3);
    scene.dispatching(4)->finish();
    draw->draw(3);
    draw->finish();
    expected = {
        "begin",
        "pipeline cs_main",
        "group 0",
        "dispatch 4 1 1",
        "end",
        "render"};
    // The render pass keeps both of its draws.
    commands = kDispatching + Commands{
                                  Command::finish,
                                  Command::beginRenderPass,
                                  Command::draw,
                                  Command::draw,
                                  Command::finish};
  }

  SECTION("a render pass inside compute")
  {
    auto compute = scene.dispatching(4);
    scene.ctx.beginRenderPass(RenderPassDesc{})->finish();
    compute->dispatchWorkgroups(5);
    compute->finish();
    expected = {
        "render",
        "begin",
        "pipeline cs_main",
        "group 0",
        "dispatch 4 1 1",
        "dispatch 5 1 1",
        "end"};
    commands = Commands{Command::beginRenderPass, Command::finish} +
               kDispatching +
               Commands{Command::dispatchWorkgroups, Command::finish};
  }

  SECTION("compute inside compute")
  {
    auto outer = scene.dispatching(1);
    scene.dispatching(2)->finish();
    outer->finish();
    expected = {
        "begin",
        "pipeline cs_main",
        "group 0",
        "dispatch 2 1 1",
        "end",
        "begin",
        "pipeline cs_main",
        "group 0",
        "dispatch 1 1 1",
        "end"};
    commands = kDispatching + Commands{Command::finish} + kDispatching +
               Commands{Command::finish};
  }

  // A command for the outer pass, with the inner one still open, would
  // otherwise be recorded into the inner pass.
  SECTION("a draw on the outer pass finishes the compute pass inside it")
  {
    auto draw = scene.ctx.beginRenderPass(RenderPassDesc{});
    auto compute = scene.dispatching(4);
    draw->draw(3);
    CHECK(compute->isFinished());
    compute->dispatchWorkgroups(9);
    draw->finish();
    expected = {
        "begin",
        "pipeline cs_main",
        "group 0",
        "dispatch 4 1 1",
        "end",
        "render"};
    commands = kDispatching + Commands{
                                  Command::finish,
                                  Command::beginRenderPass,
                                  Command::draw,
                                  Command::finish};
  }

  SECTION("a dispatch on the outer pass finishes the render pass inside it")
  {
    auto compute = scene.dispatching(1);
    auto draw = scene.ctx.beginRenderPass(RenderPassDesc{});
    draw->draw(3);
    compute->setBindGroup(0, scene.bindGroup.get());
    compute->dispatchWorkgroups(2);
    CHECK(draw->isFinished());
    // Finished, the render pass records nothing into the compute pass and
    // holds nothing for it.
    BufferDesc vertexDesc;
    vertexDesc.usage = BufferUsage::vertex;
    vertexDesc.size = 16;
    rcp<Buffer> onDevice = make_rcp<StubBuffer>(vertexDesc);
    const int32_t groupRefs = scene.bindGroup->debugging_refcnt();
    draw->draw(3);
    draw->setBindGroup(0, scene.bindGroup.get());
    draw->setVertexBuffer(0, onDevice.get());
    draw->setIndexBuffer(onDevice.get(), IndexFormat::uint16);
    // Nor does it judge a pipeline that fits no pass of its shape.
    rcp<Pipeline> mismatched = devicePipeline(scene.shader, 0);
    scene.ctx.clearLastError();
    draw->setPipeline(mismatched.get());
    CHECK(scene.ctx.lastError().empty());
    CHECK(scene.bindGroup->debugging_refcnt() == groupRefs);
    CHECK(scene.ctx.realResources().empty());
    compute->finish();
    expected = {
        "render",
        "begin",
        "pipeline cs_main",
        "group 0",
        "dispatch 1 1 1",
        "group 0",
        "dispatch 2 1 1",
        "end"};
    commands =
        Commands{Command::beginRenderPass, Command::draw, Command::finish} +
        kDispatching +
        Commands{
            Command::setBindGroup,
            Command::dispatchWorkgroups,
            Command::finish};
  }

  CHECK(!scene.ctx.hasOpenPasses());
  CHECK(scene.passCommands() == commands);
  CHECK(scene.replayOn(target) == expected);
}

TEST_CASE(
    "any command on an outer pass finishes the pass inside it",
    "[ore][compute]")
{
  enum class Outer
  {
    draw,
    drawBindGroup,
    drawOverDraw,
    drawVertexBuffer,
    drawIndexBuffer,
    drawPipeline,
    computePipeline,
    computeBindGroup,
    computeDispatch,
  };
  for (Outer command :
       {Outer::draw,
        Outer::drawBindGroup,
        Outer::drawOverDraw,
        Outer::drawVertexBuffer,
        Outer::drawIndexBuffer,
        Outer::drawPipeline,
        Outer::computePipeline,
        Outer::computeBindGroup,
        Outer::computeDispatch})
  {
    INFO("command " << static_cast<int>(command));
    RecordedCompute scene;
    BufferDesc vertexDesc;
    vertexDesc.usage = BufferUsage::vertex;
    vertexDesc.size = 16;
    rcp<Buffer> buffer = make_rcp<StubBuffer>(vertexDesc);
    rcp<Pipeline> drawPipeline = devicePipeline(scene.shader, 1);
    const bool outerDraws = command < Outer::computePipeline;
    std::unique_ptr<RenderPass> draw;
    std::unique_ptr<RenderPass> innerDraw;
    std::unique_ptr<ComputePass> compute;
    Pass* inner = nullptr;
    if (outerDraws)
    {
      draw = scene.ctx.beginRenderPass(RenderPassDesc{});
      if (command == Outer::drawOverDraw)
        inner = (innerDraw = scene.ctx.beginRenderPass({})).get();
      else
        inner = (compute = scene.ctx.beginComputePass({})).get();
    }
    else
    {
      compute = scene.ctx.beginComputePass({});
      if (command == Outer::computeDispatch)
        compute->setPipeline(scene.pipeline.get());
      inner = (draw = scene.ctx.beginRenderPass(RenderPassDesc{})).get();
    }
    REQUIRE(inner != nullptr);
    switch (command)
    {
      case Outer::draw:
      case Outer::drawOverDraw:
        draw->draw(3);
        break;
      case Outer::drawBindGroup:
        draw->setBindGroup(0, scene.bindGroup.get());
        break;
      case Outer::drawVertexBuffer:
        draw->setVertexBuffer(0, buffer.get());
        break;
      case Outer::drawIndexBuffer:
        draw->setIndexBuffer(buffer.get(), IndexFormat::uint16);
        break;
      case Outer::drawPipeline:
        draw->setPipeline(drawPipeline.get());
        break;
      case Outer::computePipeline:
        compute->setPipeline(scene.pipeline.get());
        break;
      case Outer::computeBindGroup:
        compute->setBindGroup(0, scene.bindGroup.get());
        break;
      case Outer::computeDispatch:
        compute->dispatchWorkgroups(1);
        break;
    }
    INFO(scene.ctx.lastError());
    CHECK(scene.ctx.lastError().empty());
    CHECK(inner->isFinished());

    // The inner pass sits whole ahead of the outer one's begin.
    const Commands recorded = scene.passCommands();
    REQUIRE(recorded.size() >= 4);
    CHECK(recorded[1] == Command::finish);
    CHECK(
        recorded[2] ==
        (outerDraws ? Command::beginRenderPass : Command::beginComputePass));
  }
}

TEST_CASE(
    "a recorded pass on a device keeps its own commands",
    "[ore][compute]")
{
  // Each pass replays from its own stream there, so two open at once take
  // commands in turn.
  Shader shader;
  ComputeTarget target;
  ComputePipelineDesc desc;
  desc.module = &shader;
  desc.entryPoint = "cs_empty";
  rcp<ComputePipeline> pipeline = target.makeComputePipeline(desc);
  REQUIRE(pipeline != nullptr);

  auto outerDraw = ore::cmd::beginRecordedRenderPass(target, RenderPassDesc{});
  auto innerDraw = ore::cmd::beginRecordedRenderPass(target, RenderPassDesc{});
  outerDraw->draw(3);
  CHECK(!innerDraw->isFinished());
  innerDraw->finish();
  outerDraw->finish();
  CHECK(target.calls == Calls{"render", "render"});
  target.calls.clear();

  auto outer = ore::cmd::beginRecordedComputePass(target, {});
  auto inner = ore::cmd::beginRecordedComputePass(target, {});
  outer->setPipeline(pipeline.get());
  outer->dispatchWorkgroups(1);
  CHECK(!inner->isFinished());
  inner->setPipeline(pipeline.get());
  inner->dispatchWorkgroups(2);
  inner->finish();
  outer->dispatchWorkgroups(3);
  outer->finish();
  CHECK(
      target.calls == Calls{
                          "begin",
                          "pipeline cs_empty",
                          "dispatch 2 1 1",
                          "end",
                          "begin",
                          "pipeline cs_empty",
                          "dispatch 1 1 1",
                          "dispatch 3 1 1",
                          "end"});
}

TEST_CASE(
    "a command ends the pass begun inside its own on their stream, whatever "
    "other streams' passes are open",
    "[ore][compute]")
{
  ComputeTarget target;
  target.setDeferredRecording(true);
  // Recorded into the frame's stream, or replayed from a stream of its own
  // as it finishes.
  auto begin = [&](bool frame) {
    target.frameReplay = frame;
    return ore::cmd::beginRecordedRenderPass(target, RenderPassDesc{});
  };
  auto outer = begin(true);

  SECTION("above both")
  {
    auto inner = begin(true);
    auto apart = begin(false);
    outer->draw(3);
    CHECK(inner->isFinished());
    // Begun inside the inner pass, it ends with it.
    CHECK(apart->isFinished());
  }

  SECTION("between them")
  {
    auto apart = begin(false);
    auto inner = begin(true);
    outer->draw(3);
    CHECK(inner->isFinished());
    // Begun before the inner pass, it is not that pass's to end.
    CHECK(!apart->isFinished());
    CHECK(target.calls.empty());
    apart->finish();
  }

  CHECK(target.calls == Calls{"render"});
  outer->finish();
  CHECK(!target.hasOpenPasses());
  // The draw stayed in the outer pass, which runs after the inner one.
  CHECK(
      passCommandsIn(target.pendingFrame()) == Commands{
                                                   Command::beginRenderPass,
                                                   Command::finish,
                                                   Command::beginRenderPass,
                                                   Command::draw,
                                                   Command::finish});
}

TEST_CASE("a compute pass left open is finished for it", "[ore][compute]")
{
  RecordedCompute scene;
  const uint64_t token = scene.ctx.nextPassToken();
  auto pass = scene.dispatching(4);
  CHECK(scene.ctx.hasOpenPasses());
  CHECK(scene.ctx.finishOpenPassesFrom(token) == 1);
  CHECK(pass->isFinished());
  CHECK(!scene.ctx.hasOpenPasses());
  pass.reset();
  CHECK(scene.recorded(ore::cmd::CommandType::beginComputePass) == 1);
  CHECK(scene.recorded(ore::cmd::CommandType::finish) == 1);

  // One whose recorder died first has nowhere left to record.
  std::unique_ptr<ComputePass> orphan;
  {
    ore::cmd::DeferredOreContext gone{nullptr};
    orphan = gone.beginComputePass({});
    REQUIRE(orphan != nullptr);
  }
  CHECK(orphan->isFinished());
}

TEST_CASE(
    "a recorded storage buffer takes no update while a pass binds it",
    "[ore][compute]")
{
  RecordedCompute scene;
  const uint32_t value = 7;
  auto updateStorage = [&] {
    scene.ctx.clearLastError();
    scene.storage->update(&value, sizeof(value), 0);
    return scene.ctx.lastError();
  };
  const size_t before = scene.recorded(ore::cmd::CommandType::bufferUpdate);
  const int32_t refs = scene.storage->debugging_refcnt();

  auto compute = scene.dispatching(4);
  CHECK(contains(updateStorage(), "while a pass that binds it is open"));
  // A uniform's update reaches the pass on every device.
  scene.uniform->update(&value, sizeof(value), 0);
  compute->finish();
  CHECK(scene.recorded(ore::cmd::CommandType::bufferUpdate) == before + 1);
  CHECK(updateStorage().empty());

  // The pass that bound it holds it through one begun inside, and through
  // the group being replaced.
  auto draw = scene.ctx.beginRenderPass(RenderPassDesc{});
  draw->setBindGroup(0, scene.bindGroup.get());
  BindGroupDesc::StorageEntry spare = scene.bound;
  BufferDesc spareDesc;
  spareDesc.usage = BufferUsage::storage;
  spareDesc.size = 1024;
  rcp<Buffer> spareBuffer = scene.ctx.makeBuffer(spareDesc);
  spare.buffer = spareBuffer.get();
  BindGroupDesc spareGroupDesc = scene.desc;
  spareGroupDesc.storageBuffers = &spare;
  rcp<BindGroup> spareGroup = scene.ctx.makeBindGroup(spareGroupDesc);
  REQUIRE(spareGroup != nullptr);
  draw->setBindGroup(0, spareGroup.get());
  auto inner = scene.ctx.beginComputePass({});
  CHECK(contains(updateStorage(), "while a pass that binds it is open"));
  inner->finish();
  CHECK(contains(updateStorage(), "while a pass that binds it is open"));
  draw->finish();

  // Finished, a pass holds nothing.
  CHECK(scene.storage->debugging_refcnt() == refs);
  CHECK(updateStorage().empty());
  CHECK(scene.recorded(ore::cmd::CommandType::bufferUpdate) == before + 3);
}

// A nested script cannot see the passes open around it, so its update of a
// buffer of its own must not depend on them.
TEST_CASE(
    "a storage update runs ahead of the open passes that leave its buffer be",
    "[ore][compute]")
{
  RecordedCompute scene;
  BufferDesc otherDesc;
  otherDesc.usage = BufferUsage::storage;
  otherDesc.size = 64;
  rcp<Buffer> other = scene.ctx.makeBuffer(otherDesc);
  const uint32_t value = 7;
  Commands commands;
  Calls calls;

  SECTION("inside one pass")
  {
    auto compute = scene.dispatching(4);
    other->update(&value, sizeof(value), 0);
    compute->dispatchWorkgroups(5);
    compute->finish();
    commands = Commands{Command::bufferUpdate} + kDispatching +
               Commands{Command::dispatchWorkgroups, Command::finish};
    calls = {
        "copy 64:7",
        "begin",
        "pipeline cs_main",
        "group 0",
        "dispatch 4 1 1",
        "dispatch 5 1 1",
        "end"};
  }

  SECTION("inside a pass begun inside another")
  {
    auto draw = scene.ctx.beginRenderPass(RenderPassDesc{});
    draw->draw(3);
    auto compute = scene.dispatching(4);
    other->update(&value, sizeof(value), 0);
    compute->finish();
    draw->draw(3);
    draw->finish();
    commands = Commands{Command::bufferUpdate} + kDispatching +
               Commands{
                   Command::finish,
                   Command::beginRenderPass,
                   Command::draw,
                   Command::draw,
                   Command::finish};
    calls = Calls{"copy 64:7"} + kDispatched4 + Calls{"render"};
  }

  // The pass that read the buffer ran its script's commands first, so it
  // still runs first, however many passes are open around both.
  SECTION("after a pass that finished inside the inner of two")
  {
    auto outer = scene.ctx.beginRenderPass(RenderPassDesc{});
    auto inner = scene.ctx.beginRenderPass(RenderPassDesc{});
    scene.dispatching(4)->finish();
    scene.storage->update(&value, sizeof(value), 0);
    inner->finish();
    outer->finish();
    commands = kDispatching + Commands{
                                  Command::finish,
                                  Command::bufferUpdate,
                                  Command::beginRenderPass,
                                  Command::finish,
                                  Command::beginRenderPass,
                                  Command::finish};
    calls = kDispatched4 + Calls{"copy 1024:7", "render", "render"};
  }

  // The buffer a later pass binds is updated before that pass too.
  SECTION("ahead of a pass begun after it inside the same pass")
  {
    auto draw = scene.ctx.beginRenderPass(RenderPassDesc{});
    draw->draw(3);
    scene.storage->update(&value, sizeof(value), 0);
    scene.dispatching(4)->finish();
    draw->finish();
    commands = Commands{Command::bufferUpdate} + kDispatching +
               Commands{
                   Command::finish,
                   Command::beginRenderPass,
                   Command::draw,
                   Command::finish};
    calls = Calls{"copy 1024:7"} + kDispatched4 + Calls{"render"};
  }

  // The pass's own uniform update keeps its place between the draws.
  SECTION("alone")
  {
    auto draw = scene.ctx.beginRenderPass(RenderPassDesc{});
    draw->draw(3);
    scene.uniform->update(&value, sizeof(value), 0);
    other->update(&value, sizeof(value), 0);
    draw->draw(3);
    draw->finish();
    commands = {
        Command::bufferUpdate,
        Command::beginRenderPass,
        Command::draw,
        Command::bufferUpdate,
        Command::draw,
        Command::finish};
    calls = {"copy 64:7", "render"};
  }

  // Another buffer made inside the pass is no reason to move more.
  SECTION("alone past a buffer made inside the pass")
  {
    auto draw = scene.ctx.beginRenderPass(RenderPassDesc{});
    draw->draw(3);
    scene.uniform->update(&value, sizeof(value), 0);
    rcp<Buffer> late = scene.ctx.makeBuffer(otherDesc);
    other->update(&value, sizeof(value), 0);
    draw->finish();
    commands = {
        Command::bufferUpdate,
        Command::beginRenderPass,
        Command::draw,
        Command::bufferUpdate,
        Command::finish};
    calls = {"copy 64:7", "render"};
  }

  CHECK(scene.ctx.lastError().empty());
  CHECK(scene.passCommands({Command::bufferUpdate}) == commands);
  ComputeTarget target;
  CHECK(scene.replayOn(target) == calls);
  CHECK(target.lastError().empty());
}

TEST_CASE(
    "a storage buffer made inside a pass is made and updated ahead of it",
    "[ore][compute]")
{
  RecordedCompute scene;
  const Commands also = {Command::makeBuffer, Command::bufferUpdate};
  const Commands made = scene.passCommands(also);
  const uint32_t value = 7;
  BufferDesc lateDesc;
  lateDesc.usage = BufferUsage::storage;
  lateDesc.size = 64;
  Commands commands;
  Calls calls;

  SECTION("inside the inner of two passes")
  {
    auto draw = scene.ctx.beginRenderPass(RenderPassDesc{});
    draw->draw(3);
    auto compute = scene.ctx.beginComputePass({});
    rcp<Buffer> late = scene.ctx.makeBuffer(lateDesc);
    late->update(&value, sizeof(value), 0);
    compute->finish();
    draw->draw(3);
    draw->finish();
    commands = {
        Command::makeBuffer,
        Command::bufferUpdate,
        Command::beginComputePass,
        Command::finish,
        Command::beginRenderPass,
        Command::draw,
        Command::draw,
        Command::finish};
    calls = {"copy 64:7", "begin", "end", "render"};
  }

  SECTION("inside the outer pass, updated inside the inner")
  {
    auto draw = scene.ctx.beginRenderPass(RenderPassDesc{});
    rcp<Buffer> late = scene.ctx.makeBuffer(lateDesc);
    auto compute = scene.ctx.beginComputePass({});
    late->update(&value, sizeof(value), 0);
    compute->finish();
    draw->finish();
    commands = {
        Command::makeBuffer,
        Command::bufferUpdate,
        Command::beginComputePass,
        Command::finish,
        Command::beginRenderPass,
        Command::finish};
    calls = {"copy 64:7", "begin", "end", "render"};
  }

  SECTION("inside the middle of three passes, updated inside the inner")
  {
    auto outer = scene.ctx.beginRenderPass(RenderPassDesc{});
    auto middle = scene.ctx.beginRenderPass(RenderPassDesc{});
    rcp<Buffer> late = scene.ctx.makeBuffer(lateDesc);
    auto compute = scene.ctx.beginComputePass({});
    late->update(&value, sizeof(value), 0);
    compute->finish();
    middle->finish();
    outer->finish();
    commands = {
        Command::makeBuffer,
        Command::bufferUpdate,
        Command::beginComputePass,
        Command::finish,
        Command::beginRenderPass,
        Command::finish,
        Command::beginRenderPass,
        Command::finish};
    calls = {"copy 64:7", "begin", "end", "render", "render"};
  }

  // As when a nested pass finishes there, the pass's uniform updates so far
  // go ahead with the make.
  SECTION("after a uniform update of the pass it was made inside")
  {
    auto draw = scene.ctx.beginRenderPass(RenderPassDesc{});
    draw->draw(3);
    scene.uniform->update(&value, sizeof(value), 0);
    rcp<Buffer> late = scene.ctx.makeBuffer(lateDesc);
    late->update(&value, sizeof(value), 0);
    draw->draw(3);
    draw->finish();
    commands = {
        Command::bufferUpdate,
        Command::makeBuffer,
        Command::bufferUpdate,
        Command::beginRenderPass,
        Command::draw,
        Command::draw,
        Command::finish};
    calls = {"copy 64:7", "render"};
  }

  // Once its make is ahead of the pass, a copy moves alone again, and one of
  // a buffer made before the pass follows it.
  SECTION("then updated again")
  {
    BufferDesc otherDesc = lateDesc;
    otherDesc.size = 128;
    rcp<Buffer> other = scene.ctx.makeBuffer(otherDesc);
    auto draw = scene.ctx.beginRenderPass(RenderPassDesc{});
    draw->draw(3);
    rcp<Buffer> late = scene.ctx.makeBuffer(lateDesc);
    late->update(&value, sizeof(value), 0);
    scene.uniform->update(&value, sizeof(value), 0);
    draw->draw(3);
    const uint32_t next = 8;
    late->update(&next, sizeof(next), 0);
    other->update(&value, sizeof(value), 0);
    draw->finish();
    commands = {
        Command::makeBuffer,
        Command::makeBuffer,
        Command::bufferUpdate,
        Command::bufferUpdate,
        Command::bufferUpdate,
        Command::beginRenderPass,
        Command::draw,
        Command::bufferUpdate,
        Command::draw,
        Command::finish};
    calls = {"copy 64:7", "copy 64:8", "copy 128:7", "render"};
  }

  CHECK(scene.ctx.lastError().empty());
  CHECK(scene.passCommands(also) == made + commands);
  ComputeTarget target;
  CHECK(scene.replayOn(target) == calls);
  CHECK(target.lastError().empty());
}

namespace
{
// Shader's bindings, with a compute entry that reads 64 bytes of its storage
// buffer.
struct SizedStorageShader : Shader
{
  SizedStorageShader()
  {
    BindingMap map;
    map.push(row(0, ResourceKind::UniformBuffer, kAll, 0));
    BindingMap::Entry storage = row(
        1,
        ResourceKind::StorageBufferRW,
        BindingMap::kStageFragment | BindingMap::kStageCompute,
        1);
    storage.minBindingSize = 64;
    map.push(storage);
    map.push(
        row(2, ResourceKind::StorageBufferRO, BindingMap::kStageFragment, 2));
    map.finalize();
    map.computeLayoutIds();
    m_bindingMap = map;
  }
};
} // namespace

// A bind is checked against the layout alone, so the layout has to ask for
// what the shader reads.
TEST_CASE(
    "a pipeline refuses a layout that asks less of a storage buffer than its "
    "shader reads",
    "[ore][compute]")
{
  SizedStorageShader shader;
  ComputeContext ctx;
  BindGroupLayoutEntry entries[2]{};
  REQUIRE(
      populateBindGroupLayoutEntries(
          entries,
          2,
          bindingMapForComputeEntry(&shader, "cs_main"),
          0) == 2);
  CHECK(entries[1].minBindingSize == 64);
  auto refusalWith = [&](uint32_t minBindingSize) {
    entries[1].minBindingSize = minBindingSize;
    BindGroupLayoutDesc layoutDesc;
    layoutDesc.entries = entries;
    layoutDesc.entryCount = 2;
    rcp<BindGroupLayout> layout = make_rcp<StubLayout>(layoutDesc);
    BindGroupLayout* layouts[] = {layout.get()};
    ComputePipelineDesc desc;
    desc.module = &shader;
    desc.entryPoint = "cs_main";
    desc.bindGroupLayouts = layouts;
    desc.bindGroupLayoutCount = 1;
    return refusal(ctx, desc);
  };
  CHECK(refusalWith(64).empty());
  CHECK(refusalWith(128).empty());
  // A uniform block's minimum is held as it was, at the bind alone.
  entries[0].minBindingSize = 0;
  CHECK(refusalWith(64).empty());
  CHECK(contains(
      refusalWith(0),
      "layout's minBindingSize 0 is under the 64 bytes"));
  CHECK(contains(
      refusalWith(60),
      "layout's minBindingSize 60 is under the 64 bytes"));
}

TEST_CASE(
    "a recorded group made without a layout binds with an empty storage entry",
    "[ore][compute]")
{
  ore::cmd::DeferredOreContext ctx{nullptr};
  BindGroupDesc::StorageEntry empty{};
  BindGroupDesc desc;
  desc.storageBuffers = &empty;
  desc.storageBufferCount = 1;
  rcp<BindGroup> group = ctx.makeBindGroup(desc);
  REQUIRE(group != nullptr);
  auto pass = ctx.beginRenderPass(RenderPassDesc{});
  pass->setBindGroup(0, group.get());
  pass->finish();
  CHECK(!ctx.hasOpenPasses());
}

TEST_CASE(
    "a recording refuses a pass for a device without compute",
    "[ore][compute]")
{
  ReplayCaps caps;
  caps.featuresKnown = true;
  ore::cmd::DeferredOreContext without(caps);
  std::string error;
  CHECK(without.beginComputePass({}, &error) == nullptr);
  CHECK(contains(error, "does not run compute shaders"));
  CHECK(!without.hasOpenPasses());
  CHECK(without.stream().empty());
}

TEST_CASE("a script's compute pass on a device is recorded", "[ore][compute]")
{
  Shader shader;
  ComputeTarget target;
  ComputePipelineDesc desc;
  desc.module = &shader;
  desc.entryPoint = "cs_empty";
  rcp<ComputePipeline> pipeline = target.makeComputePipeline(desc);
  REQUIRE(pipeline != nullptr);
  const Calls expected =
      {"begin", "pipeline cs_empty", "dispatch 3 1 1", "end"};

  SECTION("replayed when it finishes")
  {
    auto pass = ore::cmd::beginRecordedComputePass(target, {});
    REQUIRE(pass != nullptr);
    pass->setPipeline(pipeline.get());
    pass->dispatchWorkgroups(3);
    CHECK(target.calls.empty());
    pass->finish();
    CHECK(target.calls == expected);
  }

  SECTION("replayed with the frame")
  {
    target.frameReplay = true;
    target.setDeferredRecording(true);
    auto pass = ore::cmd::beginRecordedComputePass(target, {});
    REQUIRE(pass != nullptr);
    pass->setPipeline(pipeline.get());
    pass->dispatchWorkgroups(3);
    pass->finish();
    CHECK(target.calls.empty());
    ore::cmd::replayCommandBuffer(target, target.pendingFrame());
    CHECK(target.calls == expected);
  }

  SECTION("refused without compute")
  {
    ComputeContext without;
    without.editableFeatures().computeShaders = false;
    std::string error;
    CHECK(ore::cmd::beginRecordedComputePass(without, {}, &error) == nullptr);
    CHECK(contains(error, "does not run compute shaders"));
  }
}

TEST_CASE("a compute stream's silver tells dispatches apart", "[ore][compute]")
{
  Shader shader;
  ComputeContext ctx;
  ComputePipelineDesc desc;
  desc.module = &shader;
  desc.entryPoint = "cs_empty";
  rcp<ComputePipeline> pipeline = ctx.makeComputePipeline(desc);
  REQUIRE(pipeline != nullptr);
  auto silver = [&](uint32_t count) {
    ore::cmd::OreCommandBuffer buffer;
    {
      ore::cmd::ComputePassRecording pass(&ctx, &buffer, {});
      pass.setPipeline(pipeline.get());
      pass.dispatchWorkgroups(count);
    }
    std::vector<uint8_t> out;
    ore::cmd::serializeSilver(buffer, out);
    return out;
  };
  CHECK(ore::cmd::silverMatch(silver(4), silver(4)));
  CHECK(!ore::cmd::silverMatch(silver(4), silver(5)));
}
