/*
 * Copyright 2026 Rive
 */

// Allocates past one descriptor pool generation to force a rollover then
// drops most of the bind groups. Validation layer catches lifecycle bugs
// at vkDestroyDevice. Renders the witness output for golden compare.

#include "gm.hpp"
#include "gmutils.hpp"
#include "ore_gm_helper.hpp"

#include <vector>

#if defined(ORE_BACKEND_METAL) || defined(ORE_BACKEND_GL) ||                   \
    defined(ORE_BACKEND_VK) || defined(ORE_BACKEND_WGPU) ||                    \
    defined(ORE_BACKEND_D3D11) || defined(ORE_BACKEND_D3D12)
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_shader_module.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/renderer/ore/ore_render_pass.hpp"
#if defined(ORE_BACKEND_VK)
#include "rive/renderer/ore/ore_context_vulkan.hpp"
#endif
using namespace rive::ore;
#define ORE_POOL_ROLLOVER_ACTIVE
#endif

using namespace rivegm;
using namespace rive;
using namespace rive::gpu;

class OrePoolRolloverGM : public GM
{
public:
    OrePoolRolloverGM() : GM(128, 128) {}

    ColorInt clearColor() const override { return 0xff000000; }

    void onDraw(rive::Renderer* originalRenderer) override
    {
#ifdef ORE_POOL_ROLLOVER_ACTIVE
        auto renderContext = TestingWindow::Get()->renderContext();
        if (!renderContext || !m_ore.ensureContext(renderContext))
            return;
        auto& ctx = *renderContext->getOreContext();

        struct Uniforms
        {
            float r, g, b, a;
        };
        static const Uniforms kLow = {0.3f, 0.0f, 0.0f, 0.0f};
        static const Uniforms kHigh = {0.0f, 0.6f, 0.0f, 0.0f};

        BufferDesc bd{};
        bd.usage = BufferUsage::uniform;
        bd.size = sizeof(Uniforms);
        bd.data = &kLow;
        bd.label = "rollover_ubo_low";
        auto lowBuf = ctx.makeBuffer(bd);
        bd.data = &kHigh;
        bd.label = "rollover_ubo_high";
        auto highBuf = ctx.makeBuffer(bd);

        auto shader = ore_gm::loadShader(ctx, ore_gm::kBindingWitness);
        if (!shader.vsModule)
            return;
        auto layout0 =
            ore_gm::makeLayoutFromShader(ctx, shader.vsModule.get(), 0);

        BindGroupDesc::UBOEntry uboEntries[2]{};
        uboEntries[0].slot = 0;
        uboEntries[0].buffer = lowBuf.get();
        uboEntries[0].offset = 0;
        uboEntries[0].size = sizeof(Uniforms);
        uboEntries[1].slot = 7;
        uboEntries[1].buffer = highBuf.get();
        uboEntries[1].offset = 0;
        uboEntries[1].size = sizeof(Uniforms);

        BindGroupDesc bgDesc{};
        bgDesc.layout = layout0.get();
        bgDesc.ubos = uboEntries;
        bgDesc.uboCount = 2;
        bgDesc.label = "rollover_bg";

        // One past the per-generation cap forces a rollover and trips
        // the retry path in makeBindGroup.
#if defined(ORE_BACKEND_VK)
        constexpr int kCount =
            DescriptorPoolGeneration::kMaxSetsPerGeneration + 1;
#else
        constexpr int kCount = 257;
#endif
        std::vector<rcp<BindGroup>> bgs;
        bgs.reserve(kCount);
        for (int i = 0; i < kCount; ++i)
        {
            auto bg = ctx.makeBindGroup(bgDesc);
            if (!bg)
                return;
            bgs.push_back(std::move(bg));
        }
        // Keep an early bind group alive so its generation can't die yet,
        // then drop everything else. Draw with a post-rollover bind group
        // to prove sets are valid after the generation roll.
        auto firstGenBg = std::move(bgs.front());
        auto finalBg = std::move(bgs.back());
        bgs.clear();
        (void)firstGenBg;

        BindGroupLayout* layouts[] = {layout0.get()};
        PipelineDesc pipeDesc{};
        pipeDesc.vertexModule = shader.vsModule.get();
        pipeDesc.fragmentModule = shader.psModule.get();
        pipeDesc.vertexEntryPoint = shader.vsEntryPoint;
        pipeDesc.fragmentEntryPoint = shader.fsEntryPoint;
        pipeDesc.vertexBufferCount = 0;
        pipeDesc.topology = PrimitiveTopology::triangleList;
        pipeDesc.colorTargets[0].format = TextureFormat::rgba8unorm;
        pipeDesc.colorCount = 1;
        pipeDesc.depthStencil.depthCompare = CompareFunction::always;
        pipeDesc.depthStencil.depthWriteEnabled = false;
        pipeDesc.bindGroupLayouts = layouts;
        pipeDesc.bindGroupLayoutCount = 1;
        pipeDesc.label = "ore_pool_rollover_pipeline";
        auto pipeline = ctx.makePipeline(pipeDesc);
        if (!pipeline)
            return;

        auto canvas = renderContext->makeRenderCanvas(128, 128);
        if (!canvas)
            return;
        auto canvasTarget = ctx.wrapCanvasTexture(canvas.get());
        if (!canvasTarget)
            return;

        m_ore.beginFrame(renderContext);

        ColorAttachment ca{};
        ca.view = canvasTarget.get();
        ca.loadOp = LoadOp::clear;
        ca.storeOp = StoreOp::store;
        ca.clearColor = {0, 0, 0, 1};
        RenderPassDesc rpDesc{};
        rpDesc.colorAttachments[0] = ca;
        rpDesc.colorCount = 1;
        rpDesc.label = "ore_pool_rollover_pass";

        auto pass = ctx.beginRenderPass(rpDesc);
        pass->setPipeline(pipeline.get());
        pass->setBindGroup(0, finalBg.get());
        pass->draw(3);
        pass->finish();

        m_ore.endFrame(renderContext);
        ore_gm::invalidateGLStateAfterOre(renderContext);

        originalRenderer->save();
        originalRenderer->drawImage(canvas->renderImage(),
                                    {.filter = ImageFilter::nearest},
                                    BlendMode::srcOver,
                                    1);
        originalRenderer->restore();
#endif
    }

private:
#ifdef ORE_POOL_ROLLOVER_ACTIVE
    ore_gm::OreGMContext m_ore;
#endif
};

GMREGISTER(ore_pool_rollover, return new OrePoolRolloverGM)