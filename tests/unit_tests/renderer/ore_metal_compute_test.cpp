/*
 * Copyright 2026 Rive
 *
 * A compute pass writes a storage buffer and a draw reads it back, so what the
 * kernel computed is checked as pixels. From Metal source written here, from
 * the WGSL the scripting workspace baked as an exporter does, and through each
 * way a script's passes are recorded and replayed.
 */

#include "ore_test_backends.hpp"

#if defined(ORE_BACKEND_METAL) || defined(ORE_BACKEND_VK) ||                   \
    defined(ORE_BACKEND_D3D11) || defined(ORE_BACKEND_D3D12)
#include "rive/renderer/render_context.hpp"
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/ore/cmd/ore_deferred_compute_pass.hpp"
#include "rive/renderer/ore/cmd/ore_deferred_context.hpp"
#include "rive/renderer/ore/ore_bind_group_layout.hpp"
#include "rive/renderer/ore/ore_binding_map.hpp"
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_context.hpp"
#include "rive/renderer/ore/ore_render_pass.hpp"
#include "rive/renderer/ore/ore_rstb_entry_bindings.hpp"
#include "rive/renderer/ore/ore_rstb_workgroups.hpp"
#include "rive/renderer/ore/ore_shader_asset_modules.hpp"
#include "rive/renderer/ore/ore_shader_module.hpp"
#include "ore_compute_shaders.rstb.hpp"
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace rive;
using namespace rive::gpu;
using namespace rive::ore;

namespace
{
#if defined(ORE_BACKEND_METAL)
constexpr TestingWindow::Backend kMetal[] = {TestingWindow::Backend::metal};
#endif
#if defined(ORE_BACKEND_VK)
constexpr TestingWindow::Backend kVulkan[] = {TestingWindow::Backend::vk};
#endif
#if defined(ORE_BACKEND_D3D11)
constexpr TestingWindow::Backend kD3D11[] = {TestingWindow::Backend::d3d};
#endif
#if defined(ORE_BACKEND_D3D12)
constexpr TestingWindow::Backend kD3D12[] = {TestingWindow::Backend::d3d12};
#endif

#if defined(ORE_BACKEND_METAL)
// cs_main scales 64 values into `output` by way of a workgroup variable, which
// reads back zero unless Metal was given its size; fs_show paints three.
const char kMSL[] = R"(
#include <metal_stdlib>
using namespace metal;

struct Params { uint scale; };
struct Values { uint v[64]; };
struct Scratch { uint v[8]; };
struct VertexOut { float4 position [[position]]; };

kernel void cs_main(uint3 id [[thread_position_in_grid]],
                    uint3 local [[thread_position_in_threadgroup]],
                    constant Params& params [[buffer(0)]],
                    const device Values& input [[buffer(1)]],
                    device Values& output [[buffer(2)]],
                    threadgroup Scratch& scratch) {
    scratch.v[local.x] = input.v[id.x];
    threadgroup_barrier(mem_flags::mem_threadgroup);
    output.v[id.x] = scratch.v[local.x] * params.scale + 1u;
}

vertex VertexOut vs_cover(uint vid [[vertex_id]]) {
    const float2 positions[3] = {
        float2(-1.0, -1.0), float2(3.0, -1.0), float2(-1.0, 3.0)};
    VertexOut out;
    out.position = float4(positions[vid], 0.0, 1.0);
    return out;
}

fragment float4 fs_show(VertexOut in [[stage_in]],
                        const device Values& output [[buffer(2)]]) {
    return float4(float(output.v[5]) / 255.0,
                  float(output.v[6]) / 255.0,
                  float(output.v[63]) / 255.0,
                  1.0);
}
)";
#endif

constexpr int kSize = 32;
constexpr uint32_t kCount = 64;
constexpr uint32_t kBytes = kCount * sizeof(uint32_t);
#if defined(ORE_BACKEND_METAL)
constexpr uint8_t kCompute = BindingMap::kStageCompute;
constexpr uint8_t kFragment = BindingMap::kStageFragment;

