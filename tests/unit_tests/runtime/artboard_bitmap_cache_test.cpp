/*
 * Copyright 2026 Rive
 */

// Cache-as-bitmap validation for an artboard carrying a BitmapCache child.
//
// The feature only engages on the deferred/recording path (a factory that
// hands back both a render context and a DeferredCanvasHost), and only through
// Artboard::drawInternal -- Artboard::draw() is the standalone-root path and
// deliberately bypasses the cache. So this harness records into a
// DeferredSession bound to a GPU-free RenderContextNULL and draws through
// drawInternal, which is what a nested/instanced draw reaches.
//
// What it checks:
//   * the BitmapCache child imports and is found by the artboard,
//   * a cached draw rasterizes the content offscreen once and then composites
//     a single image instead of re-recording the vector content,
//   * a static frame re-uses the raster (no second rasterization),
//   * writing `resolution` invalidates the cache and re-rasterizes at the new
//     pixel size,
//   * a lower resolution costs less real GPU work, measured by replaying the
//     recorded frame against a null render context and reading the flush
//     descriptors it produces.

#include "rive/animation/state_machine_instance.hpp"
#include "rive/artboard.hpp"
#include "rive/bitmap_cache.hpp"
#include "rive/node.hpp"
#include "rive/file.hpp"
#include "rive/generated/core_registry.hpp"
#include "rive/renderer/cmd/deferred_replayer.hpp"
#include "rive/renderer/cmd/deferred_session.hpp"
#include "rive/renderer/cmd/render_commands.hpp"
#include "rive/renderer/cmd/render_replay.hpp"
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "common/render_context_null.hpp"
#include "rive_file_reader.hpp"
#include "utils/serializing_factory.hpp"

#include <catch.hpp>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

using namespace rive;
using namespace rive::cmd;

namespace
{
constexpr const char* kRiv = "assets/cache_as_bitmap_test.riv";

// The artboard carrying the BitmapCache, found rather than named so the tests
// do not encode which artboard the fixture happens to put it on. The file's
// default artboard is the root that nests this one.
std::unique_ptr<ArtboardInstance> cachedArtboardOf(File* file)
{
    for (size_t i = 0; i < file->artboardCount(); i++)
    {
        auto artboard = file->artboardAt(i);
        if (artboard != nullptr && artboard->bitmapCache() != nullptr)
        {
            return artboard;
        }
    }
    return nullptr;
}

// A node inside the cached artboard, used to force a content change. The
// fixture's state machine settles within a couple of frames, so any test that
// needs a *changed* frame has to make the change itself rather than wait for
// one.
Node* firstNodeOf(ArtboardInstance* artboard)
{
    for (Core* object : artboard->objects())
    {
        if (object != nullptr && object->is<Node>())
        {
            return object->as<Node>();
        }
    }
    return nullptr;
}

// Mirrors the clamps in Artboard::drawCachedAsBitmap. Duplicated on purpose:
// the test states the contract independently of the implementation so a
// silent change to either constant fails here.
constexpr float kMinRes = 0.01f;
constexpr float kMaxRes = 8.0f;
constexpr uint32_t kMaxDim = 2048;

uint32_t expectedDim(float natural, float resolution)
{
    float res = std::min(std::max(resolution, kMinRes), kMaxRes);
    return static_cast<uint32_t>(
        std::min(std::max(std::ceil(natural * res), 1.0f),
                 static_cast<float>(kMaxDim)));
}

// Records every offscreen rasterization the artboard asks for, with the pixel
// size it asked for. One entry per re-raster, so the vector's length is the
// cache-miss count and its contents are the resolution evidence.
class CountingSession : public DeferredSession
{
public:
    using DeferredSession::DeferredSession;

    struct Raster
    {
        uint32_t width;
        uint32_t height;
        uint64_t pixels() const
        {
            return static_cast<uint64_t>(width) * height;
        }
    };
    std::vector<Raster> rasters;
    // Stream bytes each bracket added, parallel to `rasters`.
    std::vector<size_t> rasterContentBytes;

    Renderer* beginCanvasContent(gpu::RenderCanvas* canvas,
                                 uint32_t clearColor) override
    {
        rasters.push_back({canvas->width(), canvas->height()});
        rasterContentBytes.push_back(commandBuffer().commandBytes().size());
        return DeferredSession::beginCanvasContent(canvas, clearColor);
    }

    void endCanvasContent(gpu::RenderCanvas* canvas) override
    {
        DeferredSession::endCanvasContent(canvas);
        rasterContentBytes.back() =
            commandBuffer().commandBytes().size() - rasterContentBytes.back();
    }
};

constexpr size_t kNumCmds = static_cast<size_t>(RenderCmd::lastRenderCmd) + 1;

// Per-frame opcode tally over the recorded 2D stream. Counting the draws the
// frame actually issued is how "the cache saved work" becomes a number rather
// than a claim.
struct Census
{
    uint64_t count[kNumCmds] = {};
    uint64_t commandBytes = 0;

    uint64_t drawOps() const
    {
        return count[static_cast<size_t>(RenderCmd::drawPath)] +
               count[static_cast<size_t>(RenderCmd::drawImage)] +
               count[static_cast<size_t>(RenderCmd::drawImageMesh)];
    }
    uint64_t drawPaths() const
    {
        return count[static_cast<size_t>(RenderCmd::drawPath)];
    }
    uint64_t drawImages() const
    {
        return count[static_cast<size_t>(RenderCmd::drawImage)];
    }
    uint64_t canvasOpens() const
    {
        return count[static_cast<size_t>(RenderCmd::canvasContentBegin)];
    }

    static Census of(const RenderCommandBuffer& buf)
    {
        Census c;
        c.commandBytes = buf.commandBytes().size();
        CommandReader<uint8_t> r(buf.commandBytes(), buf.blobBytes());
        uint8_t type;
        while (r.next(type))
        {
            if (type >= kNumCmds)
            {
                break;
            }
            c.count[type]++;
            r.skip(payloadSizeOf(static_cast<RenderCmd>(type)));
        }
        return c;
    }
};

// Placement slack. Device-scale sizing introduced two deliberate, bounded
// departures from "the quad lands exactly on the box":
//
//   * pixel snapping moves the composite by up to half a device pixel so the
//     raster samples texel centers instead of straddling them, and
//   * the raster is ceil()ed to whole texels, so the quad can overhang the box
//     by up to one texel on the right and bottom.
//
// Both are sub-pixel at the resolution being composited. Anything larger is a
// mapping bug, which is what these bounds are here to catch.
constexpr float kSnapSlack = 0.5f;

float texelSpan(const Vec2D& origin, const Vec2D& corner, uint32_t rasterDim)
{
    return std::abs(corner.x - origin.x) / static_cast<float>(rasterDim);
}

// Walks a recorded frame's screen segments, tracking the CTM, and returns the
// transform in force at the composite (the single drawImage the cache emits).
// This is how the "pixel exact with the vector path" claim gets checked: the
// image occupies [0,0,widthPx,heightPx] in local space, so CTM must map that
// box onto the artboard's box.
bool compositeTransform(const DeferredFrame& frame, Mat2D* out)
{
    bool found = false;
    // One stack across every screen segment: replay feeds them all to the same
    // renderer in record order, so a transform recorded before the offscreen
    // interruption is still in force in the segment that resumes after it.
    std::vector<Mat2D> stack{Mat2D()};
    for (const DeferredSegment& seg : frame.segments)
    {
        if (seg.target != DeferredSegment::Target::screen)
        {
            continue;
        }
        CommandReader<uint8_t> r(
            Span<const uint8_t>(frame.commands.data() + seg.begin,
                                seg.end - seg.begin),
            Span<const uint8_t>(frame.blobs.data(), frame.blobs.size()));
        uint8_t type;
        while (r.next(type))
        {
            RenderCmd cmd = static_cast<RenderCmd>(type);
            switch (cmd)
            {
                case RenderCmd::save:
                    stack.push_back(stack.back());
                    break;
                case RenderCmd::restore:
                    if (stack.size() > 1)
                    {
                        stack.pop_back();
                    }
                    break;
                case RenderCmd::transform:
                {
                    auto t = r.read<TransformPOD>();
                    stack.back() = stack.back() *
                                   Mat2D(t.xx, t.xy, t.yx, t.yy, t.tx, t.ty);
                    break;
                }
                case RenderCmd::drawImage:
                    r.skip(payloadSizeOf(cmd));
                    *out = stack.back();
                    found = true;
                    break;
                default:
                    r.skip(payloadSizeOf(cmd));
                    break;
            }
        }
    }
    return found;
}

// ---- real GPU work, counted off the flush descriptors ----

// What one replayed frame asked of the render context. These are the numbers
// that actually scale with raster resolution: tessellation is transform
// dependent, and the fill area is the texture the GPU has to cover.
struct GpuWork
{
    uint64_t flushes = 0;
    uint64_t pathCount = 0;
    uint64_t tessVertexSpans = 0;
    uint64_t tessDataHeight = 0;
    uint64_t targetPixels = 0; // summed render-target area across flushes
    // Feather work, which is where an artboard leaning on feathered paints
    // spends its GPU time. Batches are per path; the atlas area is the
    // offscreen coverage the backend has to fill before the paths draw.
    uint64_t atlasBatches = 0;
    uint64_t atlasArea = 0;

