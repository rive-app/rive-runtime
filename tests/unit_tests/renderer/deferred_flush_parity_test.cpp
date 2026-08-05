/*
 * Copyright 2026 Rive
 */

// Gate for the deferred recording changes: the same riv drawn immediately and
// drawn through a record + replay round trip must ask the render context for
// the same GPU work, flush for flush. Ported from the draw-time serialization
// tree so both sides are held to one bar.

#include "rive/renderer/cmd/deferred_render_factory.hpp"
#include "rive/renderer/cmd/deferred_replayer.hpp"
#include "rive/renderer/cmd/deferred_session.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "common/render_context_null.hpp"
#include "rive_file_reader.hpp"
#include "rive/scene.hpp"

#include <catch.hpp>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

using namespace rive;
using namespace rive::cmd;

namespace
{
constexpr int kFrames = 30;
constexpr int kFirstSteadyFrame = 2;
constexpr float kFrameSeconds = 1.0f / 60;

void advanceFrame(Scene* scene, ArtboardInstance* artboard, int frame)
{
    float seconds = frame == 0 ? 0 : kFrameSeconds;
    if (scene != nullptr)
    {
        scene->advanceAndApply(seconds);
    }
    else
    {
        artboard->advance(seconds);
    }
}

void drawFrame(Scene* scene, ArtboardInstance* artboard, Renderer* renderer)
{
    renderer->save();
    if (scene != nullptr)
    {
        scene->draw(renderer);
    }
    else
    {
        artboard->draw(renderer);
    }
    renderer->restore();
}

// What one frame asked of the render context, summed over its flushes.
struct FlushStats
{
    uint64_t flushes = 0;
    uint64_t pathCount = 0;
    uint64_t contourCount = 0;
    uint64_t tessVertexSpans = 0;
    uint64_t gradSpans = 0;
    uint64_t gradDataHeight = 0;
    uint64_t tessDataHeight = 0;
    uint64_t atlasFillBatches = 0;
    uint64_t atlasStrokeBatches = 0;
    uint64_t atlasContentArea = 0;
};

FlushStats operator-(const FlushStats& a, const FlushStats& b)
{
    return {a.flushes - b.flushes,
            a.pathCount - b.pathCount,
            a.contourCount - b.contourCount,
            a.tessVertexSpans - b.tessVertexSpans,
            a.gradSpans - b.gradSpans,
            a.gradDataHeight - b.gradDataHeight,
            a.tessDataHeight - b.tessDataHeight,
            a.atlasFillBatches - b.atlasFillBatches,
            a.atlasStrokeBatches - b.atlasStrokeBatches,
            a.atlasContentArea - b.atlasContentArea};
}

class FlushObservingNULL : public RenderContextNULL
{
public:
    FlushStats stats;
    uint32_t featuresEver = 0; // union of combinedShaderFeatures

    void flush(const gpu::FlushDescriptor& d) override
    {
        featuresEver |= static_cast<uint32_t>(d.combinedShaderFeatures);
        stats.flushes++;
        stats.pathCount += d.pathCount;
        stats.contourCount += d.contourCount;
        stats.tessVertexSpans += d.tessVertexSpanCount;
        stats.gradSpans += d.gradSpanCount;
        stats.gradDataHeight += d.gradDataHeight;
        stats.tessDataHeight += d.tessDataHeight;
        stats.atlasFillBatches += d.featherAtlasFillBatchCount;
        stats.atlasStrokeBatches += d.featherAtlasStrokeBatchCount;
        stats.atlasContentArea += uint64_t(d.featherAtlasContentWidth) *
                                  uint64_t(d.featherAtlasContentHeight);
    }
};

class ObservingContext : public gpu::RenderContext
{
public:
    ObservingContext() : RenderContext(std::make_unique<FlushObservingNULL>())
    {}
    FlushObservingNULL* observer()
    {
        return static_impl_cast<FlushObservingNULL>();
    }
};

std::vector<FlushStats> runImmediate(const char* rivPath, uint32_t* features)
{
    ObservingContext ctx;
    auto file = ReadRiveFile(rivPath, &ctx);
    auto artboard = file->artboardDefault();
    auto scene = artboard->defaultScene();
    uint32_t w = static_cast<uint32_t>(std::ceil(artboard->width()));
    uint32_t h = static_cast<uint32_t>(std::ceil(artboard->height()));
    auto rt = ctx.observer()->makeRenderTarget(w, h);

    std::vector<FlushStats> frames;
    for (int frame = 0; frame < kFrames; frame++)
    {
        advanceFrame(scene.get(), artboard.get(), frame);
        FlushStats before = ctx.observer()->stats;
        ctx.beginFrame({.renderTargetWidth = w, .renderTargetHeight = h});
        RiveRenderer renderer(&ctx);
        drawFrame(scene.get(), artboard.get(), &renderer);
        ctx.flush({.renderTarget = rt.get()});
        frames.push_back(ctx.observer()->stats - before);
    }
    *features = ctx.observer()->featuresEver;
    return frames;
}

// Opens the real screen frame on the observed context, like the host sinks.
class NullContextSink : public DeferredFrameSink
{
public:
    NullContextSink(ObservingContext* ctx, uint32_t w, uint32_t h) :
        m_ctx(ctx), m_width(w), m_height(h)
    {}