BindingMap::Entry row(
    uint8_t binding,
    ResourceKind kind,
    uint8_t stageMask,
    uint32_t minBindingSize)
{
  BindingMap::Entry e{};
  e.binding = binding;
  e.kind = kind;
  e.stageMask = stageMask;
  for (int stage = 0; stage < 3; ++stage)
  {
    e.backendSlot[stage] = (stageMask & (1u << stage)) != 0
                               ? binding
                               : BindingMap::kAbsent;
  }
  e.minBindingSize = minBindingSize;
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
#endif

// A module and the names its source gave the three entry points.
struct Shader
{
  // One module per entry point of `entries`, the same one for all of them on
  // a target whose entry points share a module.
  std::vector<rcp<ShaderModule>> modules;
  std::string compute = "cs_main";
  std::string vertex = "vs_cover";
  std::string fragment = "fs_show";
  // Every entry point as written and as the bake named it.
  std::vector<std::pair<std::string, std::string>> entries;

  std::string physical(const char* logical) const
  {
    for (const auto& entry : entries)
    {
      if (entry.first == logical)
        return entry.second;
    }
    FAIL("the shader has no entry point " << logical);
    return {};
  }

  // The module that holds the entry point written as `logical`. HLSL names
  // every entry point main, so the bake's names cannot tell them apart.
  ShaderModule* moduleFor(const char* logical) const
  {
    for (size_t i = 0; i < entries.size(); ++i)
    {
      if (entries[i].first == logical)
        return modules[i].get();
    }
    return modules.empty() ? nullptr : modules[0].get();
  }

  // Where `compute`, `vertex` and `fragment` sit among the entry points.
  size_t computeIndex = 0;
  size_t vertexIndex = 0;
  size_t fragmentIndex = 0;
  ShaderModule* moduleAt(size_t index) const
  {
    return modules.empty() ? nullptr
                           : modules[index < modules.size() ? index : 0].get();
  }
  ShaderModule* computeModule() const { return moduleAt(computeIndex); }
  ShaderModule* vertexModule() const { return moduleAt(vertexIndex); }
  ShaderModule* fragmentModule() const { return moduleAt(fragmentIndex); }
};

#if defined(ORE_BACKEND_METAL)
Shader handWrittenShader(Context& ctx)
{
  BindingMap bindings;
  bindings.push(row(0, ResourceKind::UniformBuffer, kCompute, 4));
  bindings.push(row(1, ResourceKind::StorageBufferRO, kCompute, kBytes));
  bindings.push(
      row(2, ResourceKind::StorageBufferRW, kFragment | kCompute, kBytes));
  bindings.finalize();
  const std::vector<uint8_t> map = bindings.toBlob();
  const std::vector<uint8_t> entries = encodeEntryBindings(
      {uses(0, "vs_cover", {}),
       uses(1, "fs_show", {2}),
       uses(2, "cs_main", {0, 1, 2})});
  RstbWorkgroup workgroup;
  workgroup.name = "cs_main";
  workgroup.size[0] = 8;
  workgroup.memory = {8 * sizeof(uint32_t)};
  const std::vector<uint8_t> workgroups = encodeWorkgroups({workgroup});

  ShaderModuleDesc desc{};
  desc.code = kMSL;
  desc.codeSize = sizeof(kMSL) - 1;
  desc.bindingMapBytes = map.data();
  desc.bindingMapSize = static_cast<uint32_t>(map.size());
  desc.entryBindingBytes = entries.data();
  desc.entryBindingSize = static_cast<uint32_t>(entries.size());
  desc.workgroupBytes = workgroups.data();
  desc.workgroupBytesSize = static_cast<uint32_t>(workgroups.size());
  Shader shader;
  shader.modules = {ctx.makeShaderModule(desc)};
  return shader;
}
#endif

// Shader `index` of ore_compute_shaders.rstb.hpp, baked from WGSL.
Shader bakedShaderAt(Context& ctx, size_t index)
{
  const uint32_t* offsets = ore_compute_shaders::kShaderOffsets;
  ShaderAsset asset;
  REQUIRE(decodeBareRstb(
      asset,
      ore_compute_shaders::kShaderData + offsets[index],
      offsets[index + 1] - offsets[index]));
  Shader shader;
  const bool visited = visitShaderAssetModules(
      ctx.shaderTarget(),
      asset,
      [&](const ShaderModuleDesc& desc,
          uint32_t first,
          uint32_t count,
          const std::vector<RstbEntryView>& entries) {
        if (shader.entries.empty())
        {
          for (size_t i = 0; i < entries.size(); ++i)
          {
            const RstbEntryView& entry = entries[i];
            const bool compute = entry.stage == 2;
            const bool vertex = entry.stage == 0;
            (compute  ? shader.compute
             : vertex ? shader.vertex
                      : shader.fragment) = entry.physical;
            (compute  ? shader.computeIndex
             : vertex ? shader.vertexIndex
                      : shader.fragmentIndex) = i;
            shader.entries.emplace_back(entry.logical, entry.physical);
          }
          shader.modules.resize(entries.size());
        }
        // Direct3D compiles each entry point as a module of its own.
        rcp<ShaderModule> module = ctx.makeShaderModule(desc);
        for (uint32_t i = first; i < first + count; ++i)
          shader.modules[i] = module;
        return true;
      });
  REQUIRE(visited);
  return shader;
}

// The shader of kMSL, as WGSL.
Shader bakedShader(Context& ctx) { return bakedShaderAt(ctx, 0); }

// A compute pipeline and the layout of what its entry point binds.
rcp<ComputePipeline> computePipelineFor(
    Context& make,
    ShaderModule* module,
    const std::string& entry,
    rcp<BindGroupLayout>& layout)
{
  layout = makeBindGroupLayoutFromBindingMap(
      make,
      bindingMapForComputeEntry(module, entry.c_str()),
      0);
  REQUIRE(layout != nullptr);
  BindGroupLayout* layouts[] = {layout.get()};
  ComputePipelineDesc desc;
  desc.module = module;
  desc.entryPoint = entry.c_str();
  desc.bindGroupLayouts = layouts;
  desc.bindGroupLayoutCount = 1;
  std::string error;
  rcp<ComputePipeline> pipeline = make.makeComputePipeline(desc, &error);
  INFO("makeComputePipeline: " << error);
  REQUIRE(pipeline != nullptr);
  return pipeline;
}

// A draw of one triangle list into rgba8, and the layouts of what its entry
// points bind in the first `layoutCount` groups.
rcp<Pipeline> drawPipelineFor(
    Context& make,
    ShaderModule* vertexModule,
    const std::string& vertex,
    ShaderModule* fragmentModule,
    const std::string& fragment,
    rcp<BindGroupLayout>* outLayouts,
    uint32_t layoutCount)
{
  BindGroupLayout* layouts[kMaxBindGroups] = {};
  REQUIRE(layoutCount <= kMaxBindGroups);
  for (uint32_t g = 0; g < layoutCount; ++g)
  {
    outLayouts[g] = makeBindGroupLayoutFromBindingMap(
        make,
        bindingMapForEntries(
            vertexModule,
            vertex.c_str(),
            fragmentModule,
            fragment.c_str()),
        g);
    REQUIRE(outLayouts[g] != nullptr);
    layouts[g] = outLayouts[g].get();
  }
  PipelineDesc desc{};
  desc.vertexModule = vertexModule;
  desc.fragmentModule = fragmentModule;
  desc.vertexEntryPoint = vertex.c_str();
  desc.fragmentEntryPoint = fragment.c_str();
  desc.topology = PrimitiveTopology::triangleList;
  desc.colorTargets[0].format = TextureFormat::rgba8unorm;
  desc.colorCount = 1;
  desc.bindGroupLayouts = layouts;
  desc.bindGroupLayoutCount = layoutCount;
  std::string error;
  rcp<Pipeline> pipeline = make.makePipeline(desc, &error);
  INFO("makePipeline: " << error);
  REQUIRE(pipeline != nullptr);
  return pipeline;
}

rcp<Pipeline> drawPipelineFor(
    Context& make,
    ShaderModule* vertexModule,
    const std::string& vertex,
    ShaderModule* fragmentModule,
    const std::string& fragment,
    rcp<BindGroupLayout>& layout)
{
  return drawPipelineFor(
      make,
      vertexModule,
      vertex,
      fragmentModule,
      fragment,
      &layout,
      1);
}

RenderPassDesc clearInto(TextureView* target)
{
  RenderPassDesc desc{};
  desc.colorAttachments[0].view = target;
  desc.colorAttachments[0].loadOp = LoadOp::clear;
  desc.colorAttachments[0].storeOp = StoreOp::store;
  desc.colorAttachments[0].clearColor = {0.f, 0.f, 0.f, 1.f};
  desc.colorCount = 1;
  return desc;
}

void beginOreFrame(
    TestingWindow* window,
    ore_gm::OreGMContext& oreGM,
    RenderContext* renderContext)
{
  window->beginFrame({
      .name = "ore_metal_compute",
      .clearColor = 0xff000000,
      .doClear = true,
  });
  oreGM.beginFrame(renderContext);
}

// Draws `canvas` to the window and checks its center pixel.
void expectCenterPixel(
    TestingWindow* window,
    RenderCanvas* canvas,
    const uint8_t (&rgb)[3])
{
  auto renderer = window->beginFrame({
      .name = "ore_metal_compute_read",
      .clearColor = 0xff000000,
      .doClear = true,
  });
  renderer->drawImage(
      canvas->renderImage(),
      {.filter = ImageFilter::nearest},
      BlendMode::srcOver,
      1);
  std::vector<uint8_t> px;
  window->endFrame(&px);
  REQUIRE(px.size() == static_cast<size_t>(kSize) * kSize * 4);
  const uint8_t*
      p = &px[(static_cast<size_t>(kSize / 2) * kSize + kSize / 2) * 4];
  INFO("center pixel = " << +p[0] << "," << +p[1] << "," << +p[2]);
  for (int c = 0; c < 3; ++c)
    CHECK(std::abs(p[c] - rgb[c]) <= 2);
}

rcp<Buffer> makeBuffer(
    Context& ctx,
    BufferUsage usage,
    const void* data,
    uint32_t size)
{
  BufferDesc desc{};
  desc.usage = usage;
  desc.size = size;
  desc.data = data;
  auto buffer = ctx.makeBuffer(desc);
  INFO("makeBuffer: " << ctx.lastError());
  REQUIRE(buffer != nullptr);
  return buffer;
}

// Only an old Android driver may keep compute off, and the tests skip there.
bool lacksCompute(Context& ctx)
{
  const Features& features = ctx.features();
  REQUIRE(features.computeShaders == features.storageBuffers);
#ifdef __ANDROID__
  if (!features.computeShaders)
  {
    WARN("this driver keeps compute and storage off; skipping");
    return true;
  }
#endif
  REQUIRE(features.computeShaders);
  return false;
}

// How the passes reach the device.
enum class Path
{
  immediate,
  // Made and drawn through a recording context, replayed each frame.
  recorder,
  // A script's entry points on the device: replayed as each pass finishes,
  inlineReplay,
  // or all together when the frame ends.
  frameReplay,
};

void computeIntoStorage(
    TestingWindow* window,
    Shader (*makeShader)(Context&),
    Path path = Path::immediate)
{
  window->resize(kSize, kSize);
  RenderContext* renderContext = window->renderContext();
  ore_gm::OreGMContext oreGM;
  REQUIRE(oreGM.ensureContext(renderContext));
  Context& ctx = *renderContext->getOreContext();
  if (lacksCompute(ctx))
    return;
  rcp<RenderCanvas> canvas = renderContext->makeRenderCanvas(kSize, kSize);
  REQUIRE(canvas != nullptr);
  rcp<TextureView> target = ctx.wrapCanvasTexture(canvas.get());
  REQUIRE(target != nullptr);

  ore::cmd::DeferredOreContext recorder(&ctx);
  ore::cmd::OreResident replayed;
  const bool recorded = path == Path::recorder;
  Context& make = recorded ? static_cast<Context&>(recorder) : ctx;
  ctx.setDeferredRecording(path == Path::frameReplay);

  const Shader shader = makeShader(make);
  REQUIRE(shader.computeModule() != nullptr);

  rcp<BindGroupLayout> computeLayout;
  rcp<ComputePipeline> computePipeline = computePipelineFor(
      make,
      shader.computeModule(),
      shader.compute,
      computeLayout);
  rcp<BindGroupLayout> drawLayout;
  rcp<Pipeline> drawPipeline = drawPipelineFor(
      make,
      shader.vertexModule(),
      shader.vertex,
      shader.fragmentModule(),
      shader.fragment,
      drawLayout);
  std::string error;

  uint32_t values[kCount];
  for (uint32_t i = 0; i < kCount; ++i)
    values[i] = i;
  const uint32_t scale = 3;
  rcp<Buffer> params = makeBuffer(
      make,
      BufferUsage::uniform,
      &scale,
      sizeof(scale));
  rcp<Buffer> input = makeBuffer(make, BufferUsage::storage, values, kBytes);
  rcp<Buffer> output = makeBuffer(make, BufferUsage::storage, nullptr, kBytes);

  const BindGroupDesc::UBOEntry uniforms[] = {{0, params.get(), 0, 0}};
  const BindGroupDesc::StorageEntry computeStorage[] = {
      {1, input.get(), 0, 0},
      {2, output.get(), 0, 0}};
  BindGroupDesc computeGroupDesc;
  computeGroupDesc.layout = computeLayout.get();
  computeGroupDesc.ubos = uniforms;
  computeGroupDesc.uboCount = 1;
  computeGroupDesc.storageBuffers = computeStorage;
  computeGroupDesc.storageBufferCount = 2;
  rcp<BindGroup> computeGroup = make.makeBindGroup(computeGroupDesc);
  INFO("makeBindGroup: " << make.lastError());
  REQUIRE(computeGroup != nullptr);

  const BindGroupDesc::StorageEntry drawStorage[] = {{2, output.get(), 0, 0}};
  BindGroupDesc drawGroupDesc;
  drawGroupDesc.layout = drawLayout.get();
  drawGroupDesc.storageBuffers = drawStorage;
  drawGroupDesc.storageBufferCount = 1;
  rcp<BindGroup> drawGroup = make.makeBindGroup(drawGroupDesc);
  INFO("makeBindGroup: " << make.lastError());
  REQUIRE(drawGroup != nullptr);

  // Outside a frame there is nothing to encode a pass into.
  CHECK(ctx.beginComputePass({}, &error) == nullptr);
  CHECK(error.find("no frame is open") != std::string::npos);
  error.clear();

  const RenderPassDesc passDesc = clearInto(target.get());
  // Direct3D 11 has no command buffer for a frame to open, so it draws.
  if (TestingWindow::backend() != TestingWindow::Backend::d3d)
  {
    CHECK(ctx.beginRenderPass(passDesc, &error) == nullptr);
    CHECK(error.find("no frame is open") != std::string::npos);
    error.clear();
  }

  // What an update of the fifth value writes.
  auto setFifth = [&](uint32_t value) {
    input->update(&value, sizeof(value), 5 * sizeof(uint32_t));
  };

  enum class Update
  {
    none,
    beforeBind,
    insidePass,
    afterPass,
  };

  auto beginCompute = [&]() -> std::unique_ptr<ComputePass> {
    switch (path)
    {
      case Path::immediate:
        return ctx.beginComputePass({}, &error);
      case Path::recorder:
        return recorder.beginComputePass({}, &error);
      default:
        return ore::cmd::beginRecordedComputePass(ctx, {}, &error);
    }
  };
  auto beginDraw = [&]() -> std::unique_ptr<RenderPass> {
    switch (path)
    {
      case Path::immediate:
        return ctx.beginRenderPass(passDesc);
      case Path::recorder:
        return recorder.beginRenderPass(passDesc);
      default:
        return ore::cmd::beginRecordedRenderPass(ctx, passDesc);
    }
  };

  // Computes, draws v[5], v[6] and v[63] of the result and checks the center
  // pixel. `update` sets the fifth value to 7 at that point of the frame, and
  // `nested` begins the draw's pass before the compute pass.
  auto expect =
      [&](const uint8_t(&rgb)[3], Update update, bool nested = false) {
        beginOreFrame(window, oreGM, renderContext);
        ctx.clearLastError();
        make.clearLastError();

        std::unique_ptr<RenderPass> pass;
        if (nested)
          pass = beginDraw();
        auto compute = beginCompute();
        INFO("beginComputePass: " << error);
        REQUIRE(compute != nullptr);
        if (update == Update::beforeBind)
        {
          // No open pass binds the buffer yet, so the copy runs ahead of them.
          setFifth(7);
          CHECK(make.lastError().empty());
        }
        compute->setPipeline(computePipeline.get());
        compute->setBindGroup(0, computeGroup.get());
        // A group at an index the pipeline has no layout for is left out.
        compute->setBindGroup(3, computeGroup.get());
        if (update == Update::insidePass)
        {
          // A device cannot encode a copy beside the pass, and a recorder
          // keeps one from running ahead of a pass that binds its buffer.
          setFifth(7);
          CHECK(
              make.lastError().find(
                  path == Path::recorder
                      ? "while a pass that binds it is open"
                      : "while a pass is open") != std::string::npos);
          make.clearLastError();
        }
        if (update == Update::insidePass && path == Path::immediate)
        {
          // Neither can a draw, which a recording moves ahead instead.
          CHECK(ctx.beginRenderPass(passDesc, &error) == nullptr);
          CHECK(error.find("another pass is still open") != std::string::npos);
        }
        compute->dispatchWorkgroups(kCount / 8);
        compute->finish();
        if (update == Update::afterPass)
          setFifth(7);
        INFO("compute: " << make.lastError());
        CHECK(make.lastError().empty());

        if (!nested)
          pass = beginDraw();
        REQUIRE(pass != nullptr);
        pass->setViewport(0, 0, kSize, kSize);
        pass->setPipeline(drawPipeline.get());
        pass->setBindGroup(0, drawGroup.get());
        if (path == Path::immediate)
        {
          CHECK(ctx.beginComputePass({}, &error) == nullptr);
          CHECK(error.find("another pass is still open") != std::string::npos);
        }
        pass->draw(3);
        pass->finish();
        if (recorded)
        {
          ore::cmd::replayOreStream(
              ctx,
              recorder.stream(),
              replayed,
              [&](ore::cmd::ResourceHandle h) {
                return recorder.realResources()[h & ore::cmd::kRealResourceMask]
                    .get();
              });
          recorder.resetFrame();
          INFO("replay: " << ctx.lastError());
          CHECK(ctx.lastError().empty());
        }
        oreGM.endFrame(renderContext);
        window->endFrame();
        expectCenterPixel(window, canvas.get(), rgb);
      };

  // n * 3 + 1 for 5, 6 and 63.
  const uint8_t kFirst[3] = {16, 19, 190};
  expect(kFirst, Update::none);

  // A write between frames lands before the next dispatch and leaves its
  // neighbor alone.
  setFifth(40);
  CHECK(ctx.lastError().empty());
  const uint8_t kForty[3] = {121, 19, 190};
  expect(kForty, Update::none);

  // A write after the pass is copied after it, where a write into the
  // buffer's memory would have reached the dispatch still waiting to run.
  expect(kForty, Update::afterPass);
  const uint8_t kSeven[3] = {22, 19, 190};
  expect(kSeven, Update::none);

  // A write refused inside a pass changes nothing.
  setFifth(40);
  expect(kForty, Update::insidePass);
  expect(kForty, Update::none);

  // A recorded compute pass begun inside the draw's still runs first, on
  // input the buffer has not seen computed.
  if (path != Path::immediate)
  {
    setFifth(7);
    expect(kSeven, Update::none, true);
  }
  // Only a recorder can move a copy made inside open passes ahead of them.
  if (path == Path::recorder)
  {
    setFifth(40);
    expect(kSeven, Update::beforeBind);
    setFifth(40);
    expect(kSeven, Update::beforeBind, true);
    // One made after the compute pass, inside the draw's, still follows it.
    setFifth(40);
    expect(kForty, Update::afterPass, true);
    expect(kSeven, Update::none);
  }
  ctx.setDeferredRecording(false);
}

// A storage buffer made without data is drawn before anything writes it.
void freshStorageReadsZero(TestingWindow* window)
{
  window->resize(kSize, kSize);
  RenderContext* renderContext = window->renderContext();
  ore_gm::OreGMContext oreGM;
  REQUIRE(oreGM.ensureContext(renderContext));
  Context& ctx = *renderContext->getOreContext();
  if (lacksCompute(ctx))
    return;
  rcp<RenderCanvas> canvas = renderContext->makeRenderCanvas(kSize, kSize);
  REQUIRE(canvas != nullptr);
  rcp<TextureView> target = ctx.wrapCanvasTexture(canvas.get());
  REQUIRE(target != nullptr);

  const Shader shader = bakedShader(ctx);
  rcp<BindGroupLayout> layout;
  rcp<Pipeline> pipeline = drawPipelineFor(
      ctx,
      shader.vertexModule(),
      shader.vertex,
      shader.fragmentModule(),
      shader.fragment,
      layout);

  // Freed over the next frames, their memory comes back still holding these.
  uint32_t used[kCount];
  for (uint32_t& value : used)
    value = 100;
  for (int i = 0; i < 64; ++i)
    makeBuffer(ctx, BufferUsage::storage, used, kBytes);
  for (int frame = 0; frame < 8; ++frame)
  {
    beginOreFrame(window, oreGM, renderContext);
    oreGM.endFrame(renderContext);
    window->endFrame();
  }
  rcp<Buffer> fresh = makeBuffer(ctx, BufferUsage::storage, nullptr, kBytes);

  const BindGroupDesc::StorageEntry storage[] = {{2, fresh.get(), 0, 0}};
  BindGroupDesc groupDesc;
  groupDesc.layout = layout.get();
  groupDesc.storageBuffers = storage;
  groupDesc.storageBufferCount = 1;
  rcp<BindGroup> group = ctx.makeBindGroup(groupDesc);
  REQUIRE(group != nullptr);

  // Cleared white, so black is the draw's own.
  RenderPassDesc passDesc = clearInto(target.get());
  passDesc.colorAttachments[0].clearColor = {1.f, 1.f, 1.f, 1.f};
  beginOreFrame(window, oreGM, renderContext);
  auto pass = ctx.beginRenderPass(passDesc);
  REQUIRE(pass != nullptr);
  pass->setViewport(0, 0, kSize, kSize);
  pass->setPipeline(pipeline.get());
  pass->setBindGroup(0, group.get());
  pass->draw(3);
  pass->finish();
  oreGM.endFrame(renderContext);
  window->endFrame();
  const uint8_t kBlack[3] = {0, 0, 0};
  expectCenterPixel(window, canvas.get(), kBlack);
}

rcp<BindGroup> storageGroup(
    Context& ctx,
    const rcp<BindGroupLayout>& layout,
    const BindGroupDesc::StorageEntry* storage,
    uint32_t count)
{
  BindGroupDesc desc;
  desc.layout = layout.get();
  desc.storageBuffers = storage;
  desc.storageBufferCount = count;
  rcp<BindGroup> group = ctx.makeBindGroup(desc);
  INFO("makeBindGroup: " << ctx.lastError());
  REQUIRE(group != nullptr);
  return group;
}

// Binds `inputBytes` of `input` from `inputOffset`, the rest of it for 0,
// beside all of `output`.
rcp<BindGroup> sizedGroup(
    Context& ctx,
    const rcp<BindGroupLayout>& layout,
    Buffer* input,
    uint32_t inputOffset,
    uint32_t inputBytes,
    Buffer* output)
{
  const BindGroupDesc::StorageEntry storage[] = {
      {0, input, inputOffset, inputBytes},
      {1, output, 0, 0}};
  return storageGroup(ctx, layout, storage, 2);
}

// Each stage reads an array's length and clamps an index past its end, both
// against the bytes its bind group bound.
void sizeArraysFromWhatIsBound(TestingWindow* window)
{
  window->resize(kSize, kSize);
  RenderContext* renderContext = window->renderContext();
  ore_gm::OreGMContext oreGM;
  REQUIRE(oreGM.ensureContext(renderContext));
  Context& ctx = *renderContext->getOreContext();
  if (lacksCompute(ctx))
    return;
  rcp<RenderCanvas> canvas = renderContext->makeRenderCanvas(kSize, kSize);
  REQUIRE(canvas != nullptr);
  rcp<TextureView> target = ctx.wrapCanvasTexture(canvas.get());
  REQUIRE(target != nullptr);

  // `crossed` declares the same buffers in another order, so its fragment
  // stage reads its sizes where `shader` reads others.
  const Shader shader = bakedShaderAt(ctx, 1);
  const Shader crossed = bakedShaderAt(ctx, 2);
  const std::string vertex = shader.physical("vs_sized");
  ShaderModule* vertexModule = shader.moduleFor("vs_sized");
  REQUIRE(vertexModule != nullptr);
  const std::string sizes = shader.physical("cs_sizes");
  rcp<BindGroupLayout> computeLayout;
  rcp<ComputePipeline> compute = computePipelineFor(
      ctx,
      shader.moduleFor("cs_sizes"),
      sizes,
      computeLayout);
  const std::string computed = shader.physical("fs_computed");
  rcp<BindGroupLayout> computedLayout;
  rcp<Pipeline> showComputed = drawPipelineFor(
      ctx,
      vertexModule,
      vertex,
      shader.moduleFor("fs_computed"),
      computed,
      computedLayout);
  const std::string sized = shader.physical("fs_sized");
  rcp<BindGroupLayout> sizedLayout;
  rcp<Pipeline> showSized = drawPipelineFor(
      ctx,
      vertexModule,
      vertex,
      shader.moduleFor("fs_sized"),
      sized,
      sizedLayout);
  const std::string crossedName = crossed.physical("fs_crossed");
  rcp<BindGroupLayout> crossedLayout;
  rcp<Pipeline> showCrossed = drawPipelineFor(
      ctx,
      vertexModule,
      vertex,
      crossed.moduleFor("fs_crossed"),
      crossedName,
      crossedLayout);
  const std::string bothName = crossed.physical("fs_both");
  rcp<BindGroupLayout> bothLayout;
  rcp<Pipeline> showBoth = drawPipelineFor(
      ctx,
      vertexModule,
      vertex,
      crossed.moduleFor("fs_both"),
      bothName,
      bothLayout);

  // 96 values counting up from 100, and 64 results ending in 190.
  constexpr uint32_t kInputCount = 96;
  constexpr uint32_t kSixteen = 16 * sizeof(uint32_t);
  constexpr uint32_t kTailOffset = 64 * sizeof(uint32_t);
  uint32_t values[kInputCount];
  for (uint32_t i = 0; i < kInputCount; ++i)
    values[i] = 100 + i;
  uint32_t results[kCount] = {};
  results[kCount - 1] = 190;
  const uint16_t indices[4] = {0, 1, 2, 0};
  rcp<Buffer> input = makeBuffer(
      ctx,
      BufferUsage::storage,
      values,
      sizeof(values));
  rcp<Buffer> output = makeBuffer(
      ctx,
      BufferUsage::storage,
      results,
      sizeof(results));
  rcp<Buffer> triangle = makeBuffer(
      ctx,
      BufferUsage::index,
      indices,
      sizeof(indices));
  Buffer* in = input.get();
  Buffer* out = output.get();

  const RenderPassDesc passDesc = clearInto(target.get());
  auto beginDraws = [&](Pipeline* pipeline) {
    auto pass = ctx.beginRenderPass(passDesc);
    REQUIRE(pass != nullptr);
    pass->setViewport(0, 0, kSize, kSize);
    pass->setPipeline(pipeline);
    return pass;
  };
  auto endFrame = [&](const uint8_t(&rgb)[3]) {
    INFO("frame: " << ctx.lastError());
    CHECK(ctx.lastError().empty());
    oreGM.endFrame(renderContext);
    window->endFrame();
    expectCenterPixel(window, canvas.get(), rgb);
  };

  // The pass binds the first 16 values, so that many and the last of them,
  // 115, where the whole buffer holds 96.
  beginOreFrame(window, oreGM, renderContext);
  ctx.clearLastError();
  auto computePass = ctx.beginComputePass({});
  REQUIRE(computePass != nullptr);
  computePass->setPipeline(compute.get());
  computePass->setBindGroup(
      0,
      sizedGroup(ctx, computeLayout, in, 0, kSixteen, out).get());
  computePass->dispatchWorkgroups(1);
  computePass->finish();
  auto pass = beginDraws(showComputed.get());
  pass->setBindGroup(0, sizedGroup(ctx, computedLayout, in, 0, 0, out).get());
  pass->draw(3);
  pass->finish();
  const uint8_t kComputed[3] = {16, 64, 115};
  endFrame(kComputed);

  // The vertex stage's length of the input, the fragment stage's of the
  // output, and the output's last value from an index past its end.
  rcp<BindGroup> whole = sizedGroup(ctx, sizedLayout, in, 0, 0, out);
  rcp<BindGroup> tail = sizedGroup(ctx, sizedLayout, in, kTailOffset, 0, out);
  beginOreFrame(window, oreGM, renderContext);
  pass = beginDraws(showSized.get());
  pass->setBindGroup(0, whole.get());
  pass->setIndexBuffer(triangle.get(), IndexFormat::uint16);
  pass->drawIndexed(3);
  pass->finish();
  const uint8_t kWhole[3] = {96, 64, 190};
  endFrame(kWhole);

  // A group bound later in the pass sizes the draws after it, here by what
  // the buffer holds past the group's offset.
  beginOreFrame(window, oreGM, renderContext);
  pass = beginDraws(showSized.get());
  pass->setBindGroup(0, whole.get());
  pass->draw(3);
  pass->setBindGroup(0, tail.get());
  pass->draw(3);
  pass->finish();
  const uint8_t kTail[3] = {32, 64, 190};
  endFrame(kTail);

  // A pipeline whose fragment stage comes from the other file takes that
  // file's order, with the group left as the pipeline before it bound it.
  beginOreFrame(window, oreGM, renderContext);
  pass = beginDraws(showSized.get());
  pass->setBindGroup(0, tail.get());
  pass->draw(3);
  pass->setPipeline(showCrossed.get());
  pass->draw(3);
  pass->finish();
  const uint8_t kCrossed[3] = {64, 32, 190};
  endFrame(kCrossed);

  // That file's fragment stage reads the input's size too, declared after a
  // buffer nothing binds.
  beginOreFrame(window, oreGM, renderContext);
  pass = beginDraws(showBoth.get());
  pass->setBindGroup(
      0,
      sizedGroup(ctx, bothLayout, in, 0, kSixteen, out).get());
  pass->draw(3);
  pass->finish();
  const uint8_t kBoth[3] = {16, 64, 190};
  endFrame(kBoth);
}

// One pass writes a buffer through group 1, then through group 0 with a
// pipeline that has no group 1. Direct3D writes neither while both stay set.
void writeAcrossPipelines(TestingWindow* window)
{
  window->resize(kSize, kSize);
  RenderContext* renderContext = window->renderContext();
  ore_gm::OreGMContext oreGM;
  REQUIRE(oreGM.ensureContext(renderContext));
  Context& ctx = *renderContext->getOreContext();
  if (lacksCompute(ctx))
    return;
  rcp<RenderCanvas> canvas = renderContext->makeRenderCanvas(kSize, kSize);
  REQUIRE(canvas != nullptr);
  rcp<TextureView> target = ctx.wrapCanvasTexture(canvas.get());
  REQUIRE(target != nullptr);

  const Shader shader = bakedShaderAt(ctx, 4);
  const std::string vertex = shader.physical("vs_cover");
  ShaderModule* vertexModule = shader.moduleFor("vs_cover");
  rcp<BindGroupLayout> bothLayouts[2];
  rcp<Pipeline> writeBoth = drawPipelineFor(
      ctx,
      vertexModule,
      vertex,
      shader.moduleFor("fs_both"),
      shader.physical("fs_both"),
      bothLayouts,
      2);
  rcp<BindGroupLayout> firstLayout;
  rcp<Pipeline> writeFirst = drawPipelineFor(
      ctx,
      vertexModule,
      vertex,
      shader.moduleFor("fs_first"),
      shader.physical("fs_first"),
      firstLayout);
  rcp<BindGroupLayout> shownLayout;
  rcp<Pipeline> show = drawPipelineFor(
      ctx,
      vertexModule,
      vertex,
      shader.moduleFor("fs_shown"),
      shader.physical("fs_shown"),
      shownLayout);

  uint32_t values[kCount] = {};
  values[2] = 200;
  rcp<Buffer> other = makeBuffer(ctx, BufferUsage::storage, nullptr, kBytes);
  rcp<Buffer> buffer = makeBuffer(ctx, BufferUsage::storage, values, kBytes);
  const BindGroupDesc::StorageEntry otherAtZero[] = {{0, other.get(), 0, 0}};
  const BindGroupDesc::StorageEntry atZero[] = {{0, buffer.get(), 0, 0}};
  const BindGroupDesc::StorageEntry atOne[] = {{1, buffer.get(), 0, 0}};
  rcp<BindGroup> otherFirst = storageGroup(ctx, bothLayouts[0], otherAtZero, 1);
  rcp<BindGroup> second = storageGroup(ctx, bothLayouts[1], atZero, 1);
  rcp<BindGroup> first = storageGroup(ctx, firstLayout, atZero, 1);
  rcp<BindGroup> shown = storageGroup(ctx, shownLayout, atOne, 1);

  const RenderPassDesc passDesc = clearInto(target.get());
  beginOreFrame(window, oreGM, renderContext);
  ctx.clearLastError();
  auto pass = ctx.beginRenderPass(passDesc);
  REQUIRE(pass != nullptr);
  pass->setViewport(0, 0, kSize, kSize);
  pass->setPipeline(writeBoth.get());
  pass->setBindGroup(0, otherFirst.get());
  pass->setBindGroup(1, second.get());
  pass->draw(3);
  pass->setPipeline(writeFirst.get());
  pass->setBindGroup(0, first.get());
  pass->draw(3);
  pass->finish();
  pass = ctx.beginRenderPass(passDesc);
  REQUIRE(pass != nullptr);
  pass->setViewport(0, 0, kSize, kSize);
  pass->setPipeline(show.get());
  pass->setBindGroup(0, shown.get());
  pass->draw(3);
  pass->finish();
  INFO("frame: " << ctx.lastError());
  CHECK(ctx.lastError().empty());
  oreGM.endFrame(renderContext);
  window->endFrame();
  const uint8_t kWritten[3] = {100, 150, 200};
  expectCenterPixel(window, canvas.get(), kWritten);
}

#if defined(ORE_BACKEND_METAL)
void computeFromMetalSource(TestingWindow* window)
{
  computeIntoStorage(window, handWrittenShader);
}

// Only Metal replays a frame's recorded passes when the frame ends.
void computeReplayedWithFrame(TestingWindow* window)
{
  computeIntoStorage(window, bakedShader, Path::frameReplay);
}
#endif

void computeFromBakedWGSL(TestingWindow* window)
{
  computeIntoStorage(window, bakedShader);
}

void computeThroughRecorder(TestingWindow* window)
{
  computeIntoStorage(window, bakedShader, Path::recorder);
}

void computeReplayedInline(TestingWindow* window)
{
  computeIntoStorage(window, bakedShader, Path::inlineReplay);
}
} // namespace