    GpuWork operator-(const GpuWork& o) const
    {
        return {flushes - o.flushes,
                pathCount - o.pathCount,
                tessVertexSpans - o.tessVertexSpans,
                tessDataHeight - o.tessDataHeight,
                targetPixels - o.targetPixels,
                atlasBatches - o.atlasBatches,
                atlasArea - o.atlasArea};
    }
};

class WorkObservingNULL : public RenderContextNULL
{
public:
    GpuWork work;
    // Union of the shader features every flush asked for, which is how to tell
    // whether the content actually exercises a given path (e.g. feathering).
    uint32_t featuresEver = 0;

    void flush(const gpu::FlushDescriptor& d) override
    {
        featuresEver |= static_cast<uint32_t>(d.combinedShaderFeatures);
        work.flushes++;
        work.pathCount += d.pathCount;
        work.tessVertexSpans += d.tessVertexSpanCount;
        work.tessDataHeight += d.tessDataHeight;
        if (d.renderTarget != nullptr)
        {
            work.targetPixels +=
                static_cast<uint64_t>(d.renderTarget->width()) *
                d.renderTarget->height();
        }
        work.atlasBatches +=
            d.featherAtlasFillBatchCount + d.featherAtlasStrokeBatchCount;
        work.atlasArea += static_cast<uint64_t>(d.featherAtlasContentWidth) *
                          d.featherAtlasContentHeight;
    }
};

class ObservingContext : public gpu::RenderContext
{
public:
    ObservingContext() : RenderContext(std::make_unique<WorkObservingNULL>()) {}
    WorkObservingNULL* observer()
    {
        return static_impl_cast<WorkObservingNULL>();
    }
};

// Replays a recorded frame for real: opens the offscreen canvas frames on the
// observed context and flushes each one, then the screen frame.
class ReplaySink : public DeferredFrameSink
{
public:
    ReplaySink(ObservingContext* ctx, uint32_t w, uint32_t h) :
        m_ctx(ctx), m_width(w), m_height(h)
    {
        m_screenTarget = m_ctx->observer()->makeRenderTarget(w, h);
    }

    Factory* factory() override { return m_ctx; }
    ore::Context* oreContext() override { return nullptr; }
    gpu::RenderContext* renderContext() override { return m_ctx; }

    Renderer* beginScreenFrame(uint64_t) override
    {
        m_ctx->beginFrame(
            {.renderTargetWidth = m_width, .renderTargetHeight = m_height});
        m_screen = std::make_unique<RiveRenderer>(m_ctx);
        return m_screen.get();
    }
    void flushScreen()
    {
        if (m_screen != nullptr)
        {
            m_ctx->flush({.renderTarget = m_screenTarget.get()});
            m_screen = nullptr;
        }
    }

    Renderer* beginCanvasContent(gpu::RenderCanvas* canvas, uint32_t) override
    {
        m_canvasTarget = ref_rcp(canvas->renderTarget());
        m_ctx->beginFrame({.renderTargetWidth = canvas->width(),
                           .renderTargetHeight = canvas->height()});
        m_canvas = std::make_unique<RiveRenderer>(m_ctx);
        return m_canvas.get();
    }
    void endCanvasContent() override
    {
        m_ctx->flush({.renderTarget = m_canvasTarget.get()});
        m_canvas = nullptr;
        m_canvasTarget = nullptr;
    }

private:
    ObservingContext* m_ctx;
    uint32_t m_width, m_height;
    rcp<gpu::RenderTarget> m_screenTarget;
    rcp<gpu::RenderTarget> m_canvasTarget;
    std::unique_ptr<RiveRenderer> m_screen;
    std::unique_ptr<RiveRenderer> m_canvas;
};

// One artboard + the session recording it, wired for cache-as-bitmap.
struct Harness
{
    std::unique_ptr<gpu::RenderContext> renderContext =
        RenderContextNULL::MakeContext();
    CountingSession session{rive::ore::ReplayCaps{}};
    rcp<File> file;
    std::unique_ptr<ArtboardInstance> artboard;
    std::unique_ptr<StateMachineInstance> stateMachine;
    rcp<ViewModelInstance> vmi;

    // Set to replay recorded frames against a real (null-device) context.
    ReplaySink* sink = nullptr;
    DeferredReplayer replayer;
    // The bytes the last frame() recorded, kept for stream inspection.
    DeferredFrame lastFrame;

    explicit Harness(bool cached = true)
    {
        // The artboard reads this off the factory to allocate the offscreen
        // canvas; without it drawCachedAsBitmap falls back to a vector draw.
        session.bindRenderContext(cached ? renderContext.get() : nullptr);
        file = ReadRiveFile(kRiv, &session);
        artboard = cachedArtboardOf(file.get());
        REQUIRE(artboard != nullptr);

        stateMachine = artboard->stateMachineAt(0);
        REQUIRE(stateMachine != nullptr);
        int viewModelId = artboard->viewModelId();
        vmi = viewModelId == -1 ? file->createViewModelInstance(artboard.get())
                                : file->createViewModelInstance(viewModelId, 0);
        // Optional: nothing here drives the cache through data binding, so a
        // fixture that exposes no view model is fine.
        if (vmi != nullptr)
        {
            stateMachine->bindViewModelInstance(vmi);
        }
    }

    // Written straight onto the core object rather than through a bound view
    // model property. Data binding is the editor's concern; what these tests
    // are about is what the *cache* does when resolution changes, and routing
    // through a view model only adds a fixture requirement that has to be
    // re-authored every time the file is regenerated.
    void setResolution(float value)
    {
        artboard->bitmapCache()->resolution(value);
    }

    // Advance (optionally by zero, which is how a "nothing moved" frame is
    // expressed) and record one frame through the cache-aware draw path.
    Census frame(float dt, const Mat2D* outerTransform = nullptr)
    {
        stateMachine->advanceAndApply(dt);
        Renderer* screen = session.screenRenderer();
        screen->save();
        if (outerTransform != nullptr)
        {
            screen->transform(*outerTransform);
        }
        artboard->drawInternal(screen);
        screen->restore();
        lastFrame = snapshotFrame(session);
        Census c = Census::of(session.commandBuffer());
        session.resetFrame();
        if (sink != nullptr)
        {
            replayer.replayFrame(lastFrame, *sink);
            sink->flushScreen();
        }
        return c;
    }

    // Frames rasterized so far.
    size_t rasterCount() const { return session.rasters.size(); }
    CountingSession::Raster lastRaster() const
    {
        REQUIRE(!session.rasters.empty());
        return session.rasters.back();
    }
};
} // namespace

TEST_CASE("the artboard imports its BitmapCache child", "[bitmap-cache]")
{
    Harness h;
    BitmapCache* cache = h.artboard->bitmapCache();
    REQUIRE(cache != nullptr);
    // Whatever the file authored, so long as it is usable.
    CHECK(cache->resolution() > 0.0f);

    printf("[bitmap-cache] artboard %s: %g x %g, authored resolution %g\n",
           h.artboard->name().c_str(),
           h.artboard->width(),
           h.artboard->height(),
           cache->resolution());
}

