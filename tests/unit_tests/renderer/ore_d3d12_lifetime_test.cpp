/*
 * Copyright 2026 Rive
 */

// Ore D3D12 descriptor and resource lifetime: many views alive at once, the
// shared manager on Ore's frame, Ore frames sharing one command list, and a
// refused pipeline. The GM helper pulls in Metal's Objective-C headers on Mac,
// so everything sits behind the guard.

#if defined(ORE_BACKEND_D3D12)
#include "common/testing_window.hpp"
#include "gm/ore_gm_helper.hpp"
#include <catch.hpp>

#include "rive/math/mat2d.hpp"
#include "rive/renderer/d3d12/render_context_d3d12_impl.hpp"
#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_context_d3d12.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/renderer/ore/ore_render_pass.hpp"
#include "rive/renderer/ore/ore_sampler.hpp"
#include "rive/renderer/ore/ore_shader_module.hpp"
#include "rive/renderer/ore/ore_texture.hpp"
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/render_context.hpp"

#include <memory>
#include <vector>

using namespace rive;
using namespace rive::ore;

namespace
{
constexpr int kW = 64;
constexpr int kH = 32;

bool high(uint8_t c) { return c >= 0xf0; }
bool low(uint8_t c) { return c <= 0x10; }

const uint8_t* pixelAt(const std::vector<uint8_t>& px, int x, int y)
{
  return &px[(static_cast<size_t>(y) * kW + x) * 4];
}

// Init hands back a window that's still alive, so a failed REQUIRE has to
// destroy it too or later tests inherit its open Ore frame.
struct WindowDestroyer
{
  void operator()(TestingWindow*) const { TestingWindow::Destroy(); }
};
using WindowScope = std::unique_ptr<TestingWindow, WindowDestroyer>;

// A headless D3D12 window with Ore, or null when the job can't make one.
WindowScope makeWindow()
{
  WindowScope window(TestingWindow::Init(
      TestingWindow::Backend::d3d12,
      {},
      TestingWindow::Visibility::headless));
  if (window == nullptr || window->renderContext() == nullptr ||
      !ore_gm::isOreBackendActive())
  {
    return nullptr;
  }
  window->resize(kW, kH);
  return window;
}

// Draws a texture over a pass's target with the image view shader.
struct TextureDraw
{
  rcp<BindGroupLayout> textureLayout;
  rcp<BindGroupLayout> samplerLayout;
  rcp<Pipeline> pipeline;
  rcp<Sampler> sampler;
  rcp<BindGroup> samplerGroup;

  // withDepth makes a pipeline a color only pass refuses.
  bool init(Context& ctx, bool withDepth = false)
  {
    auto shader = ore_gm::loadShader(ctx, ore_gm::kImageView);
    if (!shader.vsModule)
    {
      return false;
    }
    textureLayout = ore_gm::makeLayoutFromShader(ctx, shader.vsModule.get(), 1);
    samplerLayout = ore_gm::makeLayoutFromShader(ctx, shader.vsModule.get(), 2);
    BindGroupLayout* layouts[] = {
        nullptr,
        textureLayout.get(),
        samplerLayout.get()};

    PipelineDesc desc{};
    desc.vertexModule = shader.vsModule.get();
    desc.fragmentModule = shader.psModule.get();
    desc.vertexEntryPoint = shader.vsEntryPoint;
    desc.fragmentEntryPoint = shader.fsEntryPoint;
    desc.vertexBufferCount = 0;
    desc.topology = PrimitiveTopology::triangleList;
    desc.colorTargets[0].format = TextureFormat::rgba8unorm;
    desc.colorCount = 1;
    desc.depthStencil.format = withDepth ? TextureFormat::depth32float
                                         : TextureFormat::rgba8unorm;
    desc.depthStencil.depthCompare = CompareFunction::always;
    desc.depthStencil.depthWriteEnabled = false;
    desc.bindGroupLayouts = layouts;
    desc.bindGroupLayoutCount = 3;
    desc.label = "ore_d3d12_lifetime_pipeline";
    pipeline = ctx.makePipeline(desc);

    SamplerDesc samplerDesc{};
    samplerDesc.minFilter = Filter::nearest;
    samplerDesc.magFilter = Filter::nearest;
    sampler = ctx.makeSampler(samplerDesc);

    BindGroupDesc samplerGroupDesc{};
    samplerGroupDesc.layout = samplerLayout.get();
    BindGroupDesc::SampEntry entry{};
    entry.slot = 0;
    entry.sampler = sampler.get();
    samplerGroupDesc.samplers = &entry;
    samplerGroupDesc.samplerCount = 1;
    samplerGroup = ctx.makeBindGroup(samplerGroupDesc);
    return pipeline != nullptr && samplerGroup != nullptr;
  }

