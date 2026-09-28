/*
 * Copyright 2026 Rive
 *
 * The sizing math behind every offscreen raster (cache-as-bitmap, layer masks).
 * All of it is pure arithmetic on a RasterPlan, so none of this needs a
 * Factory, a Renderer or a GPU -- which is the point of having it in its own
 * TU. The renderer-dependent half, planRasterScale, is exercised through the
 * callers.
 */

#include <rive/offscreen_raster.hpp>

#ifdef RIVE_CANVAS

#include <catch.hpp>
#include <cmath>

using namespace rive;
using namespace rive::offscreen;

namespace
{
// planRasterScale's job, without a Renderer.
RasterPlan planAtScale(float rasterScale)
{
    RasterPlan plan;
    plan.rasterScale = rasterScale;
    return plan;
}

bool isWholeTexel(float localCoord, float rasterScale)
{
    const float texels = localCoord * rasterScale;
    return std::abs(texels - std::round(texels)) < 1e-3f;
}
} // namespace

TEST_CASE("exact fit keeps the historical sizing", "[offscreen-raster]")
{
    // What every caller got before stableGrid existed: ceil(box * scale), and
    // the box handed straight back untouched.
    RasterPlan plan = planAtScale(2.0f);
    REQUIRE(fitRasterToBox(AABB(0, 0, 100, 50), 0, RasterFit::exact, &plan));
    CHECK(plan.widthPx == 200);
    CHECK(plan.heightPx == 100);
    CHECK(plan.box == AABB(0, 0, 100, 50));
    CHECK(plan.rasterScale == 2.0f);

    // A fractional box rounds the pixels up but does NOT adjust the box: the
    // composite inverts the exact scale and lets the quad overhang instead.
    RasterPlan frac = planAtScale(1.0f);
    REQUIRE(fitRasterToBox(AABB(0, 0, 100.5f, 50), 0, RasterFit::exact, &frac));
    CHECK(frac.widthPx == 101);
    CHECK(frac.box.right() == 100.5f);
}

TEST_CASE("a degenerate box is refused", "[offscreen-raster]")
{
    RasterPlan plan = planAtScale(1.0f);
    CHECK_FALSE(fitRasterToBox(AABB(0, 0, 0, 50), 0, RasterFit::exact, &plan));
    CHECK_FALSE(fitRasterToBox(AABB(0, 0, 50, 0), 0, RasterFit::exact, &plan));
    // Inverted, which is what an empty intersection produces.
    CHECK_FALSE(
        fitRasterToBox(AABB(60, 0, 50, 50), 0, RasterFit::exact, &plan));
}

TEST_CASE("kMaxDim coarsens the scale uniformly", "[offscreen-raster]")
{
    // Both axes must keep one consistent mapping, so the cap lowers the scale
    // rather than clamping the long axis: 2048/4000 == 0.512 applies to both.
    RasterPlan plan = planAtScale(2.0f);
    REQUIRE(fitRasterToBox(AABB(0, 0, 4000, 1000), 0, RasterFit::exact, &plan));
    CHECK(plan.rasterScale == Approx(0.512f));
    CHECK(plan.widthPx == 2048);
    CHECK(plan.heightPx == 512);

    // This is the quality bug tight bounds exists to fix: a raster sized from a
    // big artboard cannot reach the scale the screen asked for.
    CHECK(plan.rasterScale < 2.0f);
}

TEST_CASE("the bucket ladder is multiples of 64", "[offscreen-raster]")
{
    CHECK(bucketedRasterDim(1) == 64);
    CHECK(bucketedRasterDim(64) == 64);
    CHECK(bucketedRasterDim(65) == 128);
    CHECK(bucketedRasterDim(200) == 256);
    CHECK(bucketedRasterDim(256) == 256);
    CHECK(bucketedRasterDim(257) == 320);
    CHECK(bucketedRasterDim(2047) == 2048);
    CHECK(bucketedRasterDim(2048) == kMaxDim);
    // Never past the cap, however large the request.
    CHECK(bucketedRasterDim(5000) == kMaxDim);

    // Slack is bounded by the quantum, not by a fraction of the request.
    for (uint32_t n = 1; n <= 2048; ++n)
    {
        const uint32_t b = bucketedRasterDim(n);
        CHECK(b >= n);
        CHECK(b - n < 64);
    }
}

