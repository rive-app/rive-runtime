/*
 * Copyright 2026 Rive
 *
 * WebGPU lets a pipeline use a layout reflected from another shader, so
 * backends must bind through the pipeline's own slots or refuse it.
 */

#include "common/testing_window.hpp"
#include "gm/ore_gm_helper.hpp"
#include <catch.hpp>

#if defined(ORE_BACKEND_METAL) || defined(ORE_BACKEND_VK) ||                   \
    defined(ORE_BACKEND_WGPU) || defined(ORE_BACKEND_D3D12) ||                 \
    defined(ORE_BACKEND_D3D11) || defined(ORE_BACKEND_GL)
#include "rive/renderer/render_context.hpp"
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/ore/ore_bind_group_layout.hpp"
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_render_pass.hpp"
#include "rive/renderer/ore/ore_shader_module.hpp"
#include "rive/renderer/ore/ore_texture.hpp"
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#ifdef __APPLE__
#include <dlfcn.h>
#endif
#define ORE_LAYOUT_NATIVE_SLOTS_TEST_ACTIVE
#endif

#ifdef ORE_LAYOUT_NATIVE_SLOTS_TEST_ACTIVE
using namespace rive;
using namespace rive::gpu;
using namespace rive::ore;

namespace
{
const TestingWindow::Backend kOreBackends[] = {
#if defined(ORE_BACKEND_METAL)
    TestingWindow::Backend::metal,
#endif
#if defined(ORE_BACKEND_D3D11)
    TestingWindow::Backend::d3d,
#endif
#if defined(ORE_BACKEND_D3D12)
    TestingWindow::Backend::d3d12,
#endif
#if defined(ORE_BACKEND_VK)
    TestingWindow::Backend::vk,
#endif
#if defined(ORE_BACKEND_GL)
#if defined(__APPLE__)
    TestingWindow::Backend::angle,
#else
    TestingWindow::Backend::gl,
#endif
#endif
#if defined(ORE_BACKEND_WGPU)
    TestingWindow::Backend::dawn,
#endif
};

// Mirrors ore_split_stage_test.cpp: `TestingWindow::Init` aborts on a backend
// it cannot bring up.
bool backendCanStart(TestingWindow::Backend backend)
{
  if (backend == TestingWindow::Backend::gl)
  {
    return getenv("DISPLAY") != nullptr || getenv("WAYLAND_DISPLAY") != nullptr;
  }
#ifdef __APPLE__
  if (backend == TestingWindow::Backend::angle)
  {
    return dlopen("libEGL.dylib", RTLD_LAZY) != nullptr;
  }
#endif
  return true;
}

constexpr int kSize = 32;
constexpr uint8_t kColorTexel[4] = {0x33, 0xcc, 0x66, 0xff};
// Distinct and combined unevenly, so swapped buffers show.
constexpr float kFrameTint[4] = {0.f, 0.f, 0.4f, 0.f};
constexpr float kMaterialColor[4] = {1.f, 0.5f, 1.f, 1.f};
constexpr uint8_t kExpected[3] = {0x33, 0x66, 0xcc};
constexpr uint8_t kEmissiveTexel[4] = {0xff, 0x00, 0xff, 0xff};

bool channelNear(uint8_t a, uint8_t b)
{
  return (a > b ? a - b : b - a) <= 0x08;
}

rcp<Buffer> makeUBO(Context& ctx, const float (&value)[4])
{
  BufferDesc desc{};
  desc.usage = BufferUsage::uniform;
  desc.size = sizeof(value);
  desc.data = value;
  return ctx.makeBuffer(desc);
}

rcp<TextureView> makeSolidTexture(Context& ctx, const uint8_t (&texel)[4])
{
  TextureDesc desc{};
  desc.width = 2;
  desc.height = 2;
  desc.format = TextureFormat::rgba8unorm;
  auto texture = ctx.makeTexture(desc);
  REQUIRE(texture != nullptr);
  uint8_t pixels[2 * 2 * 4];
  for (int i = 0; i < 4; ++i)
    memcpy(pixels + i * 4, texel, 4);
  TextureDataDesc data{};
  data.data = pixels;
  data.dataSize = sizeof(pixels);
  data.bytesPerRow = 2 * 4;
  data.width = 2;
  data.height = 2;
  std::string error;
  const bool uploaded = texture->upload(data, &error);
  INFO("upload: " << error);
  REQUIRE(uploaded);
  TextureViewDesc view{};
  view.texture = texture.get();
  return ctx.makeTextureView(view);
}

std::vector<uint8_t> readCanvas(TestingWindow* window, RenderCanvas* canvas)
{
  auto renderer = window->beginFrame({
      .name = "ore_layout_native_slots_read",
      .clearColor = 0xff000000,
      .doClear = true,
  });
  renderer->drawImage(
      canvas->renderImage(),
      {.filter = ImageFilter::nearest},
      BlendMode::srcOver,
      1);
  std::vector<uint8_t> pixels;
  window->endFrame(&pixels);
  return pixels;
}

void runScenario(TestingWindow* window, TestingWindow::Backend backend)
{
  window->resize(kSize, kSize);
  auto* renderContext = window->renderContext();
  ore_gm::OreGMContext oreGM;
  REQUIRE(oreGM.ensureContext(renderContext));
  auto& ctx = *renderContext->getOreContext();

  auto lit = ore_gm::loadShader(ctx, ore_gm::kLayoutSlotsLit);
  auto shadow = ore_gm::loadShader(ctx, ore_gm::kLayoutSlotsShadow);
  REQUIRE(lit.psModule);
  REQUIRE(shadow.vsModule);
  REQUIRE(shadow.psModule);

  // The shadow pipeline takes its material layout from the lit shader.
  auto frameLayout = makeBindGroupLayoutFromShader(
      ctx,
      shadow.psModule.get(),
      0);
  auto materialLayout = makeBindGroupLayoutFromShader(
      ctx,
      lit.psModule.get(),
      1);
  REQUIRE(frameLayout != nullptr);
  REQUIRE(materialLayout != nullptr);
  REQUIRE(materialLayout->entries().size() == 4);

  BindGroupLayout* layouts[] = {frameLayout.get(), materialLayout.get()};
  PipelineDesc desc{};
  desc.vertexModule = shadow.vsModule.get();
  desc.fragmentModule = shadow.psModule.get();
  desc.vertexEntryPoint = shadow.vsEntryPoint;
  desc.fragmentEntryPoint = shadow.fsEntryPoint;
  desc.topology = PrimitiveTopology::triangleList;
  desc.colorTargets[0].format = TextureFormat::rgba8unorm;
  desc.colorCount = 1;
  desc.depthStencil.depthCompare = CompareFunction::always;
  desc.depthStencil.depthWriteEnabled = false;
  desc.bindGroupLayouts = layouts;
  desc.bindGroupLayoutCount = 2;
  desc.label = "ore_layout_native_slots_shadow";

  std::string pipelineError;
  auto pipeline = ctx.makePipeline(desc, &pipelineError);
  // D3D binds at the layout's slots, so it refuses this pipeline instead.
  if (backend == TestingWindow::Backend::d3d ||
      backend == TestingWindow::Backend::d3d12)
  {
    INFO("makePipeline: " << pipelineError);
    REQUIRE(pipeline == nullptr);
    CHECK(pipelineError.find("@group(1) @binding(0)") != std::string::npos);
    CHECK(
        pipelineError.find("build it from this pipeline's own shader") !=
        std::string::npos);
    return;
  }
  INFO("makePipeline: " << pipelineError);
  REQUIRE(pipeline != nullptr);

  auto frameUBO = makeUBO(ctx, kFrameTint);
  auto materialUBO = makeUBO(ctx, kMaterialColor);
  auto colorMap = makeSolidTexture(ctx, kColorTexel);
  auto emissiveMap = makeSolidTexture(ctx, kEmissiveTexel);
  auto sampler = ctx.makeSampler(SamplerDesc{});
  REQUIRE(frameUBO != nullptr);
  REQUIRE(materialUBO != nullptr);
  REQUIRE(sampler != nullptr);

  BindGroupDesc::UBOEntry frameEntry{};
  frameEntry.slot = 0;
  frameEntry.buffer = frameUBO.get();
  frameEntry.size = sizeof(kFrameTint);
  BindGroupDesc frameDesc{};
  frameDesc.layout = frameLayout.get();
  frameDesc.ubos = &frameEntry;
  frameDesc.uboCount = 1;
  auto frameGroup = ctx.makeBindGroup(frameDesc);
  INFO("makeBindGroup frame: " << ctx.lastError());
  REQUIRE(frameGroup != nullptr);

  BindGroupDesc::UBOEntry materialEntry{};
  materialEntry.slot = 2;
  materialEntry.buffer = materialUBO.get();
  materialEntry.size = sizeof(kMaterialColor);
  BindGroupDesc::TexEntry textures[2]{};
  textures[0].slot = 0;
  textures[0].view = colorMap.get();
  textures[1].slot = 3;
  textures[1].view = emissiveMap.get();
  BindGroupDesc::SampEntry samplerEntry{};
  samplerEntry.slot = 1;
  samplerEntry.sampler = sampler.get();
  BindGroupDesc materialDesc{};
  materialDesc.layout = materialLayout.get();
  materialDesc.ubos = &materialEntry;
  materialDesc.uboCount = 1;
  materialDesc.textures = textures;
  materialDesc.textureCount = 2;
  materialDesc.samplers = &samplerEntry;
  materialDesc.samplerCount = 1;
  auto materialGroup = ctx.makeBindGroup(materialDesc);
  INFO("makeBindGroup material: " << ctx.lastError());
  REQUIRE(materialGroup != nullptr);

  // A pipeline that binds nothing, so the groups first apply against slots
  // that differ from the shadow pipeline's.
  auto triangle = ore_gm::loadShader(ctx, ore_gm::kTriangle);
  REQUIRE(triangle.vsModule);
  ore_gm::TrianglePipeline unboundDesc(
      triangle,
      TextureFormat::rgba8unorm,
      "ore_layout_native_slots_unbound");
  auto unbound = ctx.makePipeline(unboundDesc.desc, &pipelineError);
  INFO("makePipeline unbound: " << pipelineError);
  REQUIRE(unbound != nullptr);

  auto canvas = renderContext->makeRenderCanvas(kSize, kSize);
  REQUIRE(canvas != nullptr);
  auto target = ctx.wrapCanvasTexture(canvas.get());
  REQUIRE(target != nullptr);

  auto renderer = window->beginFrame({
      .name = "ore_layout_native_slots",
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
  passDesc.label = "ore_layout_native_slots_pass";
  auto pass = ctx.beginRenderPass(passDesc);
  REQUIRE(pass != nullptr);
  // Vulkan and D3D12 record a bind against the bound pipeline's layout, so
  // only the other backends take groups ahead of it.
  if (backend == TestingWindow::Backend::vk ||
      backend == TestingWindow::Backend::d3d12)
  {
    pass->setPipeline(pipeline.get());
    pass->setBindGroup(1, materialGroup.get());
    pass->setBindGroup(0, frameGroup.get());
  }
  else
  {
    pass->setBindGroup(1, materialGroup.get());
    pass->setPipeline(unbound.get());
    pass->setBindGroup(0, frameGroup.get());
    pass->setPipeline(pipeline.get());
  }
  pass->setViewport(0, 0, kSize, kSize);
  pass->draw(3);
  pass->finish();
  oreGM.endFrame(renderContext);
  ore_gm::invalidateGLStateAfterOre(renderContext);
  window->endFrame();

  auto px = readCanvas(window, canvas.get());
  REQUIRE(px.size() == static_cast<size_t>(kSize) * kSize * 4);
  const uint8_t*
      p = &px[(static_cast<size_t>(kSize / 2) * kSize + kSize / 2) * 4];
  INFO("center pixel = " << +p[0] << "," << +p[1] << "," << +p[2]);
  CHECK(channelNear(p[0], kExpected[0]));
  CHECK(channelNear(p[1], kExpected[1]));
  CHECK(channelNear(p[2], kExpected[2]));
}
} // namespace

TEST_CASE("ore binds a layout reflected from another shader", "[ore]")
{
  int ran = 0;
  for (auto backend : kOreBackends)
  {
    if (!backendCanStart(backend))
      continue;
    auto* window = TestingWindow::Init(
        backend,
        {},
        TestingWindow::Visibility::headless);
    if (window == nullptr || window->renderContext() == nullptr ||
        !ore_gm::isOreBackendActive())
    {
      TestingWindow::Destroy();
      continue;
    }
    INFO("backend " << TestingWindow::BackendName(backend));
    runScenario(window, backend);
    ++ran;
    TestingWindow::Destroy();
  }
  if (ran == 0)
    WARN("no Ore backend available headless; skipping");
}
#endif // ORE_LAYOUT_NATIVE_SLOTS_TEST_ACTIVE
