/*
 * Copyright 2026 Rive
 *
 * Metal keeps vertex buffers and the buffers a pipeline binds in one table, so
 * vertex buffers must move out of the way of a binding past slot 15 rather
 * than share its slot.
 */

#include "ore_test_backends.hpp"

#if defined(ORE_BACKEND_METAL)
#include "rive/renderer/render_context.hpp"
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/ore/ore_bind_group_layout.hpp"
#include "rive/renderer/ore/ore_binding_map.hpp"
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_render_pass.hpp"
#include "rive/renderer/ore/ore_shader_module.hpp"
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace rive;
using namespace rive::gpu;
using namespace rive::ore;

namespace
{
constexpr TestingWindow::Backend kMetal[] = {TestingWindow::Backend::metal};

// vs_main takes its triangle from a vertex buffer, vs_cover from the vertex
// index.
const char kSlotToken[] = "SLOT";
const char kMSL[] = R"(
#include <metal_stdlib>
using namespace metal;

struct Tint { float4 color; };
struct VertexIn {
    float2 position [[attribute(0)]];
    float4 color [[attribute(1)]];
};
struct VertexOut {
    float4 position [[position]];
    float4 color;
};

vertex VertexOut vs_main(VertexIn in [[stage_in]],
                         constant Tint& tint [[buffer(SLOT)]]) {
    VertexOut out;
    out.position = float4(in.position, 0.0, 1.0);
    out.color = in.color * tint.color;
    return out;
}

vertex VertexOut vs_cover(uint vid [[vertex_id]],
                          constant Tint& tint [[buffer(SLOT)]]) {
    const float2 positions[3] = {
        float2(-1.0, -1.0), float2(3.0, -1.0), float2(-1.0, 3.0)};
    VertexOut out;
    out.position = float4(positions[vid], 0.0, 1.0);
    out.color = tint.color;
    return out;
}

fragment float4 fs_main(VertexOut in [[stage_in]]) { return in.color; }
)";

constexpr int kSize = 32;
constexpr ore_gm::TriVertex kCover[] = {
    {-1.f, -1.f, 1.f, 0.5f, 1.f, 1.f},
    {3.f, -1.f, 1.f, 0.5f, 1.f, 1.f},
    {-1.f, 3.f, 1.f, 0.5f, 1.f, 1.f},
};
constexpr float kTint[4] = {0.2f, 0.8f, 0.4f, 1.f};
// The vertex color times the tint.
constexpr uint8_t kTinted[3] = {0x33, 0x66, 0x66};
constexpr float kOtherTint[4] = {1.f, 0.f, 0.f, 1.f};

// Reads a tint from the buffer the bake put at the first of `slots`, and
// declares a buffer at each of the others. The last `declaring` of
// `vertexBuffers` vertex buffers each declare the attributes.
struct TintPipeline
{
  TintPipeline(
      Context& ctx,
      const std::vector<uint16_t>& slots,
      uint32_t vertexBuffers,
      uint32_t declaring = 1)
  {
    BindingMap bindings;
    for (size_t i = 0; i < slots.size(); ++i)
    {
      BindingMap::Entry binding{};
      binding.binding = static_cast<uint8_t>(i);
      binding.kind = ResourceKind::UniformBuffer;
      binding.stageMask = BindingMap::kStageVertex | BindingMap::kStageFragment;
      binding.backendSlot[0] = slots[i];
      binding.backendSlot[1] = slots[i];
      binding.minBindingSize = sizeof(kTint);
      bindings.push(binding);
    }
    bindings.finalize();
    const std::vector<uint8_t> blob = bindings.toBlob();
    m_bindingCount = static_cast<uint32_t>(slots.size());

    std::string msl = kMSL;
    const std::string slot = std::to_string(slots[0]);
    for (size_t at; (at = msl.find(kSlotToken)) != std::string::npos;)
      msl.replace(at, strlen(kSlotToken), slot);
    ShaderModuleDesc moduleDesc{};
    moduleDesc.code = msl.data();
    moduleDesc.codeSize = static_cast<uint32_t>(msl.size());
    moduleDesc.bindingMapBytes = blob.data();
    moduleDesc.bindingMapSize = static_cast<uint32_t>(blob.size());
    module = ctx.makeShaderModule(moduleDesc);
    REQUIRE(module != nullptr);
    layout = makeBindGroupLayoutFromShader(ctx, module.get(), 0);
    REQUIRE(layout != nullptr);

    const ore::VertexAttribute attributes[] = {
        {offsetof(ore_gm::TriVertex, x), 0, VertexFormat::float2},
        {offsetof(ore_gm::TriVertex, r), 1, VertexFormat::float4},
    };
    VertexBufferLayout vertexLayouts[kMaxVertexBufferSlots + 1]{};
    for (uint32_t i = 0; i < vertexBuffers; ++i)
    {
      vertexLayouts[i].stride = sizeof(ore_gm::TriVertex);
      if (i + declaring >= vertexBuffers)
      {
        vertexLayouts[i].attributes = attributes;
        vertexLayouts[i].attributeCount = 2;
      }
    }
    BindGroupLayout* layouts[] = {layout.get()};
    PipelineDesc desc{};
    desc.vertexModule = module.get();
    desc.fragmentModule = module.get();
    desc.vertexEntryPoint = vertexBuffers > 0 ? "vs_main" : "vs_cover";
    desc.fragmentEntryPoint = "fs_main";
    desc.vertexBuffers = vertexLayouts;
    desc.vertexBufferCount = vertexBuffers;
    desc.topology = PrimitiveTopology::triangleList;
    desc.colorTargets[0].format = TextureFormat::rgba8unorm;
    desc.colorCount = 1;
    desc.bindGroupLayouts = layouts;
    desc.bindGroupLayoutCount = 1;
    desc.label = "ore_metal_vertex_buffer_slots";
    pipeline = ctx.makePipeline(desc, &error);
  }