TEST_CASE("stableGrid maps the pixel span exactly onto the box",
          "[offscreen-raster]")
{
    // The invariant the composite depends on: box.width() * rasterScale is
    // exactly widthPx, so 1/rasterScale inverts with nothing left over.
    for (float left : {0.0f, 10.3f, -7.9f, 123.456f})
    {
        for (float scale : {1.0f, 2.0f, 1.5f, 3.0f})
        {
            RasterPlan plan = planAtScale(scale);
            const AABB tight(left, left, left + 100.0f, left + 50.0f);
            REQUIRE(fitRasterToBox(tight, 2, RasterFit::stableGrid, &plan));
            CHECK(plan.box.width() * plan.rasterScale ==
                  Approx(static_cast<float>(plan.widthPx)));
            CHECK(plan.box.height() * plan.rasterScale ==
                  Approx(static_cast<float>(plan.heightPx)));
            // And it must still cover the box it was asked for.
            CHECK(plan.box.left() <= tight.left());
            CHECK(plan.box.top() <= tight.top());
            CHECK(plan.box.right() >= tight.right());
            CHECK(plan.box.bottom() >= tight.bottom());
        }
    }
}

TEST_CASE("stableGrid snaps the origin to the raster texel grid",
          "[offscreen-raster]")
{
    // Anchored at local 0, not at the content's own edge. If the origin tracked
    // the content, every raster would place it at the same sub-texel phase and
    // beginComposite's pixel snap would quantize the composite to whole device
    // pixels -- sub-pixel motion would judder.
    RasterPlan plan = planAtScale(2.0f);
    REQUIRE(fitRasterToBox(AABB(10.3f, 4.6f, 110.3f, 54.6f),
                           0,
                           RasterFit::stableGrid,
                           &plan));
    // floor(10.3 * 2) == 20 texels -> 10.0 local.
    CHECK(plan.box.left() == Approx(10.0f));
    CHECK(plan.box.top() == Approx(4.5f));
    CHECK(isWholeTexel(plan.box.left(), plan.rasterScale));
    CHECK(isWholeTexel(plan.box.top(), plan.rasterScale));
}

TEST_CASE("a translating box holds one allocation and keeps its phase",
          "[offscreen-raster]")
{
    // Sliding a mask across the artboard must not resize anything: the size is
    // fixed, only the origin walks the texel grid. And the sub-texel remainder
    // has to keep varying, or motion would judder.
    const float scale = 2.0f;
    uint32_t firstWidth = 0;
    bool sawFractionalPhase = false;
    for (int step = 0; step < 40; ++step)
    {
        const float x = 5.0f + static_cast<float>(step) * 0.1f;
        RasterPlan plan = planAtScale(scale);
        REQUIRE(fitRasterToBox(AABB(x, 0, x + 100.0f, 50.0f),
                               2,
                               RasterFit::stableGrid,
                               &plan));
        if (firstWidth == 0)
        {
            firstWidth = plan.widthPx;
        }
        CHECK(plan.widthPx == firstWidth);
        CHECK(plan.heightPx == bucketedRasterDim(104));
        CHECK(isWholeTexel(plan.box.left(), scale));
        // The content's offset inside the raster is what carries the sub-pixel
        // motion; it must not be constant.
        const float phase = (x - plan.box.left()) * scale;
        if (std::abs(phase - std::round(phase)) > 1e-3f)
        {
            sawFractionalPhase = true;
        }
    }
    CHECK(sawFractionalPhase);
}

TEST_CASE("the guard band adds transparent margin on every side",
          "[offscreen-raster]")
{
    RasterPlan bare = planAtScale(1.0f);
    REQUIRE(
        fitRasterToBox(AABB(0, 0, 100, 100), 0, RasterFit::stableGrid, &bare));
    RasterPlan guarded = planAtScale(1.0f);
    REQUIRE(fitRasterToBox(AABB(0, 0, 100, 100),
                           2,
                           RasterFit::stableGrid,
                           &guarded));

    // Two texels out on the near side, in local units.
    CHECK(guarded.box.left() == Approx(bare.box.left() - 2.0f));
    CHECK(guarded.box.top() == Approx(bare.box.top() - 2.0f));
    // The content is strictly interior, which is what stops the composite's
    // LinearClamp from smearing the edge texel under rotation.
    CHECK(guarded.box.left() < 0.0f);
    CHECK(guarded.box.right() > 100.0f);
}

