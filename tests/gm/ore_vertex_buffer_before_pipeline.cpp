/*
 * Copyright 2026 Rive
 */

// Binds the vertex buffer, plus a slot the pipeline never declares, before
// setPipeline, then switches pipelines without rebinding. WebGPU and Metal
// accept either order; GL and D3D read the layout at bind time, so they must
// apply the bound buffers whenever a pipeline becomes current. Expected: two
// triangles, left and right.

#include "gm.hpp"
#include "gmutils.hpp"
#include "ore_gm_helper.hpp"
#if ORE_GM_HAS_BACKEND
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_texture.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/renderer/ore/ore_render_pass.hpp"
#endif

using namespace rivegm;
using namespace rive;
using namespace rive::gpu;
#if ORE_GM_HAS_BACKEND
using namespace rive::ore;

namespace
{
const ore_gm::TriVertex kVertices[] = {
    {0.0f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f},
    {-0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f},
    {0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 1.0f},
};

const float kUnusedSlotData[4] = {0, 0, 0, 0};
} // namespace
#endif

class OreVertexBufferBeforePipelineGM : public GM
{
public:
    OreVertexBufferBeforePipelineGM() : GM(256, 256) {}

    ColorInt clearColor() const override { return 0xff000000; }

    void onDraw(rive::Renderer* originalRenderer) override
    {
        auto renderContext = TestingWindow::Get()->renderContext();
        if (!renderContext || !m_ore.ensureContext(renderContext))
            return;

#if ORE_GM_HAS_BACKEND
        auto& ctx = *renderContext->getOreContext();
        auto canvas = renderContext->makeRenderCanvas(256, 256);
        if (!canvas)
            return;
        auto colorTarget = ctx.wrapCanvasTexture(canvas.get());
        if (!colorTarget)
            return;

        auto shader = ore_gm::loadShader(ctx, ore_gm::kTriangle);
        if (!shader.vsModule)
            return;

        BufferDesc vboDesc{};
        vboDesc.usage = BufferUsage::vertex;
        vboDesc.size = sizeof(kVertices);
        vboDesc.data = kVertices;
        vboDesc.label = "ore_vb_before_pipeline_vbo";
        auto vbo = ctx.makeBuffer(vboDesc);
        BufferDesc unusedDesc{};
        unusedDesc.usage = BufferUsage::vertex;
        unusedDesc.size = sizeof(kUnusedSlotData);
        unusedDesc.data = kUnusedSlotData;
        unusedDesc.label = "ore_vb_before_pipeline_unused_slot";
        auto unused = ctx.makeBuffer(unusedDesc);

        ore_gm::TrianglePipeline pipeA(shader,
                                       TextureFormat::rgba8unorm,
                                       "ore_vb_before_pipeline_a");
        auto pipelineA = ctx.makePipeline(pipeA.desc);
        // A distinct pipeline object with the same layout: switching to it
        // must carry the bound vertex buffer over.
        ore_gm::TrianglePipeline pipeB(shader,
                                       TextureFormat::rgba8unorm,
                                       "ore_vb_before_pipeline_b");
        auto pipelineB = ctx.makePipeline(pipeB.desc);

        m_ore.beginFrame(renderContext);

        ColorAttachment ca{};
        ca.view = colorTarget.get();
        ca.loadOp = LoadOp::clear;
        ca.storeOp = StoreOp::store;
        ca.clearColor = {0.1f, 0.1f, 0.1f, 1.0f};

        RenderPassDesc rpDesc{};
        rpDesc.colorAttachments[0] = ca;
        rpDesc.colorCount = 1;
        rpDesc.label = "ore_vertex_buffer_before_pipeline_pass";

        auto pass = ctx.beginRenderPass(rpDesc);
        pass->setVertexBuffer(0, vbo.get());
        pass->setVertexBuffer(1, unused.get());
        pass->setPipeline(pipelineA.get());
        pass->setViewport(0, 64, 128, 128);
        pass->draw(3);
        pass->setPipeline(pipelineB.get());
        pass->setViewport(128, 64, 128, 128);
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
    ore_gm::OreGMContext m_ore;
};

GMREGISTER(ore_vertex_buffer_before_pipeline,
           return new OreVertexBufferBeforePipelineGM())