  rcp<BindGroup> textureGroup(Context& ctx, TextureView* view) const
  {
    BindGroupDesc desc{};
    desc.layout = textureLayout.get();
    BindGroupDesc::TexEntry entry{};
    entry.slot = 0;
    entry.view = view;
    desc.textures = &entry;
    desc.textureCount = 1;
    return ctx.makeBindGroup(desc);
  }

  // One pass that clears the target to clearColor and draws the texture.
  void draw(
      Context& ctx,
      TextureView* target,
      BindGroup* textureGroup,
      int width,
      const float clearColor[4]) const
  {
    ColorAttachment ca{};
    ca.view = target;
    ca.loadOp = LoadOp::clear;
    ca.storeOp = StoreOp::store;
    ca.clearColor =
        {clearColor[0], clearColor[1], clearColor[2], clearColor[3]};
    RenderPassDesc desc{};
    desc.colorAttachments[0] = ca;
    desc.colorCount = 1;
    desc.label = "ore_d3d12_lifetime_pass";
    auto pass = ctx.beginRenderPass(desc);
    pass->setPipeline(pipeline.get());
    pass->setBindGroup(1, textureGroup);
    pass->setBindGroup(2, samplerGroup.get());
    pass->setViewport(0, 0, width, kH);
    pass->draw(6);
    pass->finish();
  }
};

// A 1x1 texture of one color, uploaded once an Ore frame is open.
struct SolidTexture
{
  rcp<Texture> texture;
  rcp<TextureView> view;
  uint8_t pixel[4];

  bool init(Context& ctx, uint8_t r, uint8_t g, uint8_t b)
  {
    pixel[0] = r;
    pixel[1] = g;
    pixel[2] = b;
    pixel[3] = 0xff;
    TextureDesc desc{};
    desc.width = 1;
    desc.height = 1;
    desc.format = TextureFormat::rgba8unorm;
    desc.type = TextureType::texture2D;
    desc.numMipmaps = 1;
    desc.sampleCount = 1;
    texture = ctx.makeTexture(desc);
    if (texture == nullptr)
    {
      return false;
    }
    TextureViewDesc viewDesc{};
    viewDesc.texture = texture.get();
    viewDesc.dimension = TextureViewDimension::texture2D;
    viewDesc.mipCount = 1;
    viewDesc.layerCount = 1;
    view = ctx.makeTextureView(viewDesc);
    return view != nullptr;
  }

  void upload()
  {
    TextureDataDesc data{};
    data.data = pixel;
    data.width = 1;
    data.height = 1;
    data.bytesPerRow = 4;
    data.rowsPerImage = 1;
    texture->upload(data);
  }
};

rcp<TextureView> makeView(Context& ctx, TextureFormat format, bool renderTarget)
{
  TextureDesc desc{};
  desc.width = 4;
  desc.height = 4;
  desc.format = format;
  desc.type = TextureType::texture2D;
  desc.renderTarget = renderTarget;
  desc.numMipmaps = 1;
  desc.sampleCount = 1;
  auto texture = ctx.makeTexture(desc);
  if (texture == nullptr)
  {
    return nullptr;
  }
  TextureViewDesc viewDesc{};
  viewDesc.texture = texture.get();
  viewDesc.dimension = TextureViewDimension::texture2D;
  viewDesc.mipCount = 1;
  viewDesc.layerCount = 1;
  return ctx.makeTextureView(viewDesc);
}

// Runs an empty host frame, so released objects can reach their safe frame.
void emptyFrame(TestingWindow* window)
{
  auto renderer = window->beginFrame({
      .name = "ore_d3d12_lifetime_empty",
      .clearColor = 0xff000000,
      .doClear = true,
  });
  std::vector<uint8_t> pixels;
  window->endFrame(&pixels);
}
} // namespace