#if defined(ORE_BACKEND_METAL)

TEST_CASE("ore metal sizes runtime arrays from the bound buffers", "[ore]")
{
  if (ore_test::runOnBackends(kMetal, sizeArraysFromWhatIsBound) == 0)
    WARN("Metal is not available headless; skipping");
}

TEST_CASE("ore metal computes into a storage buffer a draw reads", "[ore]")
{
  if (ore_test::runOnBackends(kMetal, computeFromMetalSource) == 0)
    WARN("Metal is not available headless; skipping");
}

TEST_CASE("ore metal computes from a shader baked from WGSL", "[ore]")
{
  if (ore_test::runOnBackends(kMetal, computeFromBakedWGSL) == 0)
    WARN("Metal is not available headless; skipping");
}

TEST_CASE("ore metal draws zeros from a fresh storage buffer", "[ore]")
{
  if (ore_test::runOnBackends(kMetal, freshStorageReadsZero) == 0)
    WARN("Metal is not available headless; skipping");
}

TEST_CASE("ore metal replays recorded compute in order", "[ore]")
{
  SECTION("from a recording context")
  {
    if (ore_test::runOnBackends(kMetal, computeThroughRecorder) == 0)
      WARN("Metal is not available headless; skipping");
  }
  SECTION("as each pass finishes")
  {
    if (ore_test::runOnBackends(kMetal, computeReplayedInline) == 0)
      WARN("Metal is not available headless; skipping");
  }
  SECTION("when the frame ends")
  {
    if (ore_test::runOnBackends(kMetal, computeReplayedWithFrame) == 0)
      WARN("Metal is not available headless; skipping");
  }
}

