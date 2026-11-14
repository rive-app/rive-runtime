/*
 * Copyright 2026 Rive
 */

#include "rive/renderer/ore/layer_mask_compositor.hpp"

#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_bind_group_layout.hpp"
#include "rive/renderer/ore/ore_context.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/renderer/ore/ore_render_pass.hpp"
#include "rive/renderer/ore/ore_shader_asset_modules.hpp"
#include "rive/renderer/ore/ore_shader_module.hpp"
#include "rive/renderer/ore/ore_texture.hpp"
#include "rive/renderer/render_canvas.hpp"
#include "layer_mask_shaders.rstb.hpp"

#include <string>

namespace rive::ore
{
LayerMaskCompositor::LayerMaskCompositor() = default;
LayerMaskCompositor::~LayerMaskCompositor() = default;

static_assert(
    layer_mask_shaders::kShaderCount == 4,
    "one baked shader per LayerMaskMode");

Pipeline* LayerMaskCompositor::pipelineFor(Context& ore, LayerMaskMode mode)
{
  // An unknown mode masks by alpha, as LayerMask::maskMode does.
  const size_t i = static_cast<size_t>(mode) < kModeCount
                       ? static_cast<size_t>(mode)
                       : static_cast<size_t>(LayerMaskMode::alpha);
  if (m_pipelines[i] != nullptr)
  {
    return m_pipelines[i].get();
  }

  ShaderAsset asset;
  const uint32_t begin = layer_mask_shaders::kShaderOffsets[i];
  if (!decodeBareRstb(
          asset,
          layer_mask_shaders::kShaderData + begin,
          layer_mask_shaders::kShaderOffsets[i + 1] - begin))
  {
    return nullptr;
  }
  rcp<ShaderModule> vertex;
  rcp<ShaderModule> fragment;
  std::string vertexEntry;
  std::string fragmentEntry;
  bool built = visitShaderAssetModules(
      ore.shaderTarget(),
      asset,
      [&](const ShaderModuleDesc& desc,
          uint32_t first,
          uint32_t count,
          const std::vector<RstbEntryView>& entries) {
        rcp<ShaderModule> module = ore.makeShaderModule(desc);
        if (module == nullptr)
        {
          return false;
        }
        for (uint32_t e = first; e < first + count; e++)
        {
          if (entries[e].stage == 0)
          {
            vertex = module;
            vertexEntry = entries[e].physical;
          }
          else if (entries[e].stage == 1)
          {
            fragment = module;
            fragmentEntry = entries[e].physical;
          }
        }
        return true;
      });
  if (!built || vertex == nullptr || fragment == nullptr)
  {
    return nullptr;
  }
  if (m_layout == nullptr)
  {
    // Every mode binds the same two textures, so one layout serves them all.
    m_layout = makeBindGroupLayoutFromBindingMap(
        ore,
        bindingMapForStages(vertex.get(), fragment.get()),
        0,
        nullptr,
        0,
        vertex.get(),
        fragment.get());
    if (m_layout == nullptr)
    {
      return nullptr;
    }
  }

  BindGroupLayout* layouts[] = {m_layout.get()};
  PipelineDesc desc{};
  desc.vertexModule = vertex.get();
  desc.vertexEntryPoint = vertexEntry.c_str();
  desc.fragmentModule = fragment.get();
  desc.fragmentEntryPoint = fragmentEntry.c_str();
  desc.colorTargets[0].format = ore.canvasTargetFormat();
  desc.colorCount = 1;
  desc.bindGroupLayouts = layouts;
  desc.bindGroupLayoutCount = 1;
  desc.label = "layer mask";
  m_pipelines[i] = ore.makePipeline(desc);
  return m_pipelines[i].get();
}

bool LayerMaskCompositor::compose(
    Context& ore,
    gpu::RenderCanvas* content,
    gpu::RenderCanvas* coverage,
    gpu::RenderCanvas* result,
    LayerMaskMode mode)
{
  Pipeline* pipeline = m_failed ? nullptr : pipelineFor(ore, mode);
  if (pipeline == nullptr)
  {
    m_failed = true;
    return false;
  }
  rcp<TextureView> contentView = ore.wrapCanvasSampleView(content);
  rcp<TextureView> coverageView = ore.wrapCanvasSampleView(coverage);
  rcp<TextureView> resultView = ore.wrapCanvasTexture(result);
  if (contentView == nullptr || coverageView == nullptr ||
      resultView == nullptr)
  {
    return false;
  }
  BindGroupDesc::TexEntry textures[] = {
      {0, contentView.get()},
      {1, coverageView.get()},
  };
  BindGroupDesc groupDesc{};
  groupDesc.layout = m_layout.get();
  groupDesc.textures = textures;
  groupDesc.textureCount = 2;
  rcp<BindGroup> group = ore.makeBindGroup(groupDesc);
  if (group == nullptr)
  {
    return false;
  }

  RenderPassDesc passDesc{};
  passDesc.colorAttachments[0].view = resultView.get();
  passDesc.colorAttachments[0].loadOp = LoadOp::clear;
  passDesc.colorAttachments[0].storeOp = StoreOp::store;
  passDesc.colorAttachments[0].clearColor = {0.f, 0.f, 0.f, 0.f};
  passDesc.colorCount = 1;
  passDesc.label = "layer mask";
  auto pass = ore.beginRenderPass(passDesc);
  if (pass == nullptr)
  {
    return false;
  }
  pass->setPipeline(pipeline);
  pass->setBindGroup(0, group.get());
  pass->setViewport(
      0,
      0,
      static_cast<float>(result->width()),
      static_cast<float>(result->height()));
  pass->draw(3);
  pass->finish();
  return true;
}
} // namespace rive::ore
