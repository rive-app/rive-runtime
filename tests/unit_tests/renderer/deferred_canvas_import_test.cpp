/*
 * Copyright 2026 Rive
 */

// Image:view() on a canvas records a sample view and the consumer performs the
// real wrap at replay. The wrap is where GL inserts its Y flip companion, so it
// has to land after the canvas content that wrote the source, even when a
// script writes and samples one canvas in the same frame.

#include "deferred_test_sink.hpp"
#include "rive/renderer/cmd/deferred_replayer.hpp"
#include "rive/renderer/cmd/deferred_session.hpp"
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/rive_render_image.hpp"

#include <catch.hpp>

using namespace rive;
using namespace rive::cmd;

namespace
{
struct FakeTarget : gpu::RenderTarget
{
    FakeTarget() : gpu::RenderTarget(8, 8) {}
};

struct FakeImage : RiveRenderImage
{
    FakeImage() : RiveRenderImage(8, 8) {}
};

rcp<gpu::RenderCanvas> fakeCanvas()
{
    return make_rcp<gpu::RenderCanvas>(make_rcp<FakeImage>(),
                                       make_rcp<FakeTarget>());
}

// GPU free stand-in for the replaying backend. Only the canvas wraps are
// exercised; everything else a real replay would reach is unreachable here.
class RecordingOreContext : public ore::Context
{
public:
    RecordingOreContext() : ore::Context(nullptr) {}

    std::vector<gpu::RenderCanvas*> sampleWraps;

    rcp<ore::TextureView> wrapCanvasSampleView(
        gpu::RenderCanvas* canvas) override
    {
        sampleWraps.push_back(canvas);
        return nullptr;
    }

    rcp<ore::Buffer> makeBuffer(const ore::BufferDesc&) override
    {
        return nullptr;
    }
    rcp<ore::Texture> makeTexture(const ore::TextureDesc&) override
    {
        return nullptr;
    }
    rcp<ore::TextureView> makeTextureView(const ore::TextureViewDesc&) override
    {
        return nullptr;
    }
    rcp<ore::Sampler> makeSampler(const ore::SamplerDesc&) override
    {
        return nullptr;
    }
    rcp<ore::ShaderModule> makeShaderModule(
        const ore::ShaderModuleDesc&) override
    {
        return nullptr;
    }
    rcp<ore::BindGroupLayout> makeBindGroupLayout(
        const ore::BindGroupLayoutDesc&) override
    {
        return nullptr;
    }
    rcp<ore::Pipeline> makePipeline(const ore::PipelineDesc&,
                                    std::string*) override
    {
        return nullptr;
    }
    rcp<ore::BindGroup> makeBindGroup(const ore::BindGroupDesc&) override
    {
        return nullptr;
    }
    std::unique_ptr<ore::RenderPass> beginRenderPass(const ore::RenderPassDesc&,
                                                     std::string*) override
    {
        return nullptr;
    }
    void beginFrame(const FrameDescriptor&) override {}
    void endFrame() override {}
    void waitForGPU() override {}
    rcp<ore::TextureView> wrapCanvasTexture(gpu::RenderCanvas*) override
    {
        return nullptr;
    }
    rcp<ore::TextureView> wrapRiveTexture(gpu::Texture*,
                                          uint32_t,
                                          uint32_t) override
    {
        return nullptr;
    }
    ore::ShaderTarget shaderTarget() const override
    {
        return ore::ShaderTarget::glsl;
    }
};

// Logs the replay steps whose relative order the import depends on.
class ImportOrderSink : public deferred_test::TestSink
{
public:
    RecordingOreContext ore;
    std::vector<std::string> steps;

    rive::ore::Context* oreContext() override { return &ore; }

    Renderer* beginCanvasContent(gpu::RenderCanvas* canvas, uint32_t) override
    {
        m_openCanvas = canvas;
        m_canvasRenderer = serializingFactory.makeRenderer();
        return m_canvasRenderer.get();
    }
    void endCanvasContent() override
    {
        steps.push_back("content");
        m_contentFlushed.push_back(m_openCanvas);
        m_openCanvas = nullptr;
        m_canvasRenderer.reset();
    }
    void beginOreFrame() override { steps.push_back("ore"); }

    const std::vector<gpu::RenderCanvas*>& contentFlushed() const
    {
        return m_contentFlushed;
    }

private:
    gpu::RenderCanvas* m_openCanvas = nullptr;
    std::unique_ptr<Renderer> m_canvasRenderer;
    std::vector<gpu::RenderCanvas*> m_contentFlushed;
};
} // namespace

TEST_CASE("a canvas written and sampled in one frame wraps after its content",
          "[deferred][canvas-import]")
{
    DeferredSession session(nullptr);
    auto canvas = fakeCanvas();

    // What a script does: draw into the canvas, then Image:view() it.
    Renderer* content = session.beginCanvasContent(canvas.get(), 0xFF000000);
    auto paint = session.makeRenderPaint();
    auto path = session.makeEmptyRenderPath();
    content->drawPath(path.get(), paint.get());
    session.endCanvasContent(canvas.get());
    REQUIRE(session.oreContext().recordWrapCanvasImage(canvas->renderImage(),
                                                       8,
                                                       8) != nullptr);
    session.closeOpenRange();

    auto frame = snapshotFrame(session);
    ImportOrderSink sink;
    DeferredReplayer replayer;
    replayer.replayFrame(frame, sink);

    REQUIRE(sink.ore.sampleWraps.size() == 1);
    // Resolved off the canvas the view was recorded against, not a stale id.
    CHECK(sink.ore.sampleWraps[0] == canvas.get());
    REQUIRE(sink.contentFlushed().size() == 1);
    CHECK(sink.contentFlushed()[0] == canvas.get());
    // The companion the wrap builds is only as fresh as the content behind it.
    REQUIRE(sink.steps == std::vector<std::string>{"content", "ore"});
}

TEST_CASE("each recorded canvas view resolves to its own canvas",
          "[deferred][canvas-import]")
{
    DeferredSession session(nullptr);
    auto canvasA = fakeCanvas();
    auto canvasB = fakeCanvas();

    for (gpu::RenderCanvas* canvas : {canvasA.get(), canvasB.get()})
    {
        Renderer* content = session.beginCanvasContent(canvas, 0);
        auto paint = session.makeRenderPaint();
        auto path = session.makeEmptyRenderPath();
        content->drawPath(path.get(), paint.get());
        session.endCanvasContent(canvas);
        session.oreContext().recordWrapCanvasImage(canvas->renderImage(), 8, 8);
    }
    session.closeOpenRange();

    auto frame = snapshotFrame(session);
    ImportOrderSink sink;
    DeferredReplayer replayer;
    replayer.replayFrame(frame, sink);

    REQUIRE(sink.ore.sampleWraps.size() == 2);
    CHECK(sink.ore.sampleWraps[0] == canvasA.get());
    CHECK(sink.ore.sampleWraps[1] == canvasB.get());
}