TEST_CASE("a cached artboard rasterizes offscreen and composites one image",
          "[bitmap-cache]")
{
    Harness h;
    h.setResolution(1.0f);
    Census first = h.frame(0.0f);

    // The content went into an offscreen canvas...
    REQUIRE(h.rasterCount() == 1);
    CHECK(first.canvasOpens() == 1);
    // ...and the screen got an image composite for it.
    CHECK(first.drawImages() >= 1);

    // At resolution 1 the raster is the artboard's natural pixel size.
    auto raster = h.lastRaster();
    CHECK(raster.width == expectedDim(h.artboard->width(), 1.0f));
    CHECK(raster.height == expectedDim(h.artboard->height(), 1.0f));

    // The same artboard drawn uncached records its vector content straight to
    // the screen: no canvas, no composite image standing in for the content.
    Harness uncached(/*cached=*/false);
    Census plain = uncached.frame(0.0f);
    CHECK(uncached.rasterCount() == 0);
    CHECK(plain.canvasOpens() == 0);
    CHECK(plain.drawPaths() > 0);

    printf("[bitmap-cache] first frame: cached draws=%llu (paths=%llu, "
           "images=%llu) vs uncached draws=%llu (paths=%llu)\n",
           (unsigned long long)first.drawOps(),
           (unsigned long long)first.drawPaths(),
           (unsigned long long)first.drawImages(),
           (unsigned long long)plain.drawOps(),
           (unsigned long long)plain.drawPaths());
}

TEST_CASE("cacheEnabled off falls back to a vector draw", "[bitmap-cache]")
{
    Harness h;
    h.setResolution(1.0f);
    BitmapCache* cache = h.artboard->bitmapCache();
    REQUIRE(cache != nullptr);
    // Bit 0 of the mask is on by default, so an existing file caches exactly as
    // it did before the flags existed.
    CHECK(cache->cacheEnabled());
    REQUIRE(h.frame(0.0f).canvasOpens() == 1);
    REQUIRE(h.rasterCount() == 1);

    // Written through the passthrough key, which is the path a keyframe or a
    // data bind takes: CoreRegistry rewrites bit 0 and calls the mask setter.
    CoreRegistry::setBool(cache,
                          BitmapCacheBase::cacheEnabledPropertyKey,
                          false);
    CHECK(!cache->cacheEnabled());
    // The sibling bit is untouched by a write to this one.
    CHECK(!cache->dither());

    // Now it draws like an artboard with no BitmapCache at all: vector content
    // straight to the screen, no offscreen canvas, no composite standing in.
    Census off = h.frame(0.0f);
    CHECK(h.rasterCount() == 1); // no new raster
    CHECK(off.canvasOpens() == 0);
    CHECK(off.drawImages() == 0);
    CHECK(off.drawPaths() > 0);

    // Matches the uncached control exactly.
    Harness uncached(/*cached=*/false);
    Census plain = uncached.frame(0.0f);
    CHECK(off.drawPaths() == plain.drawPaths());

    // Turning it back on rasterizes afresh -- the disable released the texture,
    // so there is nothing left to reuse. (m_canvas is private to the friend
    // draw path; a new raster is the observable proxy for the release.)
    CoreRegistry::setBool(cache,
                          BitmapCacheBase::cacheEnabledPropertyKey,
                          true);
    CHECK(cache->cacheEnabled());
    Census back = h.frame(0.0f);
    CHECK(h.rasterCount() == 2);
    CHECK(back.canvasOpens() == 1);
    CHECK(back.drawImages() >= 1);

    printf("[bitmap-cache] disabled: paths=%llu (uncached control %llu), "
           "re-enabled raster %ux%u\n",
           (unsigned long long)off.drawPaths(),
           (unsigned long long)plain.drawPaths(),
           h.lastRaster().width,
           h.lastRaster().height);
}

TEST_CASE("a rotated or scaled artboard falls back to a vector draw",
          "[bitmap-cache]")
{
    Harness h;
    h.setResolution(1.0f);
    // Baseline: an identity self transform caches as usual.
    REQUIRE(h.frame(0.0f).canvasOpens() == 1);
    REQUIRE(h.rasterCount() == 1);

    // drawContent applies the artboard's own rotation/scale, so a self
    // transform is baked into the raster -- while the target and the composite
    // are both sized and placed from the untransformed bounds. A scaled
    // artboard would be cropped to its original extent and a rotated one
    // flattened into an axis-aligned box whose footprint no longer matches the
    // vector draw, so these take the vector path instead.
    h.artboard->rotation(0.4f);
    REQUIRE(h.artboard->hasSelfTransform());
    Census rotated = h.frame(0.0f);
    CHECK(h.rasterCount() == 1); // no new raster
    CHECK(rotated.canvasOpens() == 0);
    CHECK(rotated.drawImages() == 0);
    CHECK(rotated.drawPaths() > 0);

    // Scale on its own is enough; rotation is not the only way in.
    h.artboard->rotation(0.0f);
    h.artboard->scaleX(2.0f);
    REQUIRE(h.artboard->hasSelfTransform());
    Census scaled = h.frame(0.0f);
    CHECK(h.rasterCount() == 1);
    CHECK(scaled.canvasOpens() == 0);
    CHECK(scaled.drawImages() == 0);
    CHECK(scaled.drawPaths() > 0);

    // And the same artboard drawn uncached records the same vector content, so
    // the fallback really is "as though it had no BitmapCache".
    Harness uncached(/*cached=*/false);
    uncached.artboard->scaleX(2.0f);
    CHECK(scaled.drawPaths() == uncached.frame(0.0f).drawPaths());

    // Back at identity the cache engages again. It rasterizes rather than
    // reusing the texture from the first frame: changing the transform marks
    // the artboard changed, and didChange() is the invalidation hook.
    h.artboard->scaleX(1.0f);
    REQUIRE(!h.artboard->hasSelfTransform());
    Census identity = h.frame(0.0f);
    CHECK(h.rasterCount() == 2);
    CHECK(identity.canvasOpens() == 1);
    CHECK(identity.drawImages() >= 1);

    // ...and once it settles, back to one composite and no vector content.
    Census settled = h.frame(0.0f);
    CHECK(h.rasterCount() == 2);
    CHECK(settled.canvasOpens() == 0);
    CHECK(settled.drawPaths() == 0);
    CHECK(settled.drawOps() == 1);
}

TEST_CASE("the flag bits are independent within the mask", "[bitmap-cache]")
{
    Harness h;
    BitmapCache* cache = h.artboard->bitmapCache();
    REQUIRE(cache != nullptr);

    // dither defaults off, cacheEnabled defaults on: mask initial value is 1.
    CHECK(cache->cacheEnabled());
    CHECK(!cache->dither());

    CoreRegistry::setBool(cache, BitmapCacheBase::ditherPropertyKey, true);
    CHECK(cache->dither());
    CHECK(cache->cacheEnabled()); // untouched

    CoreRegistry::setBool(cache,
                          BitmapCacheBase::cacheEnabledPropertyKey,
                          false);
    CHECK(!cache->cacheEnabled());
    CHECK(cache->dither()); // untouched

    // Both bits live in the one serialized property.
    // Copied into a temporary: Catch binds the right-hand side by const
    // reference, which would odr-use the in-class initialized constant and
    // ask the linker for a definition the generated header does not emit.
    CHECK(cache->cacheFlags() ==
          static_cast<uint32_t>(BitmapCacheBase::ditherBitmask));
}

TEST_CASE("a static frame reuses the raster instead of re-rasterizing",
          "[bitmap-cache]")
{
    Harness h;
    h.setResolution(1.0f);
    Census first = h.frame(0.0f);
    REQUIRE(h.rasterCount() == 1);

    // Nothing changed: the second frame must hit the cache.
    Census second = h.frame(0.0f);
    CHECK(h.rasterCount() == 1);
    CHECK(second.canvasOpens() == 0);
    CHECK(second.drawImages() == 1);
    // The whole artboard now costs one draw.
    CHECK(second.drawPaths() == 0);
    CHECK(second.drawOps() == 1);

    // And it stays that way.
    for (int i = 0; i < 8; i++)
    {
        Census c = h.frame(0.0f);
        CHECK(c.drawOps() == 1);
    }
    CHECK(h.rasterCount() == 1);

    Harness uncached(/*cached=*/false);
    uncached.frame(0.0f);
    Census plain = uncached.frame(0.0f);

    printf("[bitmap-cache] steady frame: cached draws=%llu bytes=%llu vs "
           "uncached draws=%llu bytes=%llu\n",
           (unsigned long long)second.drawOps(),
           (unsigned long long)second.commandBytes,
           (unsigned long long)plain.drawOps(),
           (unsigned long long)plain.commandBytes);

    // The point of the feature: a steady frame issues strictly less work.
    CHECK(second.drawOps() < plain.drawOps());
    CHECK(second.commandBytes < plain.commandBytes);
    CHECK(first.drawOps() >= second.drawOps());
}

