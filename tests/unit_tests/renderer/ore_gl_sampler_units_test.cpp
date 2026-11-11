/*
 * Copyright 2026 Rive
 */

// WebGL rejects a fetch whose unit still holds the last draw's comparison
// sampler, so our GL backend clears it.

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
#include <iterator>
#include <vector>

using namespace rive;
using namespace rive::ore;

namespace
{
struct SamplerUnit
{
  GLint unit;
  GLint sampler;
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
    SamplerUnit unit{0, -1, type == GL_SAMPLER_2D_SHADOW};
    glGetUniformiv(program, glGetUniformLocation(program, name), &unit.unit);
    glActiveTexture(GL_TEXTURE0 + unit.unit);
    glGetIntegerv(GL_SAMPLER_BINDING, &unit.sampler);
    units.push_back(unit);
  }
  glActiveTexture(active);
  return units;
}

rcp<Pipeline> makeFullscreenPipeline(
    Context& ctx,
    const ore_gm::OreGMShaderResult& shader,
    BindGroupLayout* layout)
{
  PipelineDesc desc{};
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

  TextureDesc depthDesc{};
  depthDesc.width = 1;
  depthDesc.height = 1;
  depthDesc.format = TextureFormat::depth32float;
  depthDesc.renderTarget = true;
  depthDesc.numMipmaps = 1;
  depthDesc.label = "ore_gl_sampler_units_depth";
  auto depthTex = ctx.makeTexture(depthDesc);
  REQUIRE(depthTex != nullptr);
  TextureViewDesc viewDesc{};
  viewDesc.texture = depthTex.get();
  viewDesc.mipCount = 1;
  viewDesc.layerCount = 1;
  viewDesc.aspect = TextureAspect::depthOnly;
  auto depthView = ctx.makeTextureView(viewDesc);
  REQUIRE(depthView != nullptr);

  SamplerDesc compareDesc{};
  compareDesc.compare = CompareFunction::less;
  auto compareSampler = ctx.makeSampler(compareDesc);
  REQUIRE(compareSampler != nullptr);

  BindGroupDesc::TexEntry texture{};
  texture.slot = 0;
  texture.view = depthView.get();
  BindGroupDesc::SampEntry sampler{};
  sampler.slot = 1;
  sampler.sampler = compareSampler.get();
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
} // namespace

TEST_CASE(
    "ore GL clears a comparison sampler off a unit the next draw only fetches",
    "[ore][gl]")
{
  if (ore_test::runOnBackends(
          ore_test::kGLBackends,
          &runFetchAfterCompareScenario) == 0)
    WARN("no Ore GL backend available headless; skipping");
}
#endif // ORE_BACKEND_GL