  // `tint` at every binding.
  rcp<BindGroup> group(Context& ctx, Buffer* tint) const
  {
    std::vector<BindGroupDesc::UBOEntry> entries(m_bindingCount);
    for (uint32_t i = 0; i < m_bindingCount; ++i)
    {
      entries[i].slot = i;
      entries[i].buffer = tint;
      entries[i].size = sizeof(kTint);
    }
    BindGroupDesc desc{};
    desc.layout = layout.get();
    desc.ubos = entries.data();
    desc.uboCount = m_bindingCount;
    auto made = ctx.makeBindGroup(desc);
    INFO("makeBindGroup: " << ctx.lastError());
    REQUIRE(made != nullptr);
    return made;
  }

  rcp<ShaderModule> module;
  rcp<BindGroupLayout> layout;
  rcp<Pipeline> pipeline;
  std::string error;

private:
  uint32_t m_bindingCount = 0;
};

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
  REQUIRE(buffer != nullptr);
  return buffer;
}

struct Scene
{
  TestingWindow* window;
  RenderContext* renderContext;
  ore_gm::OreGMContext oreGM;
  rcp<RenderCanvas> canvas;
  rcp<TextureView> target;

  explicit Scene(TestingWindow* w) : window(w)
  {
    window->resize(kSize, kSize);
    renderContext = window->renderContext();
    REQUIRE(oreGM.ensureContext(renderContext));
    canvas = renderContext->makeRenderCanvas(kSize, kSize);
    REQUIRE(canvas != nullptr);
    target = ctx().wrapCanvasTexture(canvas.get());
    REQUIRE(target != nullptr);
  }

  Context& ctx() { return *renderContext->getOreContext(); }