TEST_CASE("a cache property change marks the artboard changed",
          "[bitmap-cache]")
{
    // The gap this guards: m_dirty is private to the BitmapCache, so a host
    // that skips frames while Artboard::didChange() is false would never
    // submit the frame that rebuilds the raster, and would keep compositing
    // the stale one forever.
    Harness h;
    h.setResolution(1.0f);
    h.frame(0.0f);
    REQUIRE(h.rasterCount() == 1);
    // A static frame settles the artboard: nothing moved, nothing changed.
    h.frame(0.0f);
    REQUIRE(!h.artboard->didChange());

    h.setResolution(2.0f);
    CHECK(h.artboard->didChange());
    Census resized = h.frame(0.0f);
    CHECK(h.rasterCount() == 2);
    CHECK(resized.canvasOpens() == 1);

    // And the same for a flag write, which arrives by the same route a
    // keyframe or a data bind would take.
    h.frame(0.0f);
    REQUIRE(!h.artboard->didChange());
    CoreRegistry::setBool(h.artboard->bitmapCache(),
                          BitmapCacheBase::cacheEnabledPropertyKey,
                          false);
    CHECK(h.artboard->didChange());
}

TEST_CASE("a transparent cached artboard still consumes its change",
          "[bitmap-cache]")
{
    // drawContent() clears m_didChange as its first statement, before its own
    // zero-opacity return, so the vector path always consumes the flag. The
    // cached path returns earlier, and a host that gates frame submission on
    // didChange() -- Unity reads it straight out of the native plugin -- would
    // otherwise resubmit a fully transparent artboard every frame, forever.
    Harness h;
    h.setResolution(1.0f);
    h.frame(0.0f);
    REQUIRE(h.rasterCount() == 1);
    h.frame(0.0f);
    REQUIRE(!h.artboard->didChange());

    Node* node = firstNodeOf(h.artboard.get());
    REQUIRE(node != nullptr);

    // Invisible *and* genuinely changed: the case where the flag would leak.
    h.artboard->hostOpacity(0.0f);
    node->x(node->x() + 10.0f);
    REQUIRE(h.artboard->didChange());

    Census hidden = h.frame(0.0f);
    // Nothing drawn and nothing rasterized...
    CHECK(hidden.canvasOpens() == 0);
    CHECK(h.rasterCount() == 1);
    // ...but the change is consumed, so the host settles instead of spinning.
    CHECK(!h.artboard->didChange());

    // And it stays settled while it remains transparent.
    h.frame(0.0f);
    CHECK(!h.artboard->didChange());

    // The content that moved while it was invisible must not composite as the
    // raster taken before the move. The consumed change is folded into the
    // cache's own dirty bit, so the frame that brings it back re-rasterizes.
    // (Restoring opacity marks the artboard changed too, so this passes on
    // either route -- the fold is what keeps it correct if that ever stops
    // being true.)
    h.artboard->hostOpacity(1.0f);
    Census shown = h.frame(0.0f);
    CHECK(shown.canvasOpens() == 1);
    CHECK(h.rasterCount() == 2);
}

TEST_CASE("an artboard re-rasterizes every frame its content changes",
          "[bitmap-cache]")
{
    // The invalidation hook is didChange(), so content that moves pays the
    // raster every frame plus the composite. Recording this is what makes the
    // feature's cost model legible: it is a win for static subtrees and a loss
    // for animating ones.
    //
    // The change is driven here rather than left to the fixture's state
    // machine. That machine settles within a couple of frames, so a test that
    // only advanced time would rasterize once and then measure nothing -- and
    // a `>= 1` bar would pass whether or not invalidation worked at all, which
    // is exactly the regression this is supposed to catch.
    Harness h;
    h.setResolution(0.5f);

    Node* node = firstNodeOf(h.artboard.get());
    REQUIRE(node != nullptr);

    h.frame(0.0f);
    REQUIRE(h.rasterCount() == 1);

    constexpr int kFrames = 30;
    for (int i = 0; i < kFrames; i++)
    {
        // Moves the content, which marks the artboard changed.
        node->x(node->x() + 1.0f);
        h.frame(1.0f / 60.0f);
    }
    printf("[bitmap-cache] %d changed frames -> %zu rasterizations\n",
           kFrames,
           h.rasterCount());
    // One per changed frame, plus the opening one. Anything less means a
    // changed artboard composited a stale texture.
    CHECK(h.rasterCount() == static_cast<size_t>(kFrames) + 1);
}

TEST_CASE("writing resolution re-rasterizes at the new size", "[bitmap-cache]")
{
    Harness h;
    const float w = h.artboard->width();
    const float ht = h.artboard->height();

    h.setResolution(1.0f);
    h.frame(0.0f);
    REQUIRE(h.rasterCount() == 1);
    auto atOne = h.lastRaster();
    CHECK(atOne.width == expectedDim(w, 1.0f));
    CHECK(atOne.height == expectedDim(ht, 1.0f));

    // A static frame in between proves the next re-raster is the property
    // write's doing and not just frame churn.
    h.frame(0.0f);
    REQUIRE(h.rasterCount() == 1);

    h.setResolution(2.0f);
    Census bumped = h.frame(0.0f);
    REQUIRE(h.rasterCount() == 2);
    CHECK(bumped.canvasOpens() == 1);
    auto atTwo = h.lastRaster();
    CHECK(atTwo.width == expectedDim(w, 2.0f));
    CHECK(atTwo.height == expectedDim(ht, 2.0f));
    CHECK(atTwo.pixels() > atOne.pixels());

    h.setResolution(0.5f);
    h.frame(0.0f);
    REQUIRE(h.rasterCount() == 3);
    auto atHalf = h.lastRaster();
    CHECK(atHalf.width == expectedDim(w, 0.5f));
    CHECK(atHalf.height == expectedDim(ht, 0.5f));
    CHECK(atHalf.pixels() < atOne.pixels());

    printf("[bitmap-cache] resolution 0.5 -> %ux%u, 1.0 -> %ux%u, 2.0 -> "
           "%ux%u\n",
           atHalf.width,
           atHalf.height,
           atOne.width,
           atOne.height,
           atTwo.width,
           atTwo.height);
}

TEST_CASE("the composite lands exactly where the vector content would",
          "[bitmap-cache]")
{
    // The cached raster is only correct if compositing it maps the texture
    // onto the same box the vector content occupies -- at every resolution,
    // including the ones the kMaxDim clamp distorts.
    const Mat2D outer = Mat2D::fromScaleAndTranslation(0.75f, 0.75f, 40, -12);

    for (float resolution : {0.1f, 0.5f, 1.0f, 2.0f, 8.0f})
    {
        Harness h;
        h.setResolution(resolution);
        h.frame(0.0f, &outer);
        REQUIRE(h.rasterCount() == 1);
        auto raster = h.lastRaster();

        Mat2D ctm;
        REQUIRE(compositeTransform(h.lastFrame, &ctm));

        // The image spans [0,0] .. [widthPx,heightPx] in its local space.
        Vec2D origin = ctm * Vec2D(0, 0);
        Vec2D corner = ctm * Vec2D(static_cast<float>(raster.width),
                                   static_cast<float>(raster.height));
        // Which must land on the artboard's own box under the same outer
        // transform the vector draw would have used.
        Vec2D wantOrigin = outer * Vec2D(0, 0);
        Vec2D wantCorner =
            outer * Vec2D(h.artboard->width(), h.artboard->height());

        // One texel of the raster, measured in the space the composite lands
        // in, which is the size of the ceil() overhang.
        const float texelX = texelSpan(origin, corner, raster.width);
        const float texelY =
            std::abs(corner.y - origin.y) / static_cast<float>(raster.height);

        // The origin only ever moves by the snap.
        CHECK(origin.x == Approx(wantOrigin.x).margin(kSnapSlack));
        CHECK(origin.y == Approx(wantOrigin.y).margin(kSnapSlack));
        // The far corner may additionally overhang by up to one texel, but it
        // must never fall short of the box by more than the snap: a raster that
        // undershoots is content clipped away.
        CHECK(corner.x >= wantCorner.x - kSnapSlack);
        CHECK(corner.y >= wantCorner.y - kSnapSlack);
        CHECK(corner.x <= wantCorner.x + kSnapSlack + texelX);
        CHECK(corner.y <= wantCorner.y + kSnapSlack + texelY);
    }
}

