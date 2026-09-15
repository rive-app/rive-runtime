/*
 * Copyright 2026 Rive
 */

// Pixel-level validation of cache-as-bitmap.
//
// The feature's whole contract is "the composited raster looks like the vector
// draw it replaced". Nothing else in the suite can check that: the other
// [bitmap-cache] cases assert structure (one drawImage instead of N drawPaths,
// raster dimensions, re-raster cadence) and the [silver] goldens compare
// serialized draw *commands*, so blur, tessellation coarseness, AA quality and
// sub-pixel phase are all invisible to them.
//
// So this renders the same artboard twice against a real GPU context -- once
// cached, once not -- and compares the two framebuffers. The uncached render is
// the golden, which is why there is no stored PNG here: nothing to rebaseline,
// no per-backend drift, and no way to accidentally bless a blurry baseline.
//
// Cache-as-bitmap only engages on the deferred/recording path (it needs a
// DeferredCanvasHost, which only cmd::DeferredSession supplies), so the frame
// is recorded into a session and replayed through TestingWindowFrameSink
// against the window's context.

#include "common/testing_window.hpp"

#ifdef RIVE_CANVAS

#include "common/testing_window_deferred_sink.hpp"
#include "rive/artboard.hpp"
#include "rive/bitmap_cache.hpp"
#include "rive/file.hpp"
#include "rive/math/mat2d.hpp"
#include "rive/renderer/cmd/deferred_replayer.hpp"
#include "rive/renderer/cmd/deferred_session.hpp"
#include "rive_file_reader.hpp"

#include <catch.hpp>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

using namespace rive;

namespace
{
constexpr const char* kRiv = "assets/cache_as_bitmap_test.riv";

// Difference between two RGBA8 buffers of the same size.
struct Diff
{
    uint32_t maxChannel = 0;
    double meanChannel = 0;
    // Fraction of pixels where some channel moved by more than kChannelEpsilon.
    // Modeled on rive_native's golden comparator, which uses the same idea to
    // absorb dithering: a handful of edge pixels a shade off is not a
    // regression, a broad haze over the whole image is.
    double significantFraction = 0;

    static constexpr uint32_t kChannelEpsilon = 2;

    static Diff between(const std::vector<uint8_t>& a,
                        const std::vector<uint8_t>& b)
    {
        REQUIRE(a.size() == b.size());
        REQUIRE(a.size() % 4 == 0);
        Diff d;
        uint64_t total = 0;
        size_t significant = 0;
        const size_t pixels = a.size() / 4;
        for (size_t p = 0; p < pixels; p++)
        {
            uint32_t worst = 0;
            for (size_t c = 0; c < 4; c++)
            {
                uint32_t delta = static_cast<uint32_t>(
                    std::abs(static_cast<int>(a[p * 4 + c]) -
                             static_cast<int>(b[p * 4 + c])));
                total += delta;
                worst = std::max(worst, delta);
            }
            d.maxChannel = std::max(d.maxChannel, worst);
            if (worst > kChannelEpsilon)
            {
                significant++;
            }
        }
        d.meanChannel = static_cast<double>(total) / (pixels * 4);
        d.significantFraction = static_cast<double>(significant) / pixels;
        return d;
    }