TEST_CASE("the guard band cannot push a raster past kMaxDim",
          "[offscreen-raster]")
{
    // The cap holds back room for the band and for the snap, rather than
    // clamping it away afterwards and cropping.
    RasterPlan plan = planAtScale(8.0f);
    REQUIRE(fitRasterToBox(AABB(0, 0, 4000, 4000),
                           2,
                           RasterFit::stableGrid,
                           &plan));
    CHECK(plan.widthPx <= kMaxDim);
    CHECK(plan.heightPx <= kMaxDim);
    CHECK(plan.box.width() * plan.rasterScale ==
          Approx(static_cast<float>(plan.widthPx)));
}

TEST_CASE("holdRasterAllocation grows right and bottom only",
          "[offscreen-raster]")
{
    RasterPlan plan = planAtScale(2.0f);
    REQUIRE(
        fitRasterToBox(AABB(10, 10, 110, 60), 0, RasterFit::stableGrid, &plan));
    const float left = plan.box.left();
    const float top = plan.box.top();

    holdRasterAllocation(1024, 512, &plan);
    CHECK(plan.widthPx == 1024);
    CHECK(plan.heightPx == 512);
    // The origin is what beginComposite's pixel snap reads; moving it would
    // shift the composite on screen.
    CHECK(plan.box.left() == left);
    CHECK(plan.box.top() == top);
    CHECK(plan.box.width() * plan.rasterScale == Approx(1024.0f));
    CHECK(plan.box.height() * plan.rasterScale == Approx(512.0f));

    // Asking for less than it already has changes nothing.
    holdRasterAllocation(64, 64, &plan);
    CHECK(plan.widthPx == 1024);
    CHECK(plan.heightPx == 512);
}

TEST_CASE("resize grows at once and shrinks only after a streak",
          "[offscreen-raster]")
{
    uint8_t streak = 0;

    // Nothing allocated yet.
    RasterResize first = decideRasterResize(200, 100, 0, 0, &streak);
    CHECK(first.reallocate);
    CHECK(first.widthPx == 256);
    CHECK(first.heightPx == 128);

    // Inside the bucket: no churn at all, which is the whole point.
    RasterResize steady = decideRasterResize(250, 120, 256, 128, &streak);
    CHECK_FALSE(steady.reallocate);
    CHECK(steady.widthPx == 256);
    CHECK(steady.heightPx == 128);

    // Over the bucket: grow now, because a short raster crops.
    RasterResize grow = decideRasterResize(300, 120, 256, 128, &streak);
    CHECK(grow.reallocate);
    CHECK(grow.widthPx == 320);
    CHECK(grow.heightPx == 128);

    // Growing one axis must not sneak an un-hysteresised shrink onto the other.
    streak = 0;
    RasterResize oneAxis = decideRasterResize(300, 10, 256, 512, &streak);
    CHECK(oneAxis.reallocate);
    CHECK(oneAxis.widthPx == 320);
    CHECK(oneAxis.heightPx == 512);
}

TEST_CASE("a shrink waits kShrinkDraws draws", "[offscreen-raster]")
{
    uint8_t streak = 0;
    for (int draw = 1; draw < kShrinkDraws; ++draw)
    {
        RasterResize r = decideRasterResize(100, 100, 512, 512, &streak);
        CHECK_FALSE(r.reallocate);
        CHECK(r.widthPx == 512);
    }
    RasterResize acted = decideRasterResize(100, 100, 512, 512, &streak);
    CHECK(acted.reallocate);
    CHECK(acted.widthPx == 128);
    CHECK(acted.heightPx == 128);
    CHECK(streak == 0);
}

TEST_CASE("a single frame back at full size resets the shrink streak",
          "[offscreen-raster]")
{
    // An animation that dips small and comes back must never re-allocate.
    uint8_t streak = 0;
    for (int cycle = 0; cycle < 5; ++cycle)
    {
        for (int draw = 0; draw < kShrinkDraws - 1; ++draw)
        {
            CHECK_FALSE(
                decideRasterResize(100, 100, 512, 512, &streak).reallocate);
        }
        // Back to filling the allocation: streak clears.
        CHECK_FALSE(decideRasterResize(500, 500, 512, 512, &streak).reallocate);
        CHECK(streak == 0);
    }
}

#endif // RIVE_CANVAS