TEST_CASE("a lower resolution costs proportionally less raster work",
          "[bitmap-cache]")
{
    // Raster cost is pixels: the offscreen texture the GPU has to fill and
    // hold. Recording cost is resolution-independent by design (the same
    // vector content is recorded either way), so pixels is the metric that
    // moves.
    struct Sample
    {
        float resolution;
        uint64_t pixels;
        uint64_t vramBytes;
    };
    std::vector<Sample> samples;
    // Every step here stays under kMaxDim for this artboard, so the ratios are
    // the resolution's doing and not the clamp's.
    const std::vector<float> kSteps = {0.125f, 0.25f, 0.5f, 1.0f};

    for (float resolution : kSteps)
    {
        Harness h;
        REQUIRE(expectedDim(h.artboard->width(), resolution) < kMaxDim);
        REQUIRE(expectedDim(h.artboard->height(), resolution) < kMaxDim);
        h.setResolution(resolution);
        h.frame(0.0f);
        REQUIRE(h.rasterCount() == 1);
        auto raster = h.lastRaster();
        samples.push_back(
            {resolution, raster.pixels(), raster.pixels() * 4 /*RGBA8*/});
        printf("[bitmap-cache] resolution %.3f -> %ux%u = %llu px (%llu KiB)\n",
               resolution,
               raster.width,
               raster.height,
               (unsigned long long)raster.pixels(),
               (unsigned long long)(raster.pixels() * 4 / 1024));
    }

    // Monotonic, and each doubling of resolution is ~4x the pixels.
    for (size_t i = 1; i < samples.size(); i++)
    {
        CHECK(samples[i].pixels > samples[i - 1].pixels);
        CHECK(samples[i].vramBytes > samples[i - 1].vramBytes);
        double ratio = static_cast<double>(samples[i].pixels) /
                       static_cast<double>(samples[i - 1].pixels);
        CHECK(ratio == Approx(4.0).epsilon(0.05));
    }

    // The extremes bracket the feature's value: 1/8 resolution fills ~64x
    // fewer pixels than native.
    CHECK(samples.front().pixels * 60 <= samples.back().pixels);
}

TEST_CASE("a lower resolution costs less real GPU work", "[bitmap-cache]")
{
    // Same recording, but replayed for real against a null device so the
    // numbers come off the flush descriptors the GPU backend would consume:
    // tessellation spans (transform dependent) and render-target area.
    struct Sample
    {
        float resolution;
        GpuWork work;
    };
    std::vector<Sample> samples;

    // Baseline: the same artboard with the cache never engaging.
    GpuWork uncachedFrame;
    {
        Harness u(/*cached=*/false);
        ObservingContext ctx;
        ReplaySink sink(&ctx,
                        static_cast<uint32_t>(std::ceil(u.artboard->width())),
                        static_cast<uint32_t>(std::ceil(u.artboard->height())));
        u.sink = &sink;
        u.frame(0.0f);
        GpuWork first = ctx.observer()->work;
        u.frame(0.0f);
        uncachedFrame = ctx.observer()->work - first;
        REQUIRE(u.rasterCount() == 0);
        printf("[bitmap-cache] uncached frame: flushes=%llu paths=%llu "
               "tessSpans=%llu targetPx=%llu\n",
               (unsigned long long)uncachedFrame.flushes,
               (unsigned long long)uncachedFrame.pathCount,
               (unsigned long long)uncachedFrame.tessVertexSpans,
               (unsigned long long)uncachedFrame.targetPixels);
    }

    for (float resolution : {0.25f, 0.5f, 1.0f})
    {
        Harness h;
        ObservingContext ctx;
        ReplaySink sink(&ctx,
                        static_cast<uint32_t>(std::ceil(h.artboard->width())),
                        static_cast<uint32_t>(std::ceil(h.artboard->height())));
        h.sink = &sink;

        h.setResolution(resolution);
        h.frame(0.0f); // the rasterizing frame
        GpuWork afterRaster = ctx.observer()->work;
        REQUIRE(h.rasterCount() == 1);
        // A canvas flush plus a screen flush.
        REQUIRE(afterRaster.flushes >= 2);

        h.frame(0.0f); // the cached frame
        GpuWork cachedFrame = ctx.observer()->work - afterRaster;
        REQUIRE(h.rasterCount() == 1);

        printf("[bitmap-cache] res %.2f | raster frame: flushes=%llu "
               "paths=%llu tessSpans=%llu tessRows=%llu targetPx=%llu || "
               "cached frame: flushes=%llu paths=%llu tessSpans=%llu "
               "targetPx=%llu\n",
               resolution,
               (unsigned long long)afterRaster.flushes,
               (unsigned long long)afterRaster.pathCount,
               (unsigned long long)afterRaster.tessVertexSpans,
               (unsigned long long)afterRaster.tessDataHeight,
               (unsigned long long)afterRaster.targetPixels,
               (unsigned long long)cachedFrame.flushes,
               (unsigned long long)cachedFrame.pathCount,
               (unsigned long long)cachedFrame.tessVertexSpans,
               (unsigned long long)cachedFrame.targetPixels);

        // A cached frame opens no offscreen frame at all: one flush, one
        // screen target, and less vector work than either the rasterizing
        // frame or the uncached baseline.
        CHECK(cachedFrame.flushes == 1);
        CHECK(cachedFrame.flushes < afterRaster.flushes);
        CHECK(cachedFrame.tessVertexSpans < afterRaster.tessVertexSpans);
        CHECK(cachedFrame.tessVertexSpans < uncachedFrame.tessVertexSpans);
        CHECK(cachedFrame.targetPixels == uncachedFrame.targetPixels);

        // The other side of the trade: a frame that has to rasterize costs
        // more than not caching at all -- an extra flush (a render-target
        // switch) and the offscreen fill on top of the screen's.
        CHECK(afterRaster.flushes > uncachedFrame.flushes);
        CHECK(afterRaster.targetPixels > uncachedFrame.targetPixels);

        samples.push_back({resolution, afterRaster});
    }

    // Real GPU work on the rasterizing frame rises with resolution.
    for (size_t i = 1; i < samples.size(); i++)
    {
        CHECK(samples[i].work.targetPixels > samples[i - 1].work.targetPixels);
        CHECK(samples[i].work.tessDataHeight >=
              samples[i - 1].work.tessDataHeight);
    }

    // And the overhead a re-rasterizing frame carries over not caching shrinks
    // as resolution drops: that is the whole point of the knob.
    uint64_t lowOverhead =
        samples.front().work.targetPixels - uncachedFrame.targetPixels;
    uint64_t highOverhead =
        samples.back().work.targetPixels - uncachedFrame.targetPixels;
    CHECK(lowOverhead < highOverhead);
    printf("[bitmap-cache] re-raster fill overhead vs uncached: %.2fx at res "
           "%.2f, %.2fx at res %.2f\n",
           static_cast<double>(samples.front().work.targetPixels) /
               static_cast<double>(uncachedFrame.targetPixels),
           samples.front().resolution,
           static_cast<double>(samples.back().work.targetPixels) /
               static_cast<double>(uncachedFrame.targetPixels),
           samples.back().resolution);
}

