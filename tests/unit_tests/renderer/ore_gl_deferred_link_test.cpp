/*
 * Copyright 2026 Rive
 */

// Replay links scripted pipelines in the background on GL and finishes each
// one at the first pass that binds it.

#include "ore_test_backends.hpp"

#if defined(ORE_BACKEND_GL) && defined(RIVE_DESKTOP_GL)
#include "rive/renderer/gl/gles3.hpp"
#include "rive/renderer/render_context.hpp"
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/renderer/ore/ore_render_pass.hpp"
#include "rive/renderer/ore/cmd/ore_deferred_context.hpp"
#include "rive/renderer/ore/cmd/ore_deferred_render_pass.hpp"
#include <array>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace rive;
using namespace rive::ore;

namespace
{
// Counts the status reads that wait for the driver to finish a build.
struct StatusReads
{
  static inline int link = 0;
  static inline int compile = 0;
  static inline bool linking = false;
  static inline PFNGLGETPROGRAMIVPROC realProgramiv = nullptr;
  static inline PFNGLGETSHADERIVPROC realShaderiv = nullptr;

  static void GLAD_API_PTR programiv(GLuint p, GLenum name, GLint* out)
  {
    link += name == GL_LINK_STATUS;
    if (linking && name == GL_COMPLETION_STATUS_KHR)
      *out = GL_FALSE;
    else
      realProgramiv(p, name, out);
  }
  static void GLAD_API_PTR shaderiv(GLuint s, GLenum name, GLint* out)
  {
    compile += name == GL_COMPILE_STATUS;
    realShaderiv(s, name, out);
  }

  // stillLinking reports every link as running, as a slow driver would.
  explicit StatusReads(bool stillLinking)
  {
    link = compile = 0;
    linking = stillLinking;
    realProgramiv = glad_glGetProgramiv;
    realShaderiv = glad_glGetShaderiv;
    glad_glGetProgramiv = programiv;
    glad_glGetShaderiv = shaderiv;
  }
  ~StatusReads()
  {
    glad_glGetProgramiv = realProgramiv;
    glad_glGetShaderiv = realShaderiv;
  }
};

constexpr uint32_t kSize = 16;

struct Frame
{
  ore_gm::OreGMContext* ore;
  TestingWindow* window;
  gpu::RenderContext* renderContext;
  gpu::RenderCanvas* canvas;
};

struct Drawn
{
  std::array<uint8_t, 4> center;
  int linkReads;
  int compileReads;
};

// Runs draw in an ore frame, then reads the canvas's center pixel.
template <typename Draw>
std::array<uint8_t, 4> canvasCenter(const Frame& f, Draw&& draw)
{
  auto renderer = f.window->beginFrame(
      {.name = "ore_gl_deferred_link", .clearColor = 0xff000000});
  f.ore->beginFrame(f.renderContext);
  draw();
  f.ore->endFrame(f.renderContext);
  ore_gm::invalidateGLStateAfterOre(f.renderContext);
  renderer->drawImage(
      f.canvas->renderImage(),
      {.filter = ImageFilter::nearest},
      BlendMode::srcOver,
      1);
  std::vector<uint8_t> pixels;
  f.window->endFrame(&pixels);
  REQUIRE(pixels.size() >= kSize * kSize * 4);
  const size_t at = ((kSize / 2) * kSize + kSize / 2) * 4;
  return {pixels[at], pixels[at + 1], pixels[at + 2], pixels[at + 3]};
}

// Replays what dctx recorded into the canvas, counting only replay's reads.
Drawn drawFrame(
    const Frame& f,
    ore::cmd::DeferredOreContext& dctx,
    ore::cmd::OreResident& table,
    bool stillLinking = false)
{
  Drawn drawn{};
  drawn.center = canvasCenter(f, [&] {
    {
      StatusReads reads(stillLinking);
      dctx.replayFrame(*f.renderContext->getOreContext(), table);
      drawn.linkReads = reads.link;
      drawn.compileReads = reads.compile;
    }
    dctx.resetFrame();
  });
  return drawn;
}

bool pixelNear(const std::array<uint8_t, 4>& px, int r, int g, int b)
{
  return std::abs(px[0] - r) <= 2 && std::abs(px[1] - g) <= 2 &&
         std::abs(px[2] - b) <= 2;
}

// The binding witness reads two UBOs whose GL block bindings only the
// post-link fixup assigns, so a bind that skipped it draws red, not olive.
struct WitnessScene
{
  rcp<Buffer> low, high;
  ore_gm::OreGMShaderResult shader;
  rcp<BindGroupLayout> layout;
  rcp<Pipeline> pipeline;
  rcp<BindGroup> group;

