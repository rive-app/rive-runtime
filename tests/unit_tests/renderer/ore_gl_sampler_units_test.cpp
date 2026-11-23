/*
 * Copyright 2026 Rive
 */

// WebGL rejects a fetch whose unit still holds the last draw's comparison
// sampler, so our GL backend clears it, and GL fails a draw whose unit two
// sampler types read, so a depth texture read both ways gets a second unit.

#include "ore_test_backends.hpp"

#if defined(ORE_BACKEND_GL)
#include "rive/renderer/gl/gles3.hpp"
#include "rive/renderer/render_context.hpp"
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/renderer/ore/ore_render_pass.hpp"
#include "rive/renderer/ore/ore_sampler.hpp"
#include "rive/renderer/ore/ore_texture.hpp"
#include <cstdlib>
#include <iterator>
#include <set>
#include <vector>

using namespace rive;
using namespace rive::ore;

namespace
{
struct SamplerUnit
{
  GLint unit;
  GLint sampler;
  GLint texture;
  bool shadow;
};

// The sampler bound on each unit the current program reads a texture from.
std::vector<SamplerUnit> programSamplerUnits()
{
  GLint program = 0;
  glGetIntegerv(GL_CURRENT_PROGRAM, &program);
  GLint active = 0;
  glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
  GLint count = 0;
  glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &count);
  std::vector<SamplerUnit> units;
  for (GLint i = 0; i < count; ++i)
  {
    char name[128] = {};
    GLint size = 0;
    GLenum type = 0;
    glGetActiveUniform(program, i, sizeof(name), nullptr, &size, &type, name);
    if (type != GL_SAMPLER_2D && type != GL_SAMPLER_2D_SHADOW)
      continue;
    SamplerUnit unit{0, -1, 0, type == GL_SAMPLER_2D_SHADOW};
    glGetUniformiv(program, glGetUniformLocation(program, name), &unit.unit);
    glActiveTexture(GL_TEXTURE0 + unit.unit);
    glGetIntegerv(GL_SAMPLER_BINDING, &unit.sampler);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &unit.texture);
    units.push_back(unit);
  }
  glActiveTexture(active);
  return units;
}

rcp<Pipeline> makeFullscreenPipeline(
    Context& ctx,
    const ore_gm::OreGMShaderResult& shader,
    BindGroupLayout* layout,
    bool deferBuildCheck = false)
{
  PipelineDesc desc{};
  desc.deferBuildCheck = deferBuildCheck;
  desc.vertexModule = shader.vsModule.get();
  desc.fragmentModule = shader.psModule.get();
  desc.vertexEntryPoint = shader.vsEntryPoint;
  desc.fragmentEntryPoint = shader.fsEntryPoint;
  desc.topology = PrimitiveTopology::triangleList;
  desc.colorTargets[0].format = TextureFormat::rgba8unorm;
  desc.colorCount = 1;
  desc.bindGroupLayouts = &layout;
  desc.bindGroupLayoutCount = 1;
  std::string error;
  auto pipeline = ctx.makePipeline(desc, &error);
  INFO("makePipeline: " << error);
  REQUIRE(pipeline != nullptr);
  return pipeline;
}

struct DepthScene
{
  rcp<Texture> texture;
  rcp<TextureView> view;
  rcp<Sampler> compare;
};

DepthScene makeDepthScene(Context& ctx)
{
  DepthScene scene;
  TextureDesc depthDesc{};
  depthDesc.width = 1;
  depthDesc.height = 1;
  depthDesc.format = TextureFormat::depth32float;
  depthDesc.renderTarget = true;
  depthDesc.numMipmaps = 1;
  depthDesc.label = "ore_gl_sampler_units_depth";
  scene.texture = ctx.makeTexture(depthDesc);
  REQUIRE(scene.texture != nullptr);
  TextureViewDesc viewDesc{};
  viewDesc.texture = scene.texture.get();
  viewDesc.mipCount = 1;
  viewDesc.layerCount = 1;
  viewDesc.aspect = TextureAspect::depthOnly;
  scene.view = ctx.makeTextureView(viewDesc);
  REQUIRE(scene.view != nullptr);
  SamplerDesc compareDesc{};
  compareDesc.compare = CompareFunction::less;
  scene.compare = ctx.makeSampler(compareDesc);
  REQUIRE(scene.compare != nullptr);
  return scene;
}

