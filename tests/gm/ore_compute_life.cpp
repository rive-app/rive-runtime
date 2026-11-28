/*
 * Copyright 2026 Rive
 *
 * Eight generations of Life computed on the GPU and drawn (128x128). One
 * compute pass per generation steps a 32 by 32 grid between two storage
 * buffers, a glider is written into the grid half way, and a draw paints the
 * last buffer. The rule each step reads is a uniform set right only after its
 * group is bound, since a dispatch reads uniforms as they are when it runs.
 *
 * Every step is integer math, so a backend that runs compute draws the same
 * image as any other. One that does not leaves the clear color, and one that
 * has compute and fails draws red.
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "ore_gm_helper.hpp"
#if defined(ORE_BACKEND_METAL) || defined(ORE_BACKEND_GL) ||                   \
    defined(ORE_BACKEND_VK) || defined(ORE_BACKEND_WGPU) ||                    \
    defined(ORE_BACKEND_D3D11) || defined(ORE_BACKEND_D3D12) ||                \
    defined(ORE_BACKEND_RHI)
#define ORE_COMPUTE_LIFE_ACTIVE
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_bind_group_layout.hpp"
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_compute_pass.hpp"
#include "rive/renderer/ore/ore_compute_pipeline.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/renderer/ore/ore_render_pass.hpp"
#include "rive/renderer/ore/ore_shader_asset_modules.hpp"
#include "rive/renderer/ore/ore_shader_module.hpp"
#include "../unit_tests/renderer/ore_compute_shaders.rstb.hpp"
#include <cstring>
#include <string>
#endif

using namespace rivegm;
using namespace rive;
using namespace rive::gpu;

class OreComputeLifeGM : public GM
{
public:
  OreComputeLifeGM() : GM(kSize, kSize) {}

  ColorInt clearColor() const override { return 0xff203040; }

  void onDraw(rive::Renderer* originalRenderer) override
  {
    auto renderContext = TestingWindow::Get()->renderContext();
    if (!renderContext || !m_ore.ensureContext(renderContext))
      return;

#ifdef ORE_COMPUTE_LIFE_ACTIVE
    auto& ctx = *renderContext->getOreContext();
    if (!ctx.features().computeShaders || !ctx.features().storageBuffers)
      return;

    ctx.clearLastError();
    rcp<RenderCanvas> canvas = computeLife(ctx, renderContext);
    ore_gm::invalidateGLStateAfterOre(renderContext);
    if (canvas == nullptr || !ctx.lastError().empty())
    {
      // Red, so a backend that has compute and fails cannot pass for one
      // without it.
      fprintf(
          stderr,
          "[ore_compute_life] failed: %s\n",
          ctx.lastError().c_str());
      Paint red(0xffff0000);
      draw_rect(originalRenderer, {0.f, 0.f, float(kSize), float(kSize)}, red);
      return;
    }
    originalRenderer->save();
    originalRenderer->drawImage(
        canvas->renderImage(),
        {.filter = ImageFilter::nearest},
        BlendMode::srcOver,
        1);
    originalRenderer->restore();
#endif // ORE_COMPUTE_LIFE_ACTIVE
  }

private:
  static constexpr uint32_t kGrid = 32;
  // The fragment shader paints 4 pixels to a cell.
  static constexpr uint32_t kSize = kGrid * 4;

#ifdef ORE_COMPUTE_LIFE_ACTIVE
  // The canvas the last generation was drawn into, or null on a failure.
  rcp<RenderCanvas> computeLife(
      rive::ore::Context& ctx,
      RenderContext* renderContext)
  {
    using namespace rive::ore;
    constexpr uint32_t kCells = kGrid * kGrid;
    constexpr uint32_t kGenerations = 8;
    constexpr uint32_t kLife = 3;
    if (kLife >= ore_compute_shaders::kShaderCount ||
        strcmp(ore_compute_shaders::kShaderNames[kLife], "life") != 0)
    {
      return nullptr;
    }

    ShaderAsset asset;
    const uint32_t* offsets = ore_compute_shaders::kShaderOffsets;
    if (!decodeBareRstb(
            asset,
            ore_compute_shaders::kShaderData + offsets[kLife],
            offsets[kLife + 1] - offsets[kLife]))
    {
      return nullptr;
    }
    // By stage. One module holds all three entry points, or on Direct3D each
    // has its own.
    rcp<ShaderModule> modules[3];
    std::string names[3];
    visitShaderAssetModules(
        ctx.shaderTarget(),
        asset,
        [&](const ShaderModuleDesc& desc,
            uint32_t first,
            uint32_t count,
            const std::vector<RstbEntryView>& entries) {
          rcp<ShaderModule> module = ctx.makeShaderModule(desc);
          for (uint32_t i = first; i < first + count; ++i)
          {
            const uint8_t stage = entries[i].stage;
            if (stage < 3)
            {
              modules[stage] = module;
              names[stage] = entries[i].physical;
            }
          }
          return true;
        });
    ShaderModule* vertexModule = modules[0].get();
    ShaderModule* fragmentModule = modules[1].get();
    ShaderModule* stepModule = modules[2].get();
    if (vertexModule == nullptr || fragmentModule == nullptr ||
        stepModule == nullptr)
    {
      return nullptr;
    }
    const char* vertex = names[0].c_str();
    const char* fragment = names[1].c_str();
    const char* step = names[2].c_str();

    rcp<BindGroupLayout> stepLayout = makeBindGroupLayoutFromBindingMap(
        ctx,
        bindingMapForComputeEntry(stepModule, step),
        0);
    rcp<BindGroupLayout> showLayout = makeBindGroupLayoutFromBindingMap(
        ctx,
        bindingMapForEntries(vertexModule, vertex, fragmentModule, fragment),
        0);
    if (stepLayout == nullptr || showLayout == nullptr)
      return nullptr;

    BindGroupLayout* stepLayouts[] = {stepLayout.get()};
    ComputePipelineDesc stepDesc;
    stepDesc.module = stepModule;
    stepDesc.entryPoint = step;
    stepDesc.bindGroupLayouts = stepLayouts;
    stepDesc.bindGroupLayoutCount = 1;
    rcp<ComputePipeline> stepPipeline = ctx.makeComputePipeline(stepDesc);
    if (stepPipeline == nullptr)
      return nullptr;

    BindGroupLayout* showLayouts[] = {showLayout.get()};
    PipelineDesc showDesc{};
    showDesc.vertexModule = vertexModule;
    showDesc.fragmentModule = fragmentModule;
    showDesc.vertexEntryPoint = vertex;
    showDesc.fragmentEntryPoint = fragment;
    showDesc.topology = PrimitiveTopology::triangleList;
    showDesc.colorTargets[0].format = TextureFormat::rgba8unorm;
    showDesc.colorCount = 1;
    showDesc.bindGroupLayouts = showLayouts;
    showDesc.bindGroupLayoutCount = 1;
    rcp<Pipeline> showPipeline = ctx.makePipeline(showDesc);
    if (showPipeline == nullptr)
      return nullptr;

    uint32_t soup[kCells];
    uint32_t seed = 12345;
    for (uint32_t& cell : soup)
    {
      seed = seed * 1664525u + 1013904223u;
      cell = (seed >> 24) < 90 ? 1 : 0;
    }
    BufferDesc bufferDesc{};
    bufferDesc.usage = BufferUsage::storage;
    bufferDesc.size = sizeof(soup);
    bufferDesc.data = soup;
    rcp<Buffer> grids[2];
    grids[0] = ctx.makeBuffer(bufferDesc);
    bufferDesc.data = nullptr;
    grids[1] = ctx.makeBuffer(bufferDesc);
    BufferDesc ruleDesc{};
    ruleDesc.usage = BufferUsage::uniform;
    ruleDesc.size = 16;
    rcp<Buffer> rule = ctx.makeBuffer(ruleDesc);
    if (grids[0] == nullptr || grids[1] == nullptr || rule == nullptr)
      return nullptr;

    // stepGroups[i] reads grids[i] and writes the other, showGroups[i] paints
    // grids[i].
    rcp<BindGroup> stepGroups[2], showGroups[2];
    for (uint32_t i = 0; i < 2; ++i)
    {
      const BindGroupDesc::StorageEntry stepStorage[] = {
          {0, grids[i].get(), 0, 0},
          {1, grids[1 - i].get(), 0, 0}};
      const BindGroupDesc::UBOEntry stepRule = {2, rule.get(), 0, 0};
      BindGroupDesc stepGroupDesc;
      stepGroupDesc.layout = stepLayout.get();
      stepGroupDesc.storageBuffers = stepStorage;
      stepGroupDesc.storageBufferCount = 2;
      stepGroupDesc.ubos = &stepRule;
      stepGroupDesc.uboCount = 1;
      stepGroups[i] = ctx.makeBindGroup(stepGroupDesc);

      const BindGroupDesc::StorageEntry showStorage[] = {
          {0, grids[i].get(), 0, 0}};
      BindGroupDesc showGroupDesc;
      showGroupDesc.layout = showLayout.get();
      showGroupDesc.storageBuffers = showStorage;
      showGroupDesc.storageBufferCount = 1;
      showGroups[i] = ctx.makeBindGroup(showGroupDesc);
      if (stepGroups[i] == nullptr || showGroups[i] == nullptr)
        return nullptr;
    }

    auto canvas = renderContext->makeRenderCanvas(kSize, kSize);
    if (!canvas)
      return nullptr;
    auto canvasTarget = ctx.wrapCanvasTexture(canvas.get());
    if (!canvasTarget)
      return nullptr;

    m_ore.beginFrame(renderContext);

    // Three neighbours birth a cell. The uniform as bound says none do.
    static const uint32_t kNoBirth[4] = {};
    static const uint32_t kBirth[4] = {3};
    for (uint32_t generation = 0; generation < kGenerations; ++generation)
    {
      rule->update(kNoBirth, sizeof(kNoBirth), 0);
      auto compute = ctx.beginComputePass({});
      if (compute == nullptr)
        break;
      compute->setPipeline(stepPipeline.get());
      compute->setBindGroup(0, stepGroups[generation & 1].get());
      rule->update(kBirth, sizeof(kBirth), 0);
      compute->dispatchWorkgroups(kGrid / 8, kGrid / 8);
      compute->finish();

      // Between passes, so it lands after this step and before the next.
      if (generation == kGenerations / 2 - 1)
      {
        static const uint32_t kGlider[3][3] = {{0, 1, 0}, {0, 0, 1}, {1, 1, 1}};
        Buffer* written = grids[1 - (generation & 1)].get();
        for (uint32_t row = 0; row < 3; ++row)
        {
          written->update(
              kGlider[row],
              sizeof(kGlider[row]),
              ((4 + row) * kGrid + 4) * sizeof(uint32_t));
        }
      }
    }

    ColorAttachment ca{};
    ca.view = canvasTarget.get();
    ca.loadOp = LoadOp::clear;
    ca.storeOp = StoreOp::store;
    ca.clearColor = {0, 0, 0, 1};
    RenderPassDesc rpDesc{};
    rpDesc.colorAttachments[0] = ca;
    rpDesc.colorCount = 1;
    rpDesc.label = "ore_compute_life_show";
    if (auto pass = ctx.beginRenderPass(rpDesc))
    {
      pass->setPipeline(showPipeline.get());
      pass->setBindGroup(0, showGroups[kGenerations & 1].get());
      pass->setViewport(0, 0, kSize, kSize);
      pass->draw(3);
      pass->finish();
    }
    else
    {
      canvas = nullptr;
    }

    m_ore.endFrame(renderContext);
    return canvas;
  }
#endif // ORE_COMPUTE_LIFE_ACTIVE

  ore_gm::OreGMContext m_ore;
};

GMREGISTER(ore_compute_life, return new OreComputeLifeGM)