  explicit WitnessScene(Context& ctx)
  {
    const float lowColor[4] = {0.3f, 0, 0, 0};
    const float highColor[4] = {0, 0.6f, 0, 0};
    BufferDesc bd{};
    bd.usage = BufferUsage::uniform;
    bd.size = sizeof(lowColor);
    bd.data = lowColor;
    low = ctx.makeBuffer(bd);
    bd.data = highColor;
    high = ctx.makeBuffer(bd);
    shader = ore_gm::loadShader(ctx, ore_gm::kBindingWitness);
    REQUIRE(shader.vsModule);
    layout = ore_gm::makeLayoutFromShader(ctx, shader.vsModule.get(), 0);
    pipeline = makePipeline(ctx);
    REQUIRE(pipeline);
    BindGroupDesc::UBOEntry ubos[2]{};
    ubos[0].slot = 0;
    ubos[0].buffer = low.get();
    ubos[0].size = sizeof(lowColor);
    ubos[1].slot = 7;
    ubos[1].buffer = high.get();
    ubos[1].size = sizeof(highColor);
    BindGroupDesc gd{};
    gd.layout = layout.get();
    gd.ubos = ubos;
    gd.uboCount = 2;
    group = ctx.makeBindGroup(gd);
    REQUIRE(group);
  }

  rcp<Pipeline> makePipeline(Context& ctx) const
  {
    BindGroupLayout* layouts[] = {layout.get()};
    PipelineDesc pd{};
    pd.vertexModule = shader.vsModule.get();
    pd.fragmentModule = shader.psModule.get();
    pd.vertexEntryPoint = shader.vsEntryPoint;
    pd.fragmentEntryPoint = shader.fsEntryPoint;
    pd.colorTargets[0].format = TextureFormat::rgba8unorm;
    pd.colorCount = 1;
    pd.bindGroupLayouts = layouts;
    pd.bindGroupLayoutCount = 1;
    pd.label = "witness";
    return ctx.makePipeline(pd);
  }
};

void recordPass(
    Context& ctx,
    TextureView* target,
    Pipeline* pipeline,
    BindGroup* group)
{
  RenderPassDesc rp{};
  rp.colorAttachments[0].view = target;
  rp.colorAttachments[0].loadOp = LoadOp::clear;
  rp.colorAttachments[0].clearColor = {0, 0, 1, 1};
  rp.colorCount = 1;
  auto pass = ctx.beginRenderPass(rp);
  REQUIRE(pass);
  pass->setPipeline(pipeline);
  if (group != nullptr)
    pass->setBindGroup(0, group);
  pass->draw(3);
  pass->finish();
}

// Compiles on its own but fails to link: the varying types disagree.
rcp<Pipeline> makeUnlinkablePipeline(
    Context& ctx,
    std::vector<rcp<ShaderModule>>& keep,
    bool deferBuildCheck = false)
{
  static const char kVS[] = "#version 300 es\nout vec3 v;\n"
                            "void main() { v = vec3(1.0); "
                            "gl_Position = vec4(0.0, 0.0, 0.0, 1.0); }\n";
  static const char kFS[] = "#version 300 es\nprecision mediump float;\n"
                            "in vec4 v;\nout vec4 o;\nvoid main() { o = v; }\n";
  BindingMap empty;
  empty.finalize();
  const std::vector<uint8_t> map = empty.toBlob();
  auto module = [&](const char* code, ShaderStage stage) {
    ShaderModuleDesc d{};
    d.code = code;
    d.codeSize = static_cast<uint32_t>(strlen(code));
    d.stage = stage;
    d.bindingMapBytes = map.data();
    d.bindingMapSize = static_cast<uint32_t>(map.size());
    keep.push_back(ctx.makeShaderModule(d));
    return keep.back().get();
  };
  PipelineDesc pd{};
  pd.vertexModule = module(kVS, ShaderStage::vertex);
  pd.fragmentModule = module(kFS, ShaderStage::fragment);
  pd.vertexEntryPoint = "main";
  pd.fragmentEntryPoint = "main";
  pd.colorTargets[0].format = TextureFormat::rgba8unorm;
  pd.colorCount = 1;
  pd.label = "unlinkable";
  pd.deferBuildCheck = deferBuildCheck;
  return ctx.makePipeline(pd);
}

template <typename Scenario> void runScenario(Scenario&& scenario)
{
  static Scenario* s_scenario;
  s_scenario = &scenario;
  if (ore_test::runOnBackends(ore_test::kGLBackends, [](TestingWindow* w) {
        auto* renderContext = w->renderContext();
        ore_gm::OreGMContext ore;
        REQUIRE(ore.ensureContext(renderContext));
        Context& real = *renderContext->getOreContext();
        if (!real.defersBuildChecks())
        {
          WARN("no KHR_parallel_shader_compile; links stay synchronous");
          return;
        }
        w->resize(kSize, kSize);
        auto canvas = renderContext->makeRenderCanvas(kSize, kSize);
        REQUIRE(canvas);
        ore::cmd::DeferredOreContext dctx(&real);
        auto target = dctx.wrapCanvasTexture(canvas.get());
        REQUIRE(target);
        Frame f{&ore, w, renderContext, canvas.get()};
        (*s_scenario)(f, dctx, target.get());
      }) == 0)
    WARN("no Ore GL backend available headless; skipping");
}
} // namespace