void runFetchAfterCompareScenario(TestingWindow* window)
{
  auto* renderContext = window->renderContext();
  ore_gm::OreGMContext oreGM;
  REQUIRE(oreGM.ensureContext(renderContext));
  auto& ctx = *renderContext->getOreContext();

  auto compareShader = ore_gm::loadShader(ctx, ore_gm::kUnitCompare);
  auto fetchShader = ore_gm::loadShader(ctx, ore_gm::kUnitFetch);
  REQUIRE(compareShader.vsModule);
  REQUIRE(fetchShader.vsModule);
  auto compareLayout = ore_gm::makeLayoutFromShader(
      ctx,
      compareShader.vsModule.get(),
      0);
  auto fetchLayout = ore_gm::makeLayoutFromShader(
      ctx,
      fetchShader.vsModule.get(),
      0);
  auto comparePipeline = makeFullscreenPipeline(
      ctx,
      compareShader,
      compareLayout.get());
  auto fetchPipeline = makeFullscreenPipeline(
      ctx,
      fetchShader,
      fetchLayout.get());
  // Its group then carries the comparison sampler it never samples through.
  auto sharedFetchPipeline = makeFullscreenPipeline(
      ctx,
      fetchShader,
      compareLayout.get());

  auto scene = makeDepthScene(ctx);
  BindGroupDesc::TexEntry texture{};
  texture.slot = 0;
  texture.view = scene.view.get();
  BindGroupDesc::SampEntry sampler{};
  sampler.slot = 1;
  sampler.sampler = scene.compare.get();
  BindGroupDesc compareGroupDesc{};
  compareGroupDesc.layout = compareLayout.get();
  compareGroupDesc.textures = &texture;
  compareGroupDesc.textureCount = 1;
  compareGroupDesc.samplers = &sampler;
  compareGroupDesc.samplerCount = 1;
  auto compareGroup = ctx.makeBindGroup(compareGroupDesc);
  REQUIRE(compareGroup != nullptr);
  BindGroupDesc fetchGroupDesc{};
  fetchGroupDesc.layout = fetchLayout.get();
  fetchGroupDesc.textures = &texture;
  fetchGroupDesc.textureCount = 1;
  auto fetchGroup = ctx.makeBindGroup(fetchGroupDesc);
  REQUIRE(fetchGroup != nullptr);

  auto canvas = renderContext->makeRenderCanvas(4, 4);
  REQUIRE(canvas != nullptr);
  auto target = ctx.wrapCanvasTexture(canvas.get());
  REQUIRE(target != nullptr);

  auto renderer = window->beginFrame(
      {.name = "ore_gl_sampler_units", .doClear = true});
  oreGM.beginFrame(renderContext);
  RenderPassDesc passDesc{};
  passDesc.colorAttachments[0].view = target.get();
  passDesc.colorAttachments[0].loadOp = LoadOp::clear;
  passDesc.colorAttachments[0].storeOp = StoreOp::store;
  passDesc.colorCount = 1;
  auto pass = ctx.beginRenderPass(passDesc);
  REQUIRE(pass != nullptr);

  struct Draw
  {
    Pipeline* pipeline;
    BindGroup* group;
    bool compares;
  };
  // The second compare needs back the sampler the fetch before it cleared.
  const Draw draws[] = {
      {comparePipeline.get(), compareGroup.get(), true},
      {fetchPipeline.get(), fetchGroup.get(), false},
      {comparePipeline.get(), compareGroup.get(), true},
      {sharedFetchPipeline.get(), compareGroup.get(), false},
  };
  GLint unit = -1;
  for (size_t i = 0; i < std::size(draws); ++i)
  {
    INFO("draw " << i);
    pass->setPipeline(draws[i].pipeline);
    // The last draw keeps its group, so only the pipeline change rebinds.
    if (i == 0 || draws[i].group != draws[i - 1].group)
      pass->setBindGroup(0, draws[i].group);
    pass->draw(3);
    const auto units = programSamplerUnits();
    REQUIRE(units.size() == 1);
    if (unit < 0)
      unit = units[0].unit;
    CHECK(units[0].unit == unit);
    CHECK(units[0].shadow == draws[i].compares);
    CHECK((units[0].sampler != 0) == draws[i].compares);
  }

  pass->finish();
  oreGM.endFrame(renderContext);
  ore_gm::invalidateGLStateAfterOre(renderContext);
  window->endFrame();
}