TEST_CASE(
    "ore d3d12 keeps making views and samplers past its first heaps",
    "[ore][d3d12]")
{
  auto window = makeWindow();
  if (window == nullptr)
  {
    WARN("no D3D12 Ore backend available headless; skipping");
    return;
  }
  auto* renderContext = window->renderContext();
  ore_gm::OreGMContext oreGM;
  REQUIRE(oreGM.ensureContext(renderContext));
  auto& ctx = *renderContext->getOreContext();
  auto* ctx12 = static_cast<ContextD3D12*>(&ctx);

  // Luau's GC can keep hundreds alive at once.
  size_t heaps[2] = {};
  for (int round = 0; round < 2; ++round)
  {
    INFO("round " << round);
    ctx.clearLastError();
    std::vector<rcp<TextureView>> depthViews;
    std::vector<rcp<TextureView>> colorTargets;
    std::vector<rcp<Sampler>> samplers;
    for (int i = 0; i < 1100; ++i)
    {
      auto view = makeView(ctx, TextureFormat::depth32float, true);
      REQUIRE(view != nullptr);
      depthViews.push_back(std::move(view));
    }
    for (int i = 0; i < 300; ++i)
    {
      auto view = makeView(ctx, TextureFormat::rgba8unorm, true);
      REQUIRE(view != nullptr);
      colorTargets.push_back(std::move(view));
      SamplerDesc samplerDesc{};
      samplerDesc.minFilter = (i % 2) != 0 ? Filter::linear : Filter::nearest;
      auto sampler = ctx.makeSampler(samplerDesc);
      REQUIRE(sampler != nullptr);
      samplers.push_back(std::move(sampler));
    }
    CHECK(ctx.lastError().empty());
    heaps[round] = ctx12->d3d12CpuHeapCount();

    depthViews.clear();
    colorTargets.clear();
    samplers.clear();
    for (int frame = 0; frame < 3; ++frame)
    {
      emptyFrame(window.get());
    }
  }
  // Freed slots came back, so the second round needed no new heaps.
  CHECK(heaps[1] == heaps[0]);
}

TEST_CASE(
    "ore d3d12 beginFrame moves the shared resource manager",
    "[ore][d3d12]")
{
  auto window = makeWindow();
  if (window == nullptr)
  {
    WARN("no D3D12 Ore backend available headless; skipping");
    return;
  }
  auto* renderContext = window->renderContext();
  ore_gm::OreGMContext oreGM;
  REQUIRE(oreGM.ensureContext(renderContext));
  auto* ctx = static_cast<ContextD3D12*>(renderContext->getOreContext());
  auto manager = renderContext->static_impl_cast<gpu::RenderContextD3D12Impl>()
                     ->manager();

  // Released resources are stamped with the manager's frame, so it has to
  // be Ore's, not the last 2D flush's.
  for (int frame = 0; frame < 3; ++frame)
  {
    INFO("frame " << frame);
    auto renderer = window->beginFrame({
        .name = "ore_d3d12_manager_frame",
        .clearColor = 0xff000000,
        .doClear = true,
    });
    oreGM.beginFrame(renderContext);
    CHECK(manager->currentFrameNumber() == ctx->currentFrameNumber());
    // Core's flush may already know a later safe frame, which stays.
    CHECK(manager->safeFrameNumber() >= ctx->safeFrameNumber());
    oreGM.endFrame(renderContext);
    std::vector<uint8_t> pixels;
    window->endFrame(&pixels);
  }
}