TEST_CASE("ore metal writes one buffer from two pipelines of a pass", "[ore]")
{
  if (ore_test::runOnBackends(kMetal, writeAcrossPipelines) == 0)
    WARN("Metal is not available headless; skipping");
}
#endif // ORE_BACKEND_METAL

#if defined(ORE_BACKEND_VK)
TEST_CASE("ore vulkan sizes runtime arrays from the bound buffers", "[ore]")
{
  if (ore_test::runOnBackends(kVulkan, sizeArraysFromWhatIsBound) == 0)
    WARN("Vulkan is not available headless; skipping");
}

TEST_CASE("ore vulkan computes from a shader baked from WGSL", "[ore]")
{
  if (ore_test::runOnBackends(kVulkan, computeFromBakedWGSL) == 0)
    WARN("Vulkan is not available headless; skipping");
}

TEST_CASE("ore vulkan draws zeros from a fresh storage buffer", "[ore]")
{
  if (ore_test::runOnBackends(kVulkan, freshStorageReadsZero) == 0)
    WARN("Vulkan is not available headless; skipping");
}

TEST_CASE("ore vulkan replays recorded compute in order", "[ore]")
{
  SECTION("from a recording context")
  {
    if (ore_test::runOnBackends(kVulkan, computeThroughRecorder) == 0)
      WARN("Vulkan is not available headless; skipping");
  }
  SECTION("as each pass finishes")
  {
    if (ore_test::runOnBackends(kVulkan, computeReplayedInline) == 0)
      WARN("Vulkan is not available headless; skipping");
  }
}