TEST_CASE(
    "ore GL replay makes pipelines without waiting on the driver",
    "[ore][gl]")
{
  runScenario([](const Frame& f,
                 ore::cmd::DeferredOreContext& dctx,
                 TextureView* target) {
    ore::cmd::OreResident table;
    WitnessScene scene(dctx);
    const Drawn made = drawFrame(f, dctx, table);
    CHECK(made.linkReads == 0);
    CHECK(made.compileReads == 0);
    const auto id = dctx.handleFor(scene.pipeline.get());
    CHECK(table.getAs(id, ore::cmd::OreKind::pipeline) != nullptr);

    recordPass(dctx, target, scene.pipeline.get(), scene.group.get());
    const Drawn bound = drawFrame(f, dctx, table);
    CHECK(pixelNear(bound.center, 77, 153, 0));
    CHECK(bound.linkReads == 1);
  });
}

TEST_CASE(
    "ore GL replay draws a pipeline in the frame that makes it",
    "[ore][gl]")
{
  runScenario([](const Frame& f,
                 ore::cmd::DeferredOreContext& dctx,
                 TextureView* target) {
    ore::cmd::OreResident table;
    WitnessScene scene(dctx);
    recordPass(dctx, target, scene.pipeline.get(), scene.group.get());
    CHECK(pixelNear(drawFrame(f, dctx, table).center, 77, 153, 0));
  });
}

TEST_CASE("ore GL replay reports a failed link at its first bind", "[ore][gl]")
{
  runScenario([](const Frame& f,
                 ore::cmd::DeferredOreContext& dctx,
                 TextureView* target) {
    ore::cmd::OreResident table;
    std::vector<rcp<ShaderModule>> modules;
    auto pipeline = makeUnlinkablePipeline(dctx, modules);
    REQUIRE(pipeline);
    const auto id = dctx.handleFor(pipeline.get());
    CHECK(drawFrame(f, dctx, table).linkReads == 0);
    CHECK(table.failureNote(id) == nullptr);
    recordPass(dctx, target, pipeline.get(), nullptr);
    // The poll sees the link still running, so only the bind can settle it.
    const Drawn bound = drawFrame(f, dctx, table, true);
    CHECK(bound.linkReads == 1);
    // The pass's draws drop, so it keeps its clear.
    CHECK(pixelNear(bound.center, 0, 0, 255));
    CHECK(table.getAs(id, ore::cmd::OreKind::pipeline) == nullptr);
    const char* note = table.failureNote(id);
    REQUIRE(note != nullptr);
    CHECK(std::string(note).rfind("pipeline 'unlinkable' failed: ", 0) == 0);
  });
}