    void report(const char* what) const
    {
        printf("[bitmap-cache] %s: max channel %u, mean %.4f, %.4f%% of pixels "
               "off by more than %u\n",
               what,
               maxChannel,
               meanChannel,
               significantFraction * 100.0,
               kChannelEpsilon);
    }
};

// Gradient energy: the summed *squared* luma step between neighboring pixels.
// Squared on purpose. The sum of absolute steps is total variation, which is
// blur invariant across a monotone edge -- climbing 0..255 over one pixel or
// three gives the same total, and measurement confirmed it (ratio 1.02 with
// snapping disabled, i.e. no signal). Squaring makes spreading the same climb
// over more pixels strictly cheaper, which is what blur does.
//
// Unlike the Diff above this judges one image on its own, which is what a test
// of *sharpness* rather than agreement needs.
double gradientEnergy(const std::vector<uint8_t>& px, uint32_t w, uint32_t h)
{
    auto luma = [&](uint32_t x, uint32_t y) {
        size_t i = (static_cast<size_t>(y) * w + x) * 4;
        return 0.299 * px[i] + 0.587 * px[i + 1] + 0.114 * px[i + 2];
    };
    double energy = 0;
    for (uint32_t y = 0; y + 1 < h; y++)
    {
        for (uint32_t x = 0; x + 1 < w; x++)
        {
            const double dx = luma(x + 1, y) - luma(x, y);
            const double dy = luma(x, y + 1) - luma(x, y);
            energy += dx * dx + dy * dy;
        }
    }
    return energy;
}

// How much of the buffer is not the clear color. A cached and an uncached
// render of *nothing* match perfectly, so without this guard a fixture that
// failed to load would pass every comparison below.
size_t nonBackgroundPixels(const std::vector<uint8_t>& px)
{
    size_t count = 0;
    for (size_t p = 0; p + 3 < px.size(); p += 4)
    {
        // The frame is cleared to opaque black.
        if (px[p] != 0 || px[p + 1] != 0 || px[p + 2] != 0)
        {
            count++;
        }
    }
    return count;
}

// The artboard carrying the BitmapCache, found rather than named so the test
// does not encode which artboard the fixture happens to put it on.
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

// Renders one frame of the fixture and returns the RGBA readback.
//
// deviceScale stands in for what the editor's stage applies: viewport zoom
// times window density. It is the whole point of the exercise -- the raster has
// to be sized from it, not from the artboard's own units.
std::vector<uint8_t> renderOnce(TestingWindow* window,
                                bool cacheEnabled,
                                float resolution,
                                float deviceScale,
                                Vec2D translate = {0, 0})
{
    auto* rc = window->renderContext();
    REQUIRE(rc != nullptr);

    cmd::DeferredSession session(rive::ore::ReplayCaps{});
    session.bindRenderContext(rc);

    auto file = ReadRiveFile(kRiv, &session);
    auto artboard = cachedArtboardOf(file.get());
    REQUIRE(artboard != nullptr);

    BitmapCache* cache = artboard->bitmapCache();
    // Driven straight on the core object: the fixture exposes no `resolution`
    // view-model property, and this path does not need one.
    cache->cacheEnabled(cacheEnabled);
    cache->resolution(resolution);

    const int w =
        static_cast<int>(std::ceil(artboard->width() * deviceScale)) + 8;
    const int h =
        static_cast<int>(std::ceil(artboard->height() * deviceScale)) + 8;
    window->resize(w, h);

    auto windowRenderer =
        window->beginFrame({.name = "bitmap_cache", .clearColor = 0xff000000});
    auto mainDesc = rc->frameDescriptor();

    artboard->advance(0.0f);
    Renderer* screen = session.screenRenderer();
    screen->save();
    screen->translate(translate.x, translate.y);
    screen->transform(Mat2D::fromScale(deviceScale, deviceScale));
    // drawInternal, not draw: draw() is the standalone-root path and
    // deliberately bypasses the cache.
    artboard->drawInternal(screen);
    screen->restore();

    cmd::DeferredFrame frame = cmd::snapshotFrame(session);
    session.resetFrame();
    rive_tests::TestingWindowFrameSink sink(window, rc, mainDesc);
    cmd::DeferredReplayer replayer;
    replayer.replayFrame(frame, sink);
    if (cacheEnabled)
    {
        // If the cache never opened an offscreen frame it silently fell back to
        // a vector draw, and every comparison below would be tautological.
        CHECK(sink.canvasFrames() == 1);
    }

    std::vector<uint8_t> pixels;
    window->endFrame(&pixels);
    return pixels;
}

// Deliberately not installed via TestingWindow::Set(): the window is owned by
// the test case and dies with it, so publishing it to the global would leave a
// dangling pointer for whatever runs next (it segfaulted ore_buffer_race_test
// on the way out). Everything here takes the window explicitly instead --
// including the frame sink, which is why it was parameterized on the way out of
// the GM.
std::unique_ptr<TestingWindow> makeWindow()
{
#if defined(__APPLE__)
    return std::unique_ptr<TestingWindow>(TestingWindow::MakeMetalTexture({}));
#else
    return nullptr;
#endif
}
} // namespace