    Factory* factory() override { return m_ctx; }
    ore::Context* oreContext() override { return nullptr; }
    // Parity is defined against the single render target the immediate side
    // draws, so a second target would have nothing to compare with.
    Renderer* beginScreenFrame(uint64_t target) override
    {
        REQUIRE(target == 0);
        m_ctx->beginFrame(
            {.renderTargetWidth = m_width, .renderTargetHeight = m_height});
        m_renderer = std::make_unique<RiveRenderer>(m_ctx);
        return m_renderer.get();
    }
    bool frameOpen() const { return m_renderer != nullptr; }
    void closeFrame() { m_renderer = nullptr; }

private:
    ObservingContext* m_ctx;
    uint32_t m_width, m_height;
    std::unique_ptr<RiveRenderer> m_renderer;
};

std::vector<FlushStats> runDeferred(const char* rivPath)
{
    DeferredSession session(nullptr);
    auto file = ReadRiveFile(rivPath, &session);
    auto artboard = file->artboardDefault();
    auto scene = artboard->defaultScene();
    uint32_t w = static_cast<uint32_t>(std::ceil(artboard->width()));
    uint32_t h = static_cast<uint32_t>(std::ceil(artboard->height()));

    ObservingContext ctx;
    auto rt = ctx.observer()->makeRenderTarget(w, h);
    NullContextSink sink(&ctx, w, h);
    DeferredReplayer replayer;

    std::vector<FlushStats> frames;
    for (int frame = 0; frame < kFrames; frame++)
    {
        advanceFrame(scene.get(), artboard.get(), frame);
        drawFrame(scene.get(), artboard.get(), session.screenRenderer());
        DeferredFrame snapshot = snapshotFrame(session);
        session.resetFrame();

        FlushStats before = ctx.observer()->stats;
        replayer.replayFrame(snapshot, sink);
        CHECK(replayer.droppedDraws() == 0);
        if (sink.frameOpen())
        {
            ctx.flush({.renderTarget = rt.get()});
            sink.closeFrame();
        }
        frames.push_back(ctx.observer()->stats - before);
    }
    return frames;
}

void printFlushParity(const char* name,
                      const std::vector<FlushStats>& imm,
                      const std::vector<FlushStats>& def)
{
    auto steadyAvg = [](const std::vector<FlushStats>& v, auto pick) {
        double sum = 0;
        for (size_t i = kFirstSteadyFrame; i < v.size(); i++)
        {
            sum += static_cast<double>(pick(v[i]));
        }
        return sum / static_cast<double>(v.size() - kFirstSteadyFrame);
    };
    printf("\n== %s flush parity (steady state, per frame) ==\n", name);
    printf("  %-18s %12s %12s\n", "", "immediate", "deferred");
    auto row = [&](const char* label, auto pick) {
        printf("  %-18s %12.1f %12.1f\n",
               label,
               steadyAvg(imm, pick),
               steadyAvg(def, pick));
    };
    row("flushes", [](const FlushStats& s) { return s.flushes; });
    row("paths", [](const FlushStats& s) { return s.pathCount; });
    row("contours", [](const FlushStats& s) { return s.contourCount; });
    row("tessSpans", [](const FlushStats& s) { return s.tessVertexSpans; });
    row("tessDataHeight", [](const FlushStats& s) { return s.tessDataHeight; });
    row("gradSpans", [](const FlushStats& s) { return s.gradSpans; });
    row("gradDataHeight", [](const FlushStats& s) { return s.gradDataHeight; });
    row("atlasFillBatches",
        [](const FlushStats& s) { return s.atlasFillBatches; });
    row("atlasStrokeBatches",
        [](const FlushStats& s) { return s.atlasStrokeBatches; });
    row("atlasContentArea",
        [](const FlushStats& s) { return s.atlasContentArea; });
}

void printShaderFeatures(uint32_t features)
{
    static const char* kNames[] = {"CLIPPING",
                                   "CLIP_RECT",
                                   "ADVANCED_BLEND",
                                   "FEATHER",
                                   "EVEN_ODD",
                                   "NESTED_CLIPPING",
                                   "HSL_BLEND_MODES",
                                   "DITHER"};
    printf("  shader features:");
    for (size_t i = 0; i < 8; i++)
    {
        if (features & (1u << i))
        {
            printf(" %s", kNames[i]);
        }
    }
    printf("\n");
}

// A missing riv fails the gate rather than passing vacuously.
void requireRiv(const std::string& path)
{
    FILE* fp = fopen(path.c_str(), "rb");
    if (fp == nullptr)
    {
        FAIL("flush parity riv missing: " << path);
    }
    fclose(fp);
}

void checkParity(const char* name,
                 const std::vector<FlushStats>& imm,
                 const std::vector<FlushStats>& def)
{
    // Equal flush structure means recording changed nothing the renderer can
    // see.
    for (size_t i = kFirstSteadyFrame; i < imm.size(); i++)
    {
        INFO(name << " frame " << i);
        CHECK(imm[i].flushes == def[i].flushes);
        CHECK(imm[i].tessVertexSpans == def[i].tessVertexSpans);
        CHECK(imm[i].atlasContentArea == def[i].atlasContentArea);
        CHECK(imm[i].gradDataHeight == def[i].gradDataHeight);
    }
}

void flushParity(const char* name)
{
    // Plain git assets so device deploys carry real bytes, not lfs pointers.
    std::string path = std::string("assets/parity/") + name;
    requireRiv(path);
    uint32_t features = 0;
    auto imm = runImmediate(path.c_str(), &features);
    auto def = runDeferred(path.c_str());
    printFlushParity(name, imm, def);
    printShaderFeatures(features);
    checkParity(name, imm, def);
}

// Whole corpus sweep, so properties the six named rivs never exercise
// (feathers above all) are still held to parity. Hidden because it is slow
// and needs the full lfs corpus; run with test.sh -m "[.corpus_parity]".
void flushParityQuiet(const std::string& name)
{
    std::string path = std::string("../../../../zzzgold/rivs/") + name;
    FILE* fp = fopen(path.c_str(), "rb");
    if (fp == nullptr)
    {
        return;
    }
    fclose(fp);
    uint32_t features = 0;
    auto imm = runImmediate(path.c_str(), &features);
    auto def = runDeferred(path.c_str());
    checkParity(name.c_str(), imm, def);
}
} // namespace

TEST_CASE("deferred flush parity, regressing rivs", "[deferred_flush_parity]")
{
    flushParity("Halloween_v3.riv");
    flushParity("UI_Swipe_left_to_delete.riv");
    flushParity("Tom_Morello.riv");
}

TEST_CASE("deferred flush parity, parity rivs", "[deferred_flush_parity]")
{
    flushParity("Knight_square_2.riv");
    flushParity("falling.riv");
    flushParity("popsicle_loader.riv");
}

TEST_CASE("deferred flush parity, whole corpus", "[.][corpus_parity]")
{
    std::vector<std::string> names;
    std::error_code ec;
    for (const auto& e :
         std::filesystem::directory_iterator("../../../../zzzgold/rivs/", ec))
    {
        std::string n = e.path().filename().string();
        if (n.size() > 4 && n.compare(n.size() - 4, 4, ".riv") == 0)
        {
            names.push_back(n);
        }
    }
    std::sort(names.begin(), names.end());
    printf("corpus flush parity over %zu rivs\n", names.size());
    for (const std::string& n : names)
    {
        flushParityQuiet(n);
    }
}
