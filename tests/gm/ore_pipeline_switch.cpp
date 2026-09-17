/*
 * Copyright 2026 Rive
 */

// Switches pipelines mid pass without rebinding the vertex buffer. The
// binding persists and the new pipeline's layout applies to it, so the
// second pipeline's stride of two structs reads structs 6, 8 and 10 for the
// right triangle. A backend that bakes the layout at bind time keeps the
// first stride and reads the zeroed structs 3 to 5 instead. Expected: a
// red/green/blue triangle on the left and a yellow/cyan/magenta one on the
// right.

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
// Structs 0 to 2 are the left triangle at the single stride, structs 6, 8
// and 10 the right one at the double stride from firstVertex 3. The trailing
// struct keeps the last double stride vertex whole, since Vulkan and D3D12
// may bounds check the full stride and zero the vertex otherwise.
const ore_gm::TriVertex kVertices[12] = {
    {-0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f},
    {-0.9f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f},
    {-0.1f, -0.5f, 0.0f, 0.0f, 1.0f, 1.0f},
    {},
    {},
    {},
    {0.5f, 0.5f, 1.0f, 1.0f, 0.0f, 1.0f},
    {},
    {0.1f, -0.5f, 0.0f, 1.0f, 1.0f, 1.0f},
    {},
    {0.9f, -0.5f, 1.0f, 0.0f, 1.0f, 1.0f},
    {},
};
} // namespace
#endif

class OrePipelineSwitchGM : public GM
{
public:
    OrePipelineSwitchGM() : GM(256, 256) {}

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
        vboDesc.label = "ore_pipeline_switch_vbo";
        auto vbo = ctx.makeBuffer(vboDesc);

        ore_gm::TrianglePipeline singleDesc(shader,
                                            TextureFormat::rgba8unorm,
                                            "ore_pipeline_switch_1x");
        ore_gm::TrianglePipeline twiceDesc(shader,
                                           TextureFormat::rgba8unorm,
                                           "ore_pipeline_switch_2x",
                                           2 * sizeof(ore_gm::TriVertex));
        auto single = ctx.makePipeline(singleDesc.desc);
        auto twice = ctx.makePipeline(twiceDesc.desc);
        if (!single || !twice)
            return;

        m_ore.beginFrame(renderContext);

        ColorAttachment ca{};
        ca.view = colorTarget.get();
        ca.loadOp = LoadOp::clear;
        ca.storeOp = StoreOp::store;
        ca.clearColor = {0.1f, 0.1f, 0.1f, 1.0f};

        RenderPassDesc rpDesc{};
        rpDesc.colorAttachments[0] = ca;
        rpDesc.colorCount = 1;
        rpDesc.label = "ore_pipeline_switch_pass";

        auto pass = ctx.beginRenderPass(rpDesc);
        pass->setViewport(0, 0, 256, 256);
        pass->setPipeline(single.get());
        pass->setVertexBuffer(0, vbo.get());
        pass->draw(3);
        pass->setPipeline(twice.get());
        pass->draw(3, 1, 3);
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

GMREGISTER(ore_pipeline_switch, return new OrePipelineSwitchGM())