TEST_CASE("resolution is clamped to a sane range", "[bitmap-cache]")
{
    Harness h;
    const float w = h.artboard->width();
    const float ht = h.artboard->height();

    // Below the floor clamps to kMinRes, not to zero-area.
    h.setResolution(0.0f);
    h.frame(0.0f);
    auto tiny = h.lastRaster();
    CHECK(tiny.width >= 1);
    CHECK(tiny.height >= 1);
    CHECK(tiny.width == expectedDim(w, kMinRes));
    CHECK(tiny.height == expectedDim(ht, kMinRes));

    // Negative is the same clamp, not a wrap into something huge.
    h.setResolution(-4.0f);
    h.frame(0.0f);
    CHECK(h.lastRaster().width == tiny.width);
    CHECK(h.lastRaster().height == tiny.height);

    // Above the ceiling clamps to kMaxRes, and no dimension exceeds kMaxDim.
    h.setResolution(64.0f);
    h.frame(0.0f);
    auto huge = h.lastRaster();
    CHECK(huge.width <= kMaxDim);
    CHECK(huge.height <= kMaxDim);
    CHECK(huge.width == expectedDim(w, kMaxRes));
    CHECK(huge.height == expectedDim(ht, kMaxRes));
    CHECK(huge.pixels() > tiny.pixels());

    // Non-finite values pass straight through std::max/std::min, so they are
    // handled rather than assumed away -- resolution animates and data binds,
    // so one can arrive from a bound property or an imported file.
    h.setResolution(std::numeric_limits<float>::infinity());
    Census infinite = h.frame(0.0f);
    CHECK(infinite.canvasOpens() == 1);
    CHECK(h.lastRaster().width == huge.width);
    CHECK(h.lastRaster().height == huge.height);

    // NaN has no size to rasterize at, so it takes the vector path instead of
    // running a NaN through ceil() and into a uint32_t pixel count.
    const size_t rastersBeforeNaN = h.rasterCount();
    h.setResolution(std::numeric_limits<float>::quiet_NaN());
    Census notANumber = h.frame(0.0f);
    CHECK(h.rasterCount() == rastersBeforeNaN);
    CHECK(notANumber.canvasOpens() == 0);
    CHECK(notANumber.drawImages() == 0);
    CHECK(notANumber.drawPaths() > 0);

    // Documented consequence of the per-axis clamp: once either axis saturates
    // at kMaxDim the raster is no longer proportional to the artboard, so the
    // two axes end up at different effective sample densities.
    float xDensity = static_cast<float>(huge.width) / w;
    float yDensity = static_cast<float>(huge.height) / ht;
    printf("[bitmap-cache] clamped raster %ux%u for a %gx%g artboard: "
           "x density %.2f, y density %.2f\n",
           huge.width,
           huge.height,
           w,
           ht,
           xDensity,
           yDensity);
}

TEST_CASE("the cache engages through the real nested-artboard path",
          "[bitmap-cache]")
{
    // The production shape: the file's default artboard nests ArtboardMain,
    // and a host draws the root with Artboard::draw(). The root itself is not
    // cached (draw() bypasses the hook by design) but the nested instance
    // reaches drawInternal, which is where the cache lives. Without this the
    // suite would only ever prove the hand-called drawInternal path.
    auto ctx = RenderContextNULL::MakeContext();
    CountingSession session(rive::ore::ReplayCaps{});
    session.bindRenderContext(ctx.get());
    auto file = ReadRiveFile(kRiv, &session);

    auto root = file->artboardDefault();
    REQUIRE(root != nullptr);
    // The root is a plain artboard; the caching one is the instance inside it.
    CHECK(root->bitmapCache() == nullptr);

    auto sm = root->stateMachineAt(0);
    REQUIRE(sm != nullptr);
    int vmId = root->viewModelId();
    auto vmi = vmId == -1 ? file->createViewModelInstance(root.get())
                          : file->createViewModelInstance(vmId, 0);
    if (vmi != nullptr)
    {
        sm->bindViewModelInstance(vmi);
    }

    auto drawFrame = [&](float dt) {
        sm->advanceAndApply(dt);
        Renderer* screen = session.screenRenderer();
        screen->save();
        root->draw(screen);
        screen->restore();
        snapshotFrame(session);
        session.resetFrame();
    };

    drawFrame(0.0f);
    // The nested instance rasterized itself offscreen.
    REQUIRE(session.rasters.size() == 1);

    // Static frames ride the cache: no re-raster from the root draw either.
    for (int i = 0; i < 4; i++)
    {
        drawFrame(0.0f);
    }
    CHECK(session.rasters.size() == 1);

    // Advancing time changes nothing in this fixture's nested artboard, so the
    // cache holds: the point of the feature is that a static subtree stops
    // paying for itself. Re-rasterization on change is covered separately, by
    // the resolution and size cases that invalidate deliberately -- this
    // fixture has no animation to drive it from here.
    size_t before = session.rasters.size();
    for (int i = 0; i < 4; i++)
    {
        drawFrame(1.0f / 60.0f);
    }
    printf("[bitmap-cache] nested root draw: %zu rasters over 5 static frames, "
           "%zu more over 4 advanced ones\n",
           before,
           session.rasters.size() - before);
    CHECK(session.rasters.size() == before);
}

TEST_CASE("a silver factory drives the cache only once enabled",
          "[bitmap-cache]")
{
    // The trap this guards: a [silver] test on a bare SerializingFactory
    // exercises the plain vector path no matter what the artboard asks for,
    // so a golden captured that way says nothing about cache-as-bitmap. Both
    // hooks are opt-in precisely so turning one on is a deliberate act and
    // existing goldens keep their meaning.
    SECTION("off by default")
    {
        SerializingFactory silver;
        CHECK(silver.renderContext() == nullptr);
        CHECK(silver.deferredCanvasHost() == nullptr);

        auto file = ReadRiveFile(kRiv, &silver);
        auto artboard = cachedArtboardOf(file.get());
        REQUIRE(artboard != nullptr);
        // The object still imports -- it just never rasterizes.
        REQUIRE(artboard->bitmapCache() != nullptr);

        auto renderer = silver.makeRenderer();
        silver.frameSize(artboard->width(), artboard->height());
        artboard->advance(0.0f);
        // Even the cache-aware entry point falls back to a vector draw here.
        artboard->drawInternal(renderer.get());
        CHECK(silver.bytes().size() > 0);
    }

    SECTION("enabled")
    {
        auto ctx = RenderContextNULL::MakeContext();
        SerializingFactory silver;
        silver.enableBitmapCache(ctx.get());
        CHECK(silver.renderContext() != nullptr);
        CHECK(silver.deferredCanvasHost() != nullptr);

        auto file = ReadRiveFile(kRiv, &silver);
        auto artboard = cachedArtboardOf(file.get());
        REQUIRE(artboard != nullptr);

        auto renderer = silver.makeRenderer();
        silver.frameSize(artboard->width(), artboard->height());
        artboard->advance(0.0f);
        size_t before = silver.bytes().size();
        artboard->drawInternal(renderer.get());
        // Now the draw records an offscreen bracket, so the same artboard
        // costs strictly more stream than the vector fallback did.
        CHECK(silver.bytes().size() > before);
    }
}

TEST_CASE("the raster covers the artboard's own bounds", "[bitmap-cache]")
{
    // renderIntoCanvas rasterizes local [0,0,w,h]. That is only the artboard's
    // box when its origin is the default: Artboard::bounds() is [0,0,w,h] with
    // frameOrigin on, but [-w*ox,-h*oy,w,h] with it off -- and NestedArtboard
    // turns frameOrigin off on everything it hosts, which is the only way to
    // reach the cache at all. So a hosted artboard with a non-zero origin is
    // the case to check.
    auto report = [](const char* label,
                     Harness& h,
                     CountingSession::Raster raster,
                     const Mat2D& ctm) {
        AABB want = h.artboard->bounds();
        Vec2D origin = ctm * Vec2D(0, 0);
        Vec2D corner = ctm * Vec2D(static_cast<float>(raster.width),
                                   static_cast<float>(raster.height));
        printf("[bitmap-cache] %s: bounds [%g %g %g %g], raster lands "
               "[%g %g %g %g]\n",
               label,
               want.left(),
               want.top(),
               want.right(),
               want.bottom(),
               origin.x,
               origin.y,
               corner.x,
               corner.y);
        return AABB(origin.x, origin.y, corner.x, corner.y);
    };

    SECTION("default origin")
    {
        Harness h;
        h.setResolution(1.0f);
        h.artboard->frameOrigin(false); // what a NestedArtboard host sets
        h.frame(0.0f);
        REQUIRE(h.rasterCount() == 1);
        Mat2D ctm;
        REQUIRE(compositeTransform(h.lastFrame, &ctm));
        AABB got = report("origin 0,0", h, h.lastRaster(), ctm);
        AABB want = h.artboard->bounds();
        const float slack =
            kSnapSlack + got.width() / static_cast<float>(h.lastRaster().width);
        CHECK(got.left() == Approx(want.left()).margin(slack));
        CHECK(got.top() == Approx(want.top()).margin(slack));
        CHECK(got.right() == Approx(want.right()).margin(slack));
        CHECK(got.bottom() == Approx(want.bottom()).margin(slack));
    }

    SECTION("centered origin, frameOrigin off (the hosted case)")
    {
        Harness h;
        h.setResolution(1.0f);
        h.artboard->frameOrigin(false);
        h.artboard->originX(0.5f);
        h.artboard->originY(0.5f);
        h.frame(0.0f);
        REQUIRE(h.rasterCount() == 1);
        Mat2D ctm;
        REQUIRE(compositeTransform(h.lastFrame, &ctm));
        AABB got =
            report("origin 0.5,0.5 frameOrigin off", h, h.lastRaster(), ctm);
        AABB want = h.artboard->bounds();
        const float slack =
            kSnapSlack + got.width() / static_cast<float>(h.lastRaster().width);
        CHECK(got.left() == Approx(want.left()).margin(slack));
        CHECK(got.top() == Approx(want.top()).margin(slack));
        CHECK(got.right() == Approx(want.right()).margin(slack));
        CHECK(got.bottom() == Approx(want.bottom()).margin(slack));
    }

    SECTION("centered origin, frameOrigin on")
    {
        Harness h;
        h.setResolution(1.0f);
        h.artboard->originX(0.5f);
        h.artboard->originY(0.5f);
        h.frame(0.0f);
        REQUIRE(h.rasterCount() == 1);
        Mat2D ctm;
        REQUIRE(compositeTransform(h.lastFrame, &ctm));
        AABB got =
            report("origin 0.5,0.5 frameOrigin on", h, h.lastRaster(), ctm);
        AABB want = h.artboard->bounds();
        const float slack =
            kSnapSlack + got.width() / static_cast<float>(h.lastRaster().width);
        CHECK(got.left() == Approx(want.left()).margin(slack));
        CHECK(got.top() == Approx(want.top()).margin(slack));
        CHECK(got.right() == Approx(want.right()).margin(slack));
        CHECK(got.bottom() == Approx(want.bottom()).margin(slack));
    }
}

