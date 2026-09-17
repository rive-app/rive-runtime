/*
 * Copyright 2026 Rive
 */

// Binds the vertex buffer and the index buffer at nonzero offsets. Both
// buffers open with junk the offsets skip: two off screen vertices and four
// zero indices, which would draw a degenerate triangle if either offset were
// dropped. Expected: the ore_triangle image.

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
constexpr uint32_t kJunkVertexCount = 2;
constexpr uint32_t kJunkIndexCount = 4;

const ore_gm::TriVertex kVertices[kJunkVertexCount + 3] = {
    {-2.0f, -2.0f, 1.0f, 1.0f, 1.0f, 1.0f},
    {-2.0f, -2.0f, 1.0f, 1.0f, 1.0f, 1.0f},
    {0.0f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f},
    {-0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f},
    {0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 1.0f},
};

// Trailing zero keeps the buffer four byte aligned.
const uint16_t kIndices[kJunkIndexCount + 4] = {0, 0, 0, 0, 0, 1, 2, 0};
} // namespace
#endif

class OreBufferOffsetsGM : public GM
{
public:
    OreBufferOffsetsGM() : GM(256, 256) {}

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
        vboDesc.label = "ore_buffer_offsets_vbo";
        auto vbo = ctx.makeBuffer(vboDesc);
        BufferDesc iboDesc{};
        iboDesc.usage = BufferUsage::index;
        iboDesc.size = sizeof(kIndices);
        iboDesc.data = kIndices;
        iboDesc.label = "ore_buffer_offsets_ibo";
        auto ibo = ctx.makeBuffer(iboDesc);

        ore_gm::TrianglePipeline pipe(shader,
                                      TextureFormat::rgba8unorm,
                                      "ore_buffer_offsets_pipeline");
        auto pipeline = ctx.makePipeline(pipe.desc);

        m_ore.beginFrame(renderContext);

        ColorAttachment ca{};
        ca.view = colorTarget.get();
        ca.loadOp = LoadOp::clear;
        ca.storeOp = StoreOp::store;
        ca.clearColor = {0.1f, 0.1f, 0.1f, 1.0f};

        RenderPassDesc rpDesc{};
        rpDesc.colorAttachments[0] = ca;
        rpDesc.colorCount = 1;
        rpDesc.label = "ore_buffer_offsets_pass";

        auto pass = ctx.beginRenderPass(rpDesc);
        pass->setPipeline(pipeline.get());
        pass->setVertexBuffer(0,
                              vbo.get(),
                              kJunkVertexCount * sizeof(ore_gm::TriVertex));
        pass->setIndexBuffer(ibo.get(),
                             IndexFormat::uint16,
                             kJunkIndexCount * sizeof(uint16_t));
        pass->setViewport(0, 0, 256, 256);
        pass->drawIndexed(3);
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

GMREGISTER(ore_buffer_offsets, return new OreBufferOffsetsGM())