TEST_CASE("a cached artboard composites the pixels the vector draw would",
          "[bitmap-cache]")
{
    auto window = makeWindow();
    if (window == nullptr || window->renderContext() == nullptr)
    {
        WARN("no GPU render context available, skipping");
        return;
    }

    // At a device scale of 2 -- a retina display at 100% zoom, or a 1x display
    // at 200% -- the old artboard-unit sizing rasterized at half the resolution
    // the screen was showing and magnified the result. That is the reported
    // blur, and it is what this case fails on if the sizing regresses.
    for (float deviceScale : {1.0f, 2.0f})
    {
        auto uncached = renderOnce(window.get(), false, 1.0f, deviceScale);
        auto cached = renderOnce(window.get(), true, 1.0f, deviceScale);

        // Guard against comparing two blank frames.
        CHECK(nonBackgroundPixels(uncached) > uncached.size() / 4 / 100);

        Diff d = Diff::between(uncached, cached);
        char label[64];
        snprintf(label, sizeof(label), "resolution 1 at %.1fx", deviceScale);
        d.report(label);
        // Measured on Metal this is bit exact at both scales -- an aligned 1:1
        // blit of an RGBA8 intermediate round trips -- but the threshold keeps
        // rive_native's golden-comparator margin (0.005) so a backend that
        // rounds premultiplied alpha differently is not a failure. A raster
        // sized in artboard units instead of device pixels lands an order of
        // magnitude above this: halving the resolution costs ~5%, and that is
        // exactly what the 2x case degrades into if the sizing regresses.
        CHECK(d.significantFraction < 0.005);
        CHECK(d.maxChannel < 32);
    }
}

TEST_CASE("a coarser cache resolution degrades the match", "[bitmap-cache]")
{
    // Proves the comparison above is actually measuring sharpness rather than
    // passing for some incidental reason: as the raster gets coarser the
    // difference from the vector draw has to grow.
    auto window = makeWindow();
    if (window == nullptr || window->renderContext() == nullptr)
    {
        WARN("no GPU render context available, skipping");
        return;
    }

    constexpr float kDeviceScale = 2.0f;
    auto uncached = renderOnce(window.get(), false, 1.0f, kDeviceScale);
    CHECK(nonBackgroundPixels(uncached) > uncached.size() / 4 / 100);

    double previous = -1.0;
    for (float resolution : {1.0f, 0.5f, 0.25f})
    {
        auto cached = renderOnce(window.get(), true, resolution, kDeviceScale);
        Diff d = Diff::between(uncached, cached);
        char label[64];
        snprintf(label, sizeof(label), "resolution %.2f at 2x", resolution);
        d.report(label);
        CHECK(d.significantFraction > previous);
        previous = d.significantFraction;
    }
}

TEST_CASE("a fractionally placed cache stays as sharp as an aligned one",
          "[bitmap-cache]")
{
    // A half-pixel origin makes every bilinear tap a blend of two texels, which
    // is a box blur over the whole image even at a perfect 1:1 scale. Snapping
    // the composite to the pixel grid is what prevents that.
    //
    // This cannot be measured against the vector draw: snapping deliberately
    // moves the raster by up to half a device pixel, so comparing positions
    // *penalizes* it (measured: 2.48% differing with snapping, 2.32% without --
    // the wrong direction). Sharpness has to be judged on the cached render
    // alone, so compare its gradient energy at a fractional offset against the
    // same render at an integer one. Snapped, the two are equally crisp;
    // unsnapped, the fractional one measurably smears.
    auto window = makeWindow();
    if (window == nullptr || window->renderContext() == nullptr)
    {
        WARN("no GPU render context available, skipping");
        return;
    }

    constexpr float kDeviceScale = 2.0f;
    auto aligned = renderOnce(window.get(), true, 1.0f, kDeviceScale, {2, 4});
    const uint32_t w = window->width(), h = window->height();
    auto fractional =
        renderOnce(window.get(), true, 1.0f, kDeviceScale, {2.25f, 4.4f});
    REQUIRE(window->width() == w);
    REQUIRE(window->height() == h);
    CHECK(nonBackgroundPixels(aligned) > aligned.size() / 4 / 100);

    const double alignedEnergy = gradientEnergy(aligned, w, h);
    const double fractionalEnergy = gradientEnergy(fractional, w, h);
    const double ratio = fractionalEnergy / alignedEnergy;
    printf("[bitmap-cache] gradient energy aligned %.0f, fractional %.0f "
           "(ratio %.4f)\n",
           alignedEnergy,
           fractionalEnergy,
           ratio);

    // Snapping puts both on the grid, so the fractional render keeps
    // essentially all of the aligned one's contrast. Without it, the sub-pixel
    // phase blurs the whole image and this ratio drops well below the bound.
    CHECK(ratio > 0.95);
}

#endif // RIVE_CANVAS