TEST_CASE(
    "ore d3d12 beginFrame keeps each manager number moving forward",
    "[ore][d3d12]")
{
  auto window = makeWindow();
  if (window == nullptr)
  {
    WARN("no D3D12 Ore backend available headless; skipping");
    return;
  }
  auto* renderContext = window->renderContext();
  ore_gm::OreGMContext oreGM;
  REQUIRE(oreGM.ensureContext(renderContext));
  auto& ctx = *renderContext->getOreContext();
  auto* ctx12 = static_cast<ContextD3D12*>(&ctx);
  auto manager = renderContext->static_impl_cast<gpu::RenderContextD3D12Impl>()
                     ->manager();

  auto renderer = window->beginFrame({
      .name = "ore_d3d12_manager_forward",
      .clearColor = 0xff000000,
      .doClear = true,
  });
  oreGM.beginFrame(renderContext);
  const uint64_t current = ctx12->currentFrameNumber();
  // Deliberately stale: the harness waits for the GPU on resize, so its own
  // safe frame is always current - 1.
  const uint64_t safe = 0;
  REQUIRE(current >= safe + 2);

  // The manager knows a later safe frame than the host passes next, as
  // after core's flush.
  manager->advanceFrameNumber(current, current - 1);

  // A newer current with an older safe still moves the current, and keeps
  // the manager's safe.
  ctx.clearLastError();
  Context::FrameDescriptor newer{};
  newer.externalCommandBuffer = window->getCurrentCommandBuffer();
  newer.currentFrameNumber = current + 1;
  newer.safeFrameNumber = safe;
  ctx.beginFrame(newer);
  CHECK(ctx.lastError().empty());
  CHECK(manager->currentFrameNumber() == current + 1);
  CHECK(manager->safeFrameNumber() == current - 1);

  // A frame the manager already counts as finished is refused.
  Context::FrameDescriptor finished = newer;
  finished.currentFrameNumber = current - 1;
  ctx.beginFrame(finished);
  CHECK(!ctx.lastError().empty());
  CHECK(manager->currentFrameNumber() == current + 1);

  oreGM.endFrame(renderContext);
  std::vector<uint8_t> pixels;
  window->endFrame(&pixels);
}

TEST_CASE(
    "ore d3d12 frames recorded into one list keep their own descriptors",
    "[ore][d3d12]")
{
  auto window = makeWindow();
  if (window == nullptr)
  {
    WARN("no D3D12 Ore backend available headless; skipping");
    return;
  }
  auto* renderContext = window->renderContext();
  ore_gm::OreGMContext oreGM;
  REQUIRE(oreGM.ensureContext(renderContext));
  auto& ctx = *renderContext->getOreContext();
  auto* ctx12 = static_cast<ContextD3D12*>(&ctx);

  TextureDraw drawer;
  REQUIRE(drawer.init(ctx));
  SolidTexture red;
  SolidTexture green;
  REQUIRE(red.init(ctx, 0xff, 0, 0));
  REQUIRE(green.init(ctx, 0, 0xff, 0));
  auto redGroup = drawer.textureGroup(ctx, red.view.get());
  auto greenGroup = drawer.textureGroup(ctx, green.view.get());
  REQUIRE(redGroup != nullptr);
  REQUIRE(greenGroup != nullptr);

  auto left = renderContext->makeRenderCanvas(kW / 2, kH);
  auto right = renderContext->makeRenderCanvas(kW / 2, kH);
  REQUIRE(left != nullptr);
  REQUIRE(right != nullptr);
  auto leftTarget = ctx.wrapCanvasTexture(left.get());
  auto rightTarget = ctx.wrapCanvasTexture(right.get());
  REQUIRE(leftTarget != nullptr);
  REQUIRE(rightTarget != nullptr);

  const float black[4] = {0, 0, 0, 1};
  for (int frame = 0; frame < 3; ++frame)
  {
    INFO("frame " << frame);
    auto renderer = window->beginFrame({
        .name = "ore_d3d12_two_ore_frames",
        .clearColor = 0xff000000,
        .doClear = true,
    });
    oreGM.beginFrame(renderContext);
    red.upload();
    green.upload();
    drawer.draw(ctx, leftTarget.get(), redGroup.get(), kW / 2, black);

    // A second Ore frame into the same open list, like a host replaying
    // queued frames before submitting.
    Context::FrameDescriptor again{};
    again.externalCommandBuffer = window->getCurrentCommandBuffer();
    again.currentFrameNumber = ctx12->currentFrameNumber();
    again.safeFrameNumber = ctx12->safeFrameNumber();
    REQUIRE(again.externalCommandBuffer != nullptr);
    ctx.beginFrame(again);
    drawer.draw(ctx, rightTarget.get(), greenGroup.get(), kW / 2, black);

    oreGM.endFrame(renderContext);
    ore_gm::invalidateGLStateAfterOre(renderContext);

    renderer->drawImage(
        left->renderImage(),
        {.filter = ImageFilter::nearest},
        BlendMode::srcOver,
        1);
    renderer->save();
    renderer->transform(Mat2D::fromTranslate(kW / 2.0f, 0));
    renderer->drawImage(
        right->renderImage(),
        {.filter = ImageFilter::nearest},
        BlendMode::srcOver,
        1);
    renderer->restore();
    std::vector<uint8_t> pixels;
    window->endFrame(&pixels);
    REQUIRE(pixels.size() == static_cast<size_t>(kW) * kH * 4);

    const uint8_t* l = pixelAt(pixels, kW / 4, kH / 2);
    const uint8_t* r = pixelAt(pixels, 3 * kW / 4, kH / 2);
    INFO(
        "left=" << +l[0] << "," << +l[1] << "," << +l[2] << " right=" << +r[0]
                << "," << +r[1] << "," << +r[2]);
    // A shared heap reset at beginFrame would make the left green.
    CHECK(high(l[0]));
    CHECK(low(l[1]));
    CHECK(low(r[0]));
    CHECK(high(r[1]));
  }
}