TEST_CASE("ore vulkan writes one buffer from two pipelines of a pass", "[ore]")
{
  if (ore_test::runOnBackends(kVulkan, writeAcrossPipelines) == 0)
    WARN("Vulkan is not available headless; skipping");
}
#endif // ORE_BACKEND_VK

#if defined(ORE_BACKEND_D3D11)
TEST_CASE("ore d3d11 sizes runtime arrays from the bound buffers", "[ore]")
{
  if (ore_test::runOnBackends(kD3D11, sizeArraysFromWhatIsBound) == 0)
    WARN("Direct3D 11 is not available headless; skipping");
}

TEST_CASE("ore d3d11 computes from a shader baked from WGSL", "[ore]")
{
  if (ore_test::runOnBackends(kD3D11, computeFromBakedWGSL) == 0)
    WARN("Direct3D 11 is not available headless; skipping");
}

TEST_CASE("ore d3d11 draws zeros from a fresh storage buffer", "[ore]")
{
  if (ore_test::runOnBackends(kD3D11, freshStorageReadsZero) == 0)
    WARN("Direct3D 11 is not available headless; skipping");
}

TEST_CASE("ore d3d11 replays recorded compute in order", "[ore]")
{
  SECTION("from a recording context")
  {
    if (ore_test::runOnBackends(kD3D11, computeThroughRecorder) == 0)
      WARN("Direct3D 11 is not available headless; skipping");
  }
  SECTION("as each pass finishes")
  {
    if (ore_test::runOnBackends(kD3D11, computeReplayedInline) == 0)
      WARN("Direct3D 11 is not available headless; skipping");
  }
}