bool compares(GLint sampler)
{
  GLint mode = GL_NONE;
  if (sampler != 0)
    glGetSamplerParameteriv(sampler, GL_TEXTURE_COMPARE_MODE, &mode);
  return mode == GL_COMPARE_REF_TO_TEXTURE;
}

struct TwinDraw
{
  uint32_t shader;
  // Keeps the last draw's group, so only the twin tells the pass to bind the
  // texture again.
  bool keepsGroup;
  // Binds the plain sampler and a second texture as well.
  bool fourBindings;
  size_t uniforms;
  // Plain reads sharing a shadow read's texture, which the twin unit holds.
  int twins;
  int plainSamplers;
};

// A draw that only compares; one entry fetching what it compares, keeping that
// group; a vertex stage fetching what its fragment stage compares; two
// textures read both ways, one through a plain sampler; and two whose
// comparison or plain sample goes unused, leaving GL one uniform of the two.
template <bool deferBuildCheck>
void runCompareAndFetchScenario(TestingWindow* window)
{
  auto* renderContext = window->renderContext();
  ore_gm::OreGMContext oreGM;
  REQUIRE(oreGM.ensureContext(renderContext));
  auto& ctx = *renderContext->getOreContext();
  if (deferBuildCheck && !ctx.defersBuildChecks())
  {
    WARN("no KHR_parallel_shader_compile; links stay synchronous");
    return;
  }
  auto scene = makeDepthScene(ctx);
  auto second = makeDepthScene(ctx);
  auto plainSampler = ctx.makeSampler(SamplerDesc{});
  REQUIRE(plainSampler != nullptr);
  auto canvas = renderContext->makeRenderCanvas(4, 4);
  REQUIRE(canvas != nullptr);
  auto target = ctx.wrapCanvasTexture(canvas.get());
  REQUIRE(target != nullptr);

  auto renderer = window->beginFrame(
      {.name = "ore_gl_sampler_units_split", .doClear = true});
  oreGM.beginFrame(renderContext);
  RenderPassDesc passDesc{};
  passDesc.colorAttachments[0].view = target.get();
  passDesc.colorAttachments[0].loadOp = LoadOp::clear;
  passDesc.colorAttachments[0].storeOp = StoreOp::store;
  passDesc.colorCount = 1;
  auto pass = ctx.beginRenderPass(passDesc);
  REQUIRE(pass != nullptr);

  const TwinDraw draws[] = {
      {ore_gm::kUnitCompare, false, false, 1, 0, 0},
      {ore_gm::kUnitCompareFetch, true, false, 2, 1, 0},
      {ore_gm::kUnitStageSplit, false, false, 2, 1, 0},
      {ore_gm::kUnitTwinPair, false, true, 4, 2, 1},
      {ore_gm::kUnitDeadCompare, false, false, 1, 0, 0},
      {ore_gm::kUnitDeadPlain, false, true, 2, 0, 0},
  };
  rcp<BindGroupLayout> layout;
  rcp<BindGroup> group;
  std::vector<rcp<Pipeline>> pipelines;
  for (const TwinDraw& draw : draws)
  {
    INFO("shader " << draw.shader);
    auto shader = ore_gm::loadShader(ctx, draw.shader);
    REQUIRE(shader.vsModule);
    if (!draw.keepsGroup)
    {
      layout = ore_gm::makeLayoutFromShader(ctx, shader.vsModule.get(), 0);
      BindGroupDesc::TexEntry textures[2] = {};
      textures[0].slot = 0;
      textures[0].view = scene.view.get();
      textures[1].slot = 3;
      textures[1].view = second.view.get();
      BindGroupDesc::SampEntry samplers[2] = {};
      samplers[0].slot = 1;
      samplers[0].sampler = scene.compare.get();
      samplers[1].slot = 2;
      samplers[1].sampler = plainSampler.get();
      BindGroupDesc groupDesc{};
      groupDesc.layout = layout.get();
      groupDesc.textures = textures;
      groupDesc.textureCount = draw.fourBindings ? 2 : 1;
      groupDesc.samplers = samplers;
      groupDesc.samplerCount = draw.fourBindings ? 2 : 1;
      group = ctx.makeBindGroup(groupDesc);
      REQUIRE(group != nullptr);
    }
    pipelines.push_back(
        makeFullscreenPipeline(ctx, shader, layout.get(), deferBuildCheck));

    while (glGetError() != GL_NO_ERROR)
    {
    }
    pass->setPipeline(pipelines.back().get());
    if (!draw.keepsGroup)
      pass->setBindGroup(0, group.get());
    pass->draw(3);
    CHECK(glGetError() == GL_NO_ERROR);
    const auto units = programSamplerUnits();
    REQUIRE(units.size() == draw.uniforms);
    std::set<GLint> distinct;
    int twins = 0;
    int plainSamplers = 0;
    for (const SamplerUnit& unit : units)
    {
      distinct.insert(unit.unit);
      CHECK(compares(unit.sampler) == unit.shadow);
      if (unit.shadow)
        continue;
      plainSamplers += unit.sampler != 0;
      for (const SamplerUnit& shadow : units)
        twins += shadow.shadow && shadow.texture == unit.texture;
    }
    CHECK(distinct.size() == units.size());
    CHECK(twins == draw.twins);
    CHECK(plainSamplers == draw.plainSamplers);
  }

  pass->finish();
  oreGM.endFrame(renderContext);
  ore_gm::invalidateGLStateAfterOre(renderContext);
  window->endFrame();
}
struct Multisampled
{
  rcp<Texture> texture;
  rcp<TextureView> view;
};