TEST_CASE(
    "ore d3d12 skips binds and draws after a refused pipeline",
    "[ore][d3d12]")
{
  auto window = makeWindow();
  if (window == nullptr)
  {
    WARN("no D3D12 Ore backend available headless; skipping");
    return;
  }
  auto* renderContext = window->renderContext();
  ore_gm::OreGMContext oreGM;
  REQUIRE(oreGM.ensureContext(renderContext));
  auto& ctx = *renderContext->getOreContext();

  // As when a pass's depth view went missing.
  TextureDraw drawer;
  REQUIRE(drawer.init(ctx, /*withDepth=*/true));
  SolidTexture red;
  REQUIRE(red.init(ctx, 0xff, 0, 0));
  auto redGroup = drawer.textureGroup(ctx, red.view.get());
  REQUIRE(redGroup != nullptr);

  auto canvas = renderContext->makeRenderCanvas(kW, kH);
  REQUIRE(canvas != nullptr);
  auto target = ctx.wrapCanvasTexture(canvas.get());
  REQUIRE(target != nullptr);

  auto renderer = window->beginFrame({
      .name = "ore_d3d12_refused_pipeline",
      .clearColor = 0xff000000,
      .doClear = true,
  });
  oreGM.beginFrame(renderContext);
  red.upload();
  ctx.clearLastError();
  const float blue[4] = {0, 0, 1, 1};
  drawer.draw(ctx, target.get(), redGroup.get(), kW, blue);
  CHECK(!ctx.lastError().empty());
  oreGM.endFrame(renderContext);
  ore_gm::invalidateGLStateAfterOre(renderContext);

  renderer->drawImage(
      canvas->renderImage(),
      {.filter = ImageFilter::nearest},
      BlendMode::srcOver,
      1);
  std::vector<uint8_t> pixels;
  window->endFrame(&pixels);
  REQUIRE(pixels.size() == static_cast<size_t>(kW) * kH * 4);

  // The pass still clears, and the refused draw leaves nothing behind.
  const uint8_t* center = pixelAt(pixels, kW / 2, kH / 2);
  INFO("center=" << +center[0] << "," << +center[1] << "," << +center[2]);
  CHECK(low(center[0]));
  CHECK(high(center[2]));
}
TEST_CASE(
    "ore d3d12 refuses a frame that isn't past its safe frame",
    "[ore][d3d12]")
{
  auto window = makeWindow();
  if (window == nullptr)
  {
    WARN("no D3D12 Ore backend available headless; skipping");
    return;
  }
  auto* renderContext = window->renderContext();
  ore_gm::OreGMContext oreGM;
  REQUIRE(oreGM.ensureContext(renderContext));
  auto& ctx = *renderContext->getOreContext();
  auto* ctx12 = static_cast<ContextD3D12*>(&ctx);

  auto canvas = renderContext->makeRenderCanvas(kW, kH);
  REQUIRE(canvas != nullptr);
  auto target = ctx.wrapCanvasTexture(canvas.get());
  REQUIRE(target != nullptr);

  auto renderer = window->beginFrame({
      .name = "ore_d3d12_untracked_frame",
      .clearColor = 0xff000000,
      .doClear = true,
  });
  oreGM.beginFrame(renderContext);

  // Nothing could tell when this frame's heaps are free, so a second frame
  // in the same list might get the first one's.
  const uint64_t current = ctx12->currentFrameNumber();
  const uint64_t safe = ctx12->safeFrameNumber();
  for (uint64_t frame : {current, uint64_t(0)})
  {
    INFO("frame " << frame);
    ctx.clearLastError();
    Context::FrameDescriptor untracked{};
    untracked.externalCommandBuffer = window->getCurrentCommandBuffer();
    untracked.currentFrameNumber = frame;
    untracked.safeFrameNumber = frame;
    ctx.beginFrame(untracked);
    CHECK(!ctx.lastError().empty());
    // Buffers recycle backings by these, so a refused frame can't move them.
    CHECK(ctx12->currentFrameNumber() == current);
    CHECK(ctx12->safeFrameNumber() == safe);

    ColorAttachment ca{};
    ca.view = target.get();
    ca.loadOp = LoadOp::clear;
    ca.storeOp = StoreOp::store;
    RenderPassDesc desc{};
    desc.colorAttachments[0] = ca;
    desc.colorCount = 1;
    CHECK(ctx.beginRenderPass(desc) == nullptr);
  }

  oreGM.endFrame(renderContext);
  std::vector<uint8_t> pixels;
  window->endFrame(&pixels);
}