TEST_CASE("ore GL replay reports a failed link no pass binds", "[ore][gl]")
{
  runScenario([](const Frame& f,
                 ore::cmd::DeferredOreContext& dctx,
                 TextureView*) {
    ore::cmd::OreResident table;
    std::vector<rcp<ShaderModule>> modules;
    auto pipeline = makeUnlinkablePipeline(dctx, modules);
    REQUIRE(pipeline);
    const auto id = dctx.handleFor(pipeline.get());
    drawFrame(f, dctx, table);
    CHECK(table.getAs(id, ore::cmd::OreKind::pipeline) != nullptr);
    CHECK(table.failureNote(id) == nullptr);
    for (int i = 0; i < 200 && table.failureNote(id) == nullptr; ++i)
      drawFrame(f, dctx, table);
    CHECK(table.getAs(id, ore::cmd::OreKind::pipeline) == nullptr);
    const char* note = table.failureNote(id);
    REQUIRE(note != nullptr);
    CHECK(std::string(note).rfind("pipeline 'unlinkable' failed: ", 0) == 0);
  });
}

TEST_CASE("ore GL replay forgets links of destroyed pipelines", "[ore][gl]")
{
  runScenario([](const Frame& f,
                 ore::cmd::DeferredOreContext& dctx,
                 TextureView* target) {
    ore::cmd::OreResident table;
    WitnessScene scene(dctx);
    for (int frame = 0; frame < 4; ++frame)
    {
      // More than one poll can settle, each made and destroyed unbound.
      for (int i = 0; i < 12; ++i)
        scene.makePipeline(dctx);
      dctx.drainPendingDestroys();
      recordPass(dctx, target, scene.pipeline.get(), scene.group.get());
      drawFrame(f, dctx, table);
      CHECK(table.pendingPipelines.size() == 0);
    }
  });
}

TEST_CASE("ore GL immediate makePipeline still fails at once", "[ore][gl]")
{
  runScenario([](const Frame& f, ore::cmd::DeferredOreContext&, TextureView*) {
    Context& real = *f.renderContext->getOreContext();
    std::vector<rcp<ShaderModule>> modules;
    real.clearLastError();
    CHECK(makeUnlinkablePipeline(real, modules) == nullptr);
    CHECK(real.lastError().rfind("Ore GL program link error: ", 0) == 0);
  });
}

TEST_CASE("ore GL pass drops draws after a failed deferred link", "[ore][gl]")
{
  runScenario([](const Frame& f, ore::cmd::DeferredOreContext&, TextureView*) {
    Context& real = *f.renderContext->getOreContext();
    std::vector<rcp<ShaderModule>> modules;
    auto failing = makeUnlinkablePipeline(real, modules, true);
    REQUIRE(failing);
    WitnessScene scene(real);
    const uint32_t kIndices[] = {0, 1, 2};
    BufferDesc bd{};
    bd.usage = BufferUsage::index;
    bd.size = sizeof(kIndices);
    bd.data = kIndices;
    auto indices = real.makeBuffer(bd);
    REQUIRE(indices);
    auto target = real.wrapCanvasTexture(f.canvas);
    REQUIRE(target);
    // Inline replay has no resident table, so RenderPassGL settles the link.
    auto drawPass = [&](Pipeline* next) {
      return canvasCenter(f, [&] {
        RenderPassDesc rp{};
        rp.colorAttachments[0].view = target.get();
        rp.colorAttachments[0].loadOp = LoadOp::clear;
        rp.colorAttachments[0].clearColor = {0, 0, 1, 1};
        rp.colorCount = 1;
        auto pass = ore::cmd::beginRecordedRenderPass(real, rp);
        pass->setPipeline(failing.get());
        pass->setIndexBuffer(indices.get(), IndexFormat::uint32);
        pass->draw(3);
        pass->drawIndexed(3);
        if (next != nullptr)
        {
          pass->setPipeline(next);
          pass->setBindGroup(0, scene.group.get());
          pass->draw(3);
        }
        pass->finish();
      });
    };
    real.clearLastError();
    CHECK(pixelNear(drawPass(nullptr), 0, 0, 255));
    CHECK(real.lastError().rfind("Ore GL program link error: ", 0) == 0);
    CHECK(pixelNear(drawPass(scene.pipeline.get()), 77, 153, 0));
  });
}
#endif