TEST_CASE("ore d3d11 writes one buffer from two pipelines of a pass", "[ore]")
{
  if (ore_test::runOnBackends(kD3D11, writeAcrossPipelines) == 0)
    WARN("Direct3D 11 is not available headless; skipping");
}
#endif // ORE_BACKEND_D3D11

#if defined(ORE_BACKEND_D3D12)
TEST_CASE("ore d3d12 sizes runtime arrays from the bound buffers", "[ore]")
{
  if (ore_test::runOnBackends(kD3D12, sizeArraysFromWhatIsBound) == 0)
    WARN("Direct3D 12 is not available headless; skipping");
}

TEST_CASE("ore d3d12 computes from a shader baked from WGSL", "[ore]")
{
  if (ore_test::runOnBackends(kD3D12, computeFromBakedWGSL) == 0)
    WARN("Direct3D 12 is not available headless; skipping");
}

TEST_CASE("ore d3d12 draws zeros from a fresh storage buffer", "[ore]")
{
  if (ore_test::runOnBackends(kD3D12, freshStorageReadsZero) == 0)
    WARN("Direct3D 12 is not available headless; skipping");
}

TEST_CASE("ore d3d12 replays recorded compute in order", "[ore]")
{
  SECTION("from a recording context")
  {
    if (ore_test::runOnBackends(kD3D12, computeThroughRecorder) == 0)
      WARN("Direct3D 12 is not available headless; skipping");
  }
  SECTION("as each pass finishes")
  {
    if (ore_test::runOnBackends(kD3D12, computeReplayedInline) == 0)
      WARN("Direct3D 12 is not available headless; skipping");
  }
}

TEST_CASE("ore d3d12 writes one buffer from two pipelines of a pass", "[ore]")
{
  if (ore_test::runOnBackends(kD3D12, writeAcrossPipelines) == 0)
    WARN("Direct3D 12 is not available headless; skipping");
}
#endif // ORE_BACKEND_D3D12

#endif // ORE_BACKEND_METAL || ORE_BACKEND_VK || ORE_BACKEND_D3D11 || D3D12