TEST_CASE(
    "ore d3d12 moves to a fresh heap when one fills mid pass",
    "[ore][d3d12]")
{
  auto window = makeWindow();
  if (window == nullptr)
  {
    WARN("no D3D12 Ore backend available headless; skipping");
    return;
  }
  auto* renderContext = window->renderContext();
  ore_gm::OreGMContext oreGM;
  REQUIRE(oreGM.ensureContext(renderContext));
  auto& ctx = *renderContext->getOreContext();

  TextureDraw drawer;
  REQUIRE(drawer.init(ctx));
  SolidTexture red;
  REQUIRE(red.init(ctx, 0xff, 0, 0));
  auto redGroup = drawer.textureGroup(ctx, red.view.get());
  REQUIRE(redGroup != nullptr);

  auto canvas = renderContext->makeRenderCanvas(kW, kH);
  REQUIRE(canvas != nullptr);
  auto target = ctx.wrapCanvasTexture(canvas.get());
  REQUIRE(target != nullptr);

  auto renderer = window->beginFrame({
      .name = "ore_d3d12_heap_rollover",
      .clearColor = 0xff000000,
      .doClear = true,
  });
  oreGM.beginFrame(renderContext);
  red.upload();

  ColorAttachment ca{};
  ca.view = target.get();
  ca.loadOp = LoadOp::clear;
  ca.storeOp = StoreOp::store;
  ca.clearColor = {0, 0, 1, 1};
  RenderPassDesc passDesc{};
  passDesc.colorAttachments[0] = ca;
  passDesc.colorCount = 1;
  auto pass = ctx.beginRenderPass(passDesc);
  REQUIRE(pass != nullptr);

  // More binds than either heap holds, so each fills and the other group's
  // table, set before the swap, has to be set again for the draw.
  pass->setPipeline(drawer.pipeline.get());
  ctx.clearLastError();
  pass->setBindGroup(1, redGroup.get());
  for (int i = 0; i < 600; ++i)
  {
    pass->setBindGroup(2, drawer.samplerGroup.get());
  }
  for (int i = 0; i < 4100; ++i)
  {
    pass->setBindGroup(1, redGroup.get());
  }
  pass->setViewport(0, 0, kW, kH);
  pass->draw(6);
  pass->finish();
  CHECK(ctx.lastError().empty());

  oreGM.endFrame(renderContext);
  ore_gm::invalidateGLStateAfterOre(renderContext);
  renderer->drawImage(
      canvas->renderImage(),
      {.filter = ImageFilter::nearest},
      BlendMode::srcOver,
      1);
  std::vector<uint8_t> pixels;
  window->endFrame(&pixels);
  REQUIRE(pixels.size() == static_cast<size_t>(kW) * kH * 4);

  const uint8_t* center = pixelAt(pixels, kW / 2, kH / 2);
  INFO("center=" << +center[0] << "," << +center[1] << "," << +center[2]);
  CHECK(high(center[0]));
  CHECK(low(center[2]));
}