Multisampled makeMultisampled(Context& ctx, TextureFormat format)
{
  Multisampled made;
  TextureDesc desc{};
  desc.width = 4;
  desc.height = 4;
  desc.format = format;
  desc.renderTarget = true;
  desc.sampleCount = 4;
  made.texture = ctx.makeTexture(desc);
  REQUIRE(made.texture != nullptr);
  TextureViewDesc viewDesc{};
  viewDesc.texture = made.texture.get();
  made.view = ctx.makeTextureView(viewDesc);
  REQUIRE(made.view != nullptr);
  return made;
}

// A multisampled colour and depth target, and the group a shader reads them
// through, when it has one yet.
struct MultisampledPair
{
  Multisampled colour;
  Multisampled depth;
  rcp<BindGroup> group;

  void bind(Context& ctx, BindGroupLayout* layout)
  {
    BindGroupDesc::TexEntry textures[2] = {};
    textures[0].slot = 0;
    textures[0].view = colour.view.get();
    textures[1].slot = 1;
    textures[1].view = depth.view.get();
    BindGroupDesc desc{};
    desc.layout = layout;
    desc.textures = textures;
    desc.textureCount = 2;
    group = ctx.makeBindGroup(desc);
    REQUIRE(group != nullptr);
  }

  void clear(Context& ctx, float red, float green, float depthValue)
  {
    RenderPassDesc desc{};
    desc.colorAttachments[0].view = colour.view.get();
    desc.colorAttachments[0].clearColor = {red, green, 0.0f, 1.0f};
    desc.colorCount = 1;
    desc.depthStencil.view = depth.view.get();
    desc.depthStencil.depthClearValue = depthValue;
    auto pass = ctx.beginRenderPass(desc);
    REQUIRE(pass != nullptr);
    pass->finish();
  }
};