TEST_CASE("the recording holds vector commands, never pixels", "[bitmap-cache]")
{
    // What the record/replay pipeline can and cannot show. The offscreen
    // bracket carries the artboard's draw calls; the pixel size appears only as
    // the canvas's declared dimensions. Nothing here rasterizes -- not the
    // deferred session, not the serializing factory, not a .sriv -- so a golden
    // or the viewer can prove the cache path was taken and the target was sized
    // right, but can never show the resolution's blur. That needs a real
    // device, i.e. a GM.
    size_t bytesLow = 0, bytesHigh = 0;
    for (float resolution : {0.25f, 1.0f})
    {
        Harness h;
        h.setResolution(resolution);
        h.frame(0.0f); // identical animation state both times
        REQUIRE(h.rasterCount() == 1);
        (resolution < 1.0f ? bytesLow : bytesHigh) =
            h.session.rasterContentBytes.front();
        printf("[bitmap-cache] res %.2f -> %ux%u raster, %zu recorded content "
               "bytes\n",
               resolution,
               h.lastRaster().width,
               h.lastRaster().height,
               h.session.rasterContentBytes.front());
    }

    // 25x the pixels, byte for byte the same recording. Pixels would have
    // scaled with the area; vector commands do not.
    CHECK(bytesLow == bytesHigh);
    CHECK(bytesLow > 0);
}

TEST_CASE("what in the offscreen raster scales with resolution",
          "[bitmap-cache]")
{
    // Lowering the resolution does not lower the cost of a cached frame
    // proportionally: there is a floor. This names it. Per-pixel work (the
    // render-target area the flush has to fill) tracks the resolution, while
    // per-path work (the number of paths tessellated) does not move at all --
    // the same vector content is recorded either way. So the floor is the
    // per-path half, and dropping resolution can never buy it back.
    struct Sample
    {
        float resolution;
        GpuWork work;
    };
    std::vector<Sample> samples;
    printf("[bitmap-cache] %-6s %12s %8s %10s %10s %12s  %s\n",
           "res",
           "targetPx",
           "paths",
           "tessSpans",
           "tessRows",
           "atlasArea",
           "shaderFeatures");
    for (float resolution : {0.125f, 0.25f, 0.5f, 1.0f})
    {
        Harness h;
        ObservingContext ctx;
        ReplaySink sink(&ctx,
                        static_cast<uint32_t>(std::ceil(h.artboard->width())),
                        static_cast<uint32_t>(std::ceil(h.artboard->height())));
        h.sink = &sink;
        h.setResolution(resolution);
        Node* node = firstNodeOf(h.artboard.get());
        REQUIRE(node != nullptr);
        // Settle first. The nested artboards ArtboardMain hosts -- which is
        // where the feathered content lives -- are not on screen at t=0, so
        // measuring the opening frame would report on an artboard that is
        // mostly empty. Every frame replays: the session drops its stream each
        // time, so skipping the replay would strand the resources the import
        // recorded and leave the measured frame missing its paths.
        constexpr int kSettleFrames = 60;
        for (int i = 0; i < kSettleFrames; i++)
        {
            h.frame(1.0f / 60);
        }
        // Measure a frame that actually rasterizes. A settled frame does not:
        // it composites the existing texture, so every resolution would report
        // the same screen-sized fill and the question would go unanswered.
        const size_t rastersBefore = h.rasterCount();
        GpuWork before = ctx.observer()->work;
        node->x(node->x() + 1.0f);
        h.frame(1.0f / 60);
        REQUIRE(h.rasterCount() == rastersBefore + 1);
        GpuWork w = ctx.observer()->work - before;
        static const char* kFeatures[] = {"CLIPPING",
                                          "CLIP_RECT",
                                          "ADVANCED_BLEND",
                                          "FEATHER",
                                          "EVEN_ODD",
                                          "NESTED_CLIPPING",
                                          "HSL_BLEND_MODES",
                                          "DITHER"};
        std::string features;
        for (size_t i = 0; i < 8; i++)
        {
            if (ctx.observer()->featuresEver & (1u << i))
            {
                features +=
                    std::string(features.empty() ? "" : "|") + kFeatures[i];
            }
        }
        printf("[bitmap-cache] %-6.2f %12llu %8llu %10llu %10llu %12llu  %s\n",
               resolution,
               (unsigned long long)w.targetPixels,
               (unsigned long long)w.pathCount,
               (unsigned long long)w.tessVertexSpans,
               (unsigned long long)w.tessDataHeight,
               (unsigned long long)w.atlasArea,
               features.empty() ? "(none)" : features.c_str());
        samples.push_back({resolution, w});
    }

    REQUIRE(samples.size() == 4);
    for (const auto& sample : samples)
    {
        // A settled frame that tessellates nothing would make every ratio
        // below vacuous.
        REQUIRE(sample.work.pathCount > 0);
        REQUIRE(sample.work.targetPixels > 0);
    }

    // Per-pixel, and quadratically so. Every step up the ladder is a doubling
    // of resolution, and each one adds roughly 4x the area the previous step
    // added. (The totals themselves cannot be compared as ratios: they include
    // the screen's own fill, which is the same on every row.)
    for (size_t i = 1; i < samples.size(); i++)
    {
        CHECK(samples[i].work.targetPixels > samples[i - 1].work.targetPixels);
    }
    for (size_t i = 2; i < samples.size(); i++)
    {
        const uint64_t previousStep =
            samples[i - 1].work.targetPixels - samples[i - 2].work.targetPixels;
        const uint64_t step =
            samples[i].work.targetPixels - samples[i - 1].work.targetPixels;
        CHECK(step > previousStep * 3);
    }

    // Per-path: the floor. The same vector content is recorded and tessellated
    // at every resolution, so neither of these moves -- which is why dropping
    // resolution cannot buy the per-path half back, however low it goes.
    for (const auto& sample : samples)
    {
        CHECK(sample.work.pathCount == samples.front().work.pathCount);
        CHECK(sample.work.tessVertexSpans ==
              samples.front().work.tessVertexSpans);
    }
}

// ---- device-scale sizing (the crispness fix) ----
//
// The original policy sized the raster in artboard units and reused it at every
// zoom, so `resolution` only meant "native pixels" when the artboard happened
// to land at one device pixel per artboard unit. On a 2x display, or at any
// zoom but 100%, a resolution-1 cache was a lower-resolution image being
// magnified -- which is what softened text. These cases pin the new contract:
// the raster is sized from the scale the artboard is actually landing at, so
// resolution 1 composites 1:1.