TEST_CASE("ore d3d12 keeps bind groups across pipelines", "[ore][d3d12]")
{
  auto window = makeWindow();
  if (window == nullptr)
  {
    WARN("no D3D12 Ore backend available headless; skipping");
    return;
  }
  auto* renderContext = window->renderContext();
  ore_gm::OreGMContext oreGM;
  REQUIRE(oreGM.ensureContext(renderContext));
  auto& ctx = *renderContext->getOreContext();

  // Two pipelines from the same shader still have their own root signatures.
  TextureDraw first;
  TextureDraw second;
  REQUIRE(first.init(ctx));
  REQUIRE(second.init(ctx));
  SolidTexture red;
  REQUIRE(red.init(ctx, 0xff, 0, 0));
  auto redGroup = first.textureGroup(ctx, red.view.get());
  REQUIRE(redGroup != nullptr);

  auto canvas = renderContext->makeRenderCanvas(kW, kH);
  REQUIRE(canvas != nullptr);
  auto target = ctx.wrapCanvasTexture(canvas.get());
  REQUIRE(target != nullptr);

  enum class Order
  {
    switchPipeline,
    bindBeforePipeline,
    nullPipeline,
  };
  for (Order order :
       {Order::switchPipeline, Order::bindBeforePipeline, Order::nullPipeline})
  {
    INFO("order " << static_cast<int>(order));
    auto renderer = window->beginFrame({
        .name = "ore_d3d12_groups_across_pipelines",
        .clearColor = 0xff000000,
        .doClear = true,
    });
    oreGM.beginFrame(renderContext);
    red.upload();

    ColorAttachment ca{};
    ca.view = target.get();
    ca.loadOp = LoadOp::clear;
    ca.storeOp = StoreOp::store;
    ca.clearColor = {0, 0, 1, 1};
    RenderPassDesc passDesc{};
    passDesc.colorAttachments[0] = ca;
    passDesc.colorCount = 1;
    auto pass = ctx.beginRenderPass(passDesc);
    REQUIRE(pass != nullptr);
    if (order != Order::bindBeforePipeline)
    {
      pass->setPipeline(first.pipeline.get());
    }
    pass->setBindGroup(1, redGroup.get());
    pass->setBindGroup(2, first.samplerGroup.get());
    switch (order)
    {
      case Order::switchPipeline:
        pass->setPipeline(second.pipeline.get());
        break;
      case Order::bindBeforePipeline:
        pass->setPipeline(first.pipeline.get());
        break;
      case Order::nullPipeline:
        pass->setPipeline(nullptr);
        break;
    }
    pass->setViewport(0, 0, kW, kH);
    pass->draw(6);
    pass->finish();

    oreGM.endFrame(renderContext);
    ore_gm::invalidateGLStateAfterOre(renderContext);
    renderer->drawImage(
        canvas->renderImage(),
        {.filter = ImageFilter::nearest},
        BlendMode::srcOver,
        1);
    std::vector<uint8_t> pixels;
    window->endFrame(&pixels);
    REQUIRE(pixels.size() == static_cast<size_t>(kW) * kH * 4);

    // Only a null pipeline skips the draw, leaving the clear.
    const bool drawn = order != Order::nullPipeline;
    const uint8_t* center = pixelAt(pixels, kW / 2, kH / 2);
    INFO("center=" << +center[0] << "," << +center[1] << "," << +center[2]);
    CHECK(high(center[0]) == drawn);
    CHECK(high(center[2]) != drawn);
  }
}
#endif // ORE_BACKEND_D3D12