  // Runs `draws` in one pass over a blue clear and checks the center pixel.
  template <typename Draws> void expect(const uint8_t (&rgb)[3], Draws&& draws)
  {
    window->beginFrame({
        .name = "ore_metal_vertex_buffer_slots",
        .clearColor = 0xff000000,
        .doClear = true,
    });
    oreGM.beginFrame(renderContext);
    RenderPassDesc passDesc{};
    passDesc.colorAttachments[0].view = target.get();
    passDesc.colorAttachments[0].loadOp = LoadOp::clear;
    passDesc.colorAttachments[0].storeOp = StoreOp::store;
    passDesc.colorAttachments[0].clearColor = {0.f, 0.f, 1.f, 1.f};
    passDesc.colorCount = 1;
    passDesc.label = "ore_metal_vertex_buffer_slots_pass";
    auto pass = ctx().beginRenderPass(passDesc);
    REQUIRE(pass != nullptr);
    pass->setViewport(0, 0, kSize, kSize);
    draws(*pass);
    pass->finish();
    oreGM.endFrame(renderContext);
    window->endFrame();

    auto renderer = window->beginFrame({
        .name = "ore_metal_vertex_buffer_slots_read",
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
      CHECK(std::abs(p[c] - rgb[c]) <= 0x08);
  }
};

void drawBesideHighBindings(TestingWindow* window)
{
  Scene scene(window);
  Context& ctx = scene.ctx();
  auto vertices = makeBuffer(ctx, BufferUsage::vertex, kCover, sizeof(kCover));
  auto tint = makeBuffer(ctx, BufferUsage::uniform, kTint, sizeof(kTint));
  auto otherTint = makeBuffer(
      ctx,
      BufferUsage::uniform,
      kOtherTint,
      sizeof(kOtherTint));

  SECTION("a binding where vertex buffers used to start")
  {
    TintPipeline tinted(ctx, {16}, 1);
    TintPipeline other(ctx, {0}, 1);
    INFO("makePipeline: " << tinted.error);
    REQUIRE(tinted.pipeline != nullptr);
    REQUIRE(other.pipeline != nullptr);
    auto group = tinted.group(ctx, tint.get());
    auto otherGroup = other.group(ctx, otherTint.get());
    scene.expect(kTinted, [&](RenderPass& pass) {
      // Set once, ahead of the pipelines that each place it.
      pass.setVertexBuffer(0, vertices.get());
      pass.setBindGroup(0, group.get());
      pass.setPipeline(tinted.pipeline.get());
      pass.draw(3);
      pass.setPipeline(other.pipeline.get());
      pass.setBindGroup(0, otherGroup.get());
      pass.draw(3);
      pass.setPipeline(tinted.pipeline.get());
      pass.setBindGroup(0, group.get());
      pass.draw(3);
    });
  }

  SECTION("after a pipeline without vertex buffers bound that slot")
  {
    TintPipeline cover(ctx, {16}, 0);
    TintPipeline tinted(ctx, {0}, 1);
    REQUIRE(cover.pipeline != nullptr);
    REQUIRE(tinted.pipeline != nullptr);
    auto coverGroup = cover.group(ctx, otherTint.get());
    auto tintedGroup = tinted.group(ctx, tint.get());
    scene.expect(kTinted, [&](RenderPass& pass) {
      pass.setVertexBuffer(0, vertices.get());
      pass.setPipeline(cover.pipeline.get());
      pass.setBindGroup(0, coverGroup.get());
      pass.draw(3);
      pass.setPipeline(tinted.pipeline.get());
      pass.setBindGroup(0, tintedGroup.get());
      pass.draw(3);
    });
  }

  SECTION("eight vertex buffers around twelve bindings")
  {
    // Slots 16 to 27 leave two above for vertex buffers and send six below.
    std::vector<uint16_t> slots;
    for (uint16_t slot = 16; slot < 28; ++slot)
      slots.push_back(slot);
    TintPipeline tinted(ctx, slots, kMaxVertexBufferSlots);
    INFO("makePipeline: " << tinted.error);
    REQUIRE(tinted.pipeline != nullptr);
    auto group = tinted.group(ctx, tint.get());
    const uint16_t kIndices[] = {0, 1, 2, 0};
    auto indices = makeBuffer(
        ctx,
        BufferUsage::index,
        kIndices,
        sizeof(kIndices));
    scene.expect(kTinted, [&](RenderPass& pass) {
      pass.setPipeline(tinted.pipeline.get());
      pass.setBindGroup(0, group.get());
      for (uint32_t slot = 0; slot < kMaxVertexBufferSlots; ++slot)
        pass.setVertexBuffer(slot, vertices.get());
      pass.setIndexBuffer(indices.get(), IndexFormat::uint16);
      pass.drawIndexed(3);
    });
  }

  SECTION("a vertex buffer no attribute reads")
  {
    constexpr ore_gm::TriVertex kBlack[3] = {};
    auto unread = makeBuffer(ctx, BufferUsage::vertex, kBlack, sizeof(kBlack));
    // With two declaring, the second buffer's attributes replace the first's.
    for (uint32_t declaring : {1u, 2u})
    {
      TintPipeline tinted(ctx, {0}, 2, declaring);
      INFO("makePipeline: " << tinted.error);
      REQUIRE(tinted.pipeline != nullptr);
      auto group = tinted.group(ctx, tint.get());
      scene.expect(kTinted, [&](RenderPass& pass) {
        pass.setPipeline(tinted.pipeline.get());
        pass.setBindGroup(0, group.get());
        pass.setVertexBuffer(0, unread.get());
        pass.setVertexBuffer(1, vertices.get());
        pass.draw(3);
      });
    }
  }

  SECTION("more vertex buffers than a pipeline may have")
  {
    TintPipeline crowded(ctx, {0}, kMaxVertexBufferSlots + 1);
    CHECK(crowded.pipeline == nullptr);
    CHECK(crowded.error.find("vertex buffers") != std::string::npos);
  }
}
} // namespace

TEST_CASE(
    "ore metal keeps vertex buffers clear of the buffers it binds",
    "[ore]")
{
  if (ore_test::runOnBackends(kMetal, drawBesideHighBindings) == 0)
    WARN("Metal is not available headless; skipping");
}
#endif // ORE_BACKEND_METAL