namespace
{
// One cached frame drawn under `outer`, reporting both the raster it allocated
// and the transform the composite ended up with.
struct Composite
{
    CountingSession::Raster raster;
    Mat2D ctm;
    float artboardWidth;
    float artboardHeight;
};

Composite compositeUnder(const Mat2D& outer)
{
    Harness h;
    h.frame(0.0f, &outer);
    REQUIRE(h.rasterCount() == 1);
    Composite c;
    c.raster = h.lastRaster();
    REQUIRE(compositeTransform(h.lastFrame, &c.ctm));
    c.artboardWidth = h.artboard->width();
    c.artboardHeight = h.artboard->height();
    return c;
}
} // namespace

namespace
{
// Two device scales an octave apart that both stay inside the kMaxDim clamp for
// whatever artboard the fixture carries. A clamped axis would make every ratio
// below the clamp's doing rather than the policy's, and this fixture's artboard
// is big enough that a hardcoded 2x would clamp. Both come out as whole
// sixteenths so the sizing quantization is a no-op and expectedDim still
// describes the answer exactly.
struct ScalePair
{
    float lo;
    float hi;
};

ScalePair scalesInsideClamp(const Composite& probe)
{
    const float ceilingScale =
        std::min(static_cast<float>(kMaxDim) / probe.artboardWidth,
                 static_cast<float>(kMaxDim) / probe.artboardHeight);
    // Halve the ceiling, floor it to an *even* number of sixteenths so that
    // halving again stays on the grid.
    int sixteenths = static_cast<int>(std::floor(ceilingScale * 8.0f));
    sixteenths -= sixteenths % 2;
    const float hi = static_cast<float>(sixteenths) / 16.0f;
    return {hi * 0.5f, hi};
}
} // namespace

TEST_CASE("the raster is sized from the device scale the artboard lands at",
          "[bitmap-cache]")
{
    const Composite probe = compositeUnder(Mat2D());
    const ScalePair s = scalesInsideClamp(probe);
    REQUIRE(s.lo >= 0.125f);

    auto lo = compositeUnder(Mat2D::fromScale(s.lo, s.lo));
    auto hi = compositeUnder(Mat2D::fromScale(s.hi, s.hi));

    CHECK(lo.raster.width == expectedDim(probe.artboardWidth, s.lo));
    CHECK(lo.raster.height == expectedDim(probe.artboardHeight, s.lo));
    CHECK(hi.raster.width == expectedDim(probe.artboardWidth, s.hi));
    CHECK(hi.raster.height == expectedDim(probe.artboardHeight, s.hi));
    // Doubling the device scale is 4x the texels, the same way doubling
    // `resolution` is.
    CHECK(hi.raster.pixels() > lo.raster.pixels() * 3);
}

TEST_CASE("resolution 1 composites one texel per device pixel",
          "[bitmap-cache]")
{
    // The crispness claim itself. A composite scale of 1 means the bilinear
    // sample is an identity blit; anything above 1 is magnification, which is
    // exactly the softness that sizing in artboard units used to produce.
    const Composite probe = compositeUnder(Mat2D());
    const ScalePair s = scalesInsideClamp(probe);
    REQUIRE(s.lo >= 0.125f);

    for (float scale : {s.lo, s.hi})
    {
        auto now = compositeUnder(Mat2D::fromScale(scale, scale));
        CHECK(now.ctm.findMaxScale() == Approx(1.0f).margin(0.001));
    }
}

TEST_CASE(
    "the composite still lands on the artboard's box under device scaling",
    "[bitmap-cache]")
{
    // Sizing changed; placement must not. The image spans
    // [0,0,widthPx,heightPx] in local space, and its origin has to sit on the
    // artboard's origin. The far corner is now allowed to overhang by up to the
    // sub-texel that ceil() rounded up -- that sliver is the price of an
    // exact-ratio mapping, and it must stay under one device pixel.
    for (float scale : {0.75f, 1.0f, 2.0f})
    {
        const Mat2D outer =
            Mat2D::fromScaleAndTranslation(scale, scale, 40, -12);
        auto c = compositeUnder(outer);

        Vec2D origin = c.ctm * Vec2D(0, 0);
        Vec2D corner = c.ctm * Vec2D(static_cast<float>(c.raster.width),
                                     static_cast<float>(c.raster.height));
        Vec2D wantOrigin = outer * Vec2D(0, 0);
        Vec2D wantCorner = outer * Vec2D(c.artboardWidth, c.artboardHeight);

        CHECK(origin.x == Approx(wantOrigin.x).margin(0.01));
        CHECK(origin.y == Approx(wantOrigin.y).margin(0.01));
        CHECK(corner.x >= wantCorner.x - 0.01f);
        CHECK(corner.y >= wantCorner.y - 0.01f);
        CHECK(corner.x - wantCorner.x < 1.0f);
        CHECK(corner.y - wantCorner.y < 1.0f);
    }
}

TEST_CASE("pixel snapping puts the composited raster on the pixel grid",
          "[bitmap-cache]")
{
    // A half-pixel origin makes every bilinear tap a blend of two texels: a box
    // blur over the whole image even when the scale is a perfect 1:1. Snapping
    // costs at most half a pixel of position and buys back the sharpness.
    // Both offsets are deliberately off-grid: unsnapped this would composite at
    // (10.25, 7.4) device pixels, a sub-pixel phase on both axes.
    const Mat2D outer =
        Mat2D::fromScaleAndTranslation(2.0f, 2.0f, 10.25f, 7.4f);
    const Vec2D unsnapped = outer * Vec2D(0, 0);
    REQUIRE(std::abs(unsnapped.x - std::round(unsnapped.x)) > 0.01f);
    REQUIRE(std::abs(unsnapped.y - std::round(unsnapped.y)) > 0.01f);

    auto c = compositeUnder(outer);
    Vec2D origin = c.ctm * Vec2D(0, 0);
    CHECK(origin.x == Approx(std::round(origin.x)).margin(0.001));
    CHECK(origin.y == Approx(std::round(origin.y)).margin(0.001));

    // Snapping moves the image by less than a pixel and changes nothing else.
    CHECK(std::abs(origin.x - unsnapped.x) <= 0.5f + 0.001f);
    CHECK(std::abs(origin.y - unsnapped.y) <= 0.5f + 0.001f);
}

TEST_CASE("kMaxDim caps the device scale a large artboard can be cached at",
          "[bitmap-cache]")
{
    // Device-scale sizing only buys crispness up to the point the 2048 cap
    // takes over, and for a full-size artboard that point arrives almost
    // immediately: 1920 tall leaves headroom for barely 1.07x. So on a 2x
    // display this artboard still composites magnified -- the cap, not the
    // sizing policy, is what is left limiting it. Pinned here so raising
    // kMaxDim (or replacing the per-axis cap with a pixel budget) shows up as
    // a change in this number rather than a silent quality shift.
    const Composite probe = compositeUnder(Mat2D());
    // Above this artboard's own ceiling. A 1080x1920 artboard caps at ~1.07x,
    // so a retina display alone is enough to clamp it; this fixture is small
    // (225x225) and does not clamp until ~9x, hence the deliberately large
    // scale. The behavior under the cap is what is being pinned, not the
    // particular scale that reaches it.
    const float retina = 16.0f;
    auto c = compositeUnder(Mat2D::fromScale(retina, retina));

    const float ceilingScale =
        std::min(static_cast<float>(kMaxDim) / probe.artboardWidth,
                 static_cast<float>(kMaxDim) / probe.artboardHeight);
    REQUIRE(ceilingScale < retina); // otherwise this artboard is not capped

    // The raster tops out on its long axis...
    CHECK(std::max(c.raster.width, c.raster.height) == kMaxDim);
    // ...and the composite is left magnifying by the shortfall, instead of the
    // 1:1 an uncapped artboard gets.
    const float magnification = c.ctm.findMaxScale();
    CHECK(magnification > 1.0f);
    CHECK(magnification == Approx(retina / ceilingScale).epsilon(0.02));

    printf("[bitmap-cache] %.0fx%.0f artboard at %.1fx device scale: capped at "
           "%ux%u, still magnifying %.3fx (uncapped would be 1.000x)\n",
           probe.artboardWidth,
           probe.artboardHeight,
           retina,
           c.raster.width,
           c.raster.height,
           magnification);
}