// What the shader reads from `pair`: its red, its green and its depth.
void checkReads(
    Context& ctx,
    Pipeline* pipeline,
    TextureView* target,
    const MultisampledPair& pair,
    int red,
    int green,
    int depth)
{
  RenderPassDesc desc{};
  desc.colorAttachments[0].view = target;
  desc.colorCount = 1;
  auto pass = ctx.beginRenderPass(desc);
  REQUIRE(pass != nullptr);
  pass->setPipeline(pipeline);
  pass->setBindGroup(0, pair.group.get());
  pass->draw(3);
  uint8_t pixel[4] = {};
  glReadPixels(1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
  pass->finish();
  CHECK(std::abs(pixel[0] - red) <= 1);
  CHECK(std::abs(pixel[1] - green) <= 1);
  CHECK(std::abs(pixel[2] - depth) <= 1);
}

void runMultisampledReadScenario(TestingWindow* window)
{
  auto* renderContext = window->renderContext();
  ore_gm::OreGMContext oreGM;
  REQUIRE(oreGM.ensureContext(renderContext));
  auto& ctx = *renderContext->getOreContext();

  auto shader = ore_gm::loadShader(ctx, ore_gm::kMultisampledRead);
  REQUIRE(shader.vsModule);
  auto layout = ore_gm::makeLayoutFromShader(ctx, shader.vsModule.get(), 0);
  auto pipeline = makeFullscreenPipeline(ctx, shader, layout.get());
  auto canvas = renderContext->makeRenderCanvas(4, 4);
  REQUIRE(canvas != nullptr);
  auto target = ctx.wrapCanvasTexture(canvas.get());
  REQUIRE(target != nullptr);

  auto renderer = window->beginFrame(
      {.name = "ore_gl_multisampled_read", .doClear = true});
  oreGM.beginFrame(renderContext);

  // Bound before anything is drawn: each pass that draws resolves again.
  MultisampledPair early{
      makeMultisampled(ctx, TextureFormat::rgba8unorm),
      makeMultisampled(ctx, TextureFormat::depth32float)};
  early.bind(ctx, layout.get());
  early.clear(ctx, 0.25f, 0.5f, 0.75f);
  checkReads(ctx, pipeline.get(), target.get(), early, 64, 128, 191);
  early.clear(ctx, 0.5f, 0.25f, 0.25f);
  checkReads(ctx, pipeline.get(), target.get(), early, 128, 64, 64);

  // Bound only after it was drawn to, so its copy starts from what is there.
  MultisampledPair late{
      makeMultisampled(ctx, TextureFormat::rgba8unorm),
      makeMultisampled(ctx, TextureFormat::depth32float)};
  late.clear(ctx, 1.0f, 0.0f, 0.5f);
  late.bind(ctx, layout.get());
  checkReads(ctx, pipeline.get(), target.get(), late, 255, 0, 128);

  oreGM.endFrame(renderContext);
  ore_gm::invalidateGLStateAfterOre(renderContext);
  window->endFrame();
}
} // namespace

TEST_CASE(
    "ore GL gives a depth texture read with and without a comparison two units",
    "[ore][gl]")
{
  if (ore_test::runOnBackends(
          ore_test::kGLBackends,
          &runCompareAndFetchScenario<false>) == 0)
    WARN("no Ore GL backend available headless; skipping");
  // A link left to settle at setPipeline has to split its units there.
  ore_test::runOnBackends(
      ore_test::kGLBackends,
      &runCompareAndFetchScenario<true>);
}

TEST_CASE(
    "ore GL clears a comparison sampler off a unit the next draw only fetches",
    "[ore][gl]")
{
  if (ore_test::runOnBackends(
          ore_test::kGLBackends,
          &runFetchAfterCompareScenario) == 0)
    WARN("no Ore GL backend available headless; skipping");
}

// GL ES 3.0 cannot bind a multisampled texture, so the shader reads a copy
// resolved from it.
TEST_CASE(
    "ore GL reads a multisampled texture through its resolved copy",
    "[ore][gl]")
{
  if (ore_test::runOnBackends(
          ore_test::kGLBackends,
          &runMultisampledReadScenario) == 0)
    WARN("no Ore GL backend available headless; skipping");
}
#endif // ORE_BACKEND_GL
