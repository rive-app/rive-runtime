/*
 * Copyright 2026 Rive
 */

// Tests the adaptive interior-triangulation budget controller.
//
// The first half drives TriangulationController directly: its whole contract is
// numbers in (area, verb count, seconds spent) and decisions out, so triangles,
// paths and a GPU backend would only get in the way. The controller's
// convergence is observable through its score threshold alone -- that threshold
// only stops moving once the EWMA of per-frame triangulation time is inside the
// dead-band, so "threshold reaches steady state" is equivalent to
// "triangulation time converged to the budget."
//
// The second half goes through a RenderContext on a fake clock, for the
// properties that only exist once real paths and the triangulation cache are in
// play.

#include <limits>
#include "rive/renderer/rive_renderer.hpp"
#include "rive/renderer/render_context.hpp"
#include "rive/renderer/triangulation_controller.hpp"
#include "common/render_context_null.hpp"
#include <catch.hpp>

DISABLE_CLANG_SIMD_ABI_WARNING()

namespace rive::gpu
{
namespace
{
constexpr float Infinity = std::numeric_limits<float>::infinity();

// A path that clears the guards comfortably, and one that clears them by less.
// Score is area / (n * log2 n), so the smaller one is also the lower-value one.
constexpr float BigArea = 1024.f * 1024.f;
constexpr float SmallArea = 600.f * 600.f;
constexpr size_t RectVerbs = 5;

// Runs `frames` frames of `drawsPerFrame` identical eligible paths, each
// costing `secondsEach` to build when admitted. Returns how many the last frame
// admitted.
int runFrames(TriangulationController& tri,
              int frames,
              int drawsPerFrame,
              float frameBudgetMs,
              double secondsEach,
              float area = BigArea)
{
    int admitted = 0;
    for (int f = 0; f < frames; ++f)
    {
        admitted = 0;
        tri.beginFrame({.frameBudgetMs = frameBudgetMs});
        // The guards are not what's under test here; every path must clear them
        // so the only thing deciding is the score.
        REQUIRE(tri.isEligible(area, RectVerbs));
        for (int d = 0; d < drawsPerFrame; ++d)
        {
            if (tri.admits(area, RectVerbs))
            {
                ++admitted;
                tri.recordBuilt(secondsEach);
            }
        }
        tri.endFrame();
    }
    return admitted;
}

// The controller relaxes the threshold when triangulation is under budget and
// tightens it when over -- the closed loop responds in both directions.
TEST_CASE("interior triangulation budget controller adapts",
          "[TriangulationController]")
{
    TriangulationController tri;
    // The threshold starts fully permissive, so the first move must be upward.
    const float initialThreshold = tri.testingOnly_scoreThreshold();

    // 10ms per triangulation is far over the 2ms budget, so the threshold
    // climbs to shed triangulations.
    runFrames(tri, /*frames=*/40, /*drawsPerFrame=*/8, 2.f, /*seconds=*/0.01);
    const float tightened = tri.testingOnly_scoreThreshold();
    CHECK(tightened > initialThreshold);

    // Free triangulations can never exceed the budget, so it relaxes until it's
    // admitting everything again.
    const int lastFrameAdmitted =
        runFrames(tri, /*frames=*/40, /*drawsPerFrame=*/8, 2.f, /*seconds=*/0);
    CHECK(lastFrameAdmitted == 8);
    CHECK(tri.testingOnly_scoreThreshold() < tightened);
}

// The in-frame backstop. The threshold above only reacts between frames, so
// without this a single frame of expensive fills could overrun by an unbounded
// amount while the controller catches up.
TEST_CASE("interior triangulation stops once the frame budget is spent",
          "[TriangulationController]")
{
    TriangulationController tri;

    // No warm-up needed: the threshold starts fully permissive, so the score
    // admits every path and the cutoff is the only thing that can stop them.
    //
    // 1.5ms per triangulation against a 2ms budget: the 1st leaves the frame
    // under (1.5ms), the 2nd puts it over (3.0ms), and every one after that
    // must be shed.
    tri.beginFrame({.frameBudgetMs = 2.f});
    int admitted = 0;
    for (int d = 0; d < 8; ++d)
    {
        if (tri.admits(BigArea, RectVerbs))
        {
            ++admitted;
            tri.recordBuilt(0.0015);
        }
    }

    // Exactly the two that fit, not all eight.
    CHECK(admitted == 2);
    CHECK(tri.testingOnly_builtThisFrame() == 2);
    CHECK(tri.testingOnly_budgetExhausted());
    CHECK(tri.testingOnly_secondsThisFrame() == Approx(0.003));

    // The six that were shed lost on cost, not on score, so they say nothing
    // about where the threshold belongs. Feeding them to the tuner as
    // rejections would tell it to loosen -- the opposite of what a frame that
    // just blew its budget needs.
    const float threshold = tri.testingOnly_scoreThreshold();
    tri.endFrame();
    CHECK(tri.testingOnly_scoreThreshold() == threshold);
}

// With an infinite budget the decision is guards-only: no matter how expensive
// triangulation gets, the controller must never move the threshold. This is the
// property golden tests rely on.
TEST_CASE("an infinite triangulation budget ignores measured time",
          "[TriangulationController]")
{
    TriangulationController tri;
    const float initialThreshold = tri.testingOnly_scoreThreshold();

    // 1s per triangulation -- absurdly over any real budget.
    const int lastFrameAdmitted =
        runFrames(tri, /*frames=*/20, /*drawsPerFrame=*/8, Infinity, 1.0);
    CHECK(lastFrameAdmitted == 8);
    CHECK(tri.testingOnly_scoreThreshold() == initialThreshold);
}

// "Guards alone" also has to hold on a controller whose threshold was already
// driven up by earlier finite-budget frames. The threshold persists across
// frames and tuning simply stops once the budget is infinite, so without an
// explicit bypass the decision would still depend on timing that happened
// before the mode was switched on -- exactly what this mode exists to rule out.
TEST_CASE("an infinite triangulation budget ignores a raised threshold",
          "[TriangulationController]")
{
    TriangulationController tri;

    // Drive the threshold up with expensive frames. It settles just above the
    // score of the paths runFrames() feeds it, since tightening stops once
    // nothing is admitted.
    runFrames(tri, /*frames=*/40, /*drawsPerFrame=*/8, 2.f, /*seconds=*/0.01);

    // SmallArea scores well under where the threshold settled, so on a finite
    // budget the score alone rejects it...
    tri.beginFrame({.frameBudgetMs = 2.f});
    REQUIRE(tri.isEligible(SmallArea, RectVerbs));
    REQUIRE(!tri.admits(SmallArea, RectVerbs));

    // ...but at infinity it's admitted anyway.
    tri.beginFrame({.frameBudgetMs = Infinity});
    CHECK(tri.admits(SmallArea, RectVerbs));
}

// The other special value: nothing is affordable, so no path triangulates no
// matter how well it scores or how cheap it would have been.
TEST_CASE("a zero triangulation budget triangulates nothing",
          "[TriangulationController]")
{
    TriangulationController tri;

    // Drive the threshold somewhere non-default so "unchanged" below is a real
    // claim rather than a restatement of the initial value.
    runFrames(tri, /*frames=*/40, /*drawsPerFrame=*/8, 2.f, /*seconds=*/0.01);
    const float tightened = tri.testingOnly_scoreThreshold();
    REQUIRE(tightened > 1.f);

    tri.beginFrame({.frameBudgetMs = 0.f});
    // Control: this path is eligible when there's a budget, so a rejection here
    // is the budget's doing and not the guards'.
    CHECK(!tri.isEligible(BigArea, RectVerbs));

    // And with nothing triangulated there's nothing to tune, so the controller
    // leaves the threshold alone rather than drifting on an empty signal.
    tri.endFrame();
    CHECK(tri.testingOnly_scoreThreshold() == tightened);
}

// ============================================================================
// Everything below needs a real RenderContext: these are properties of how the
// renderer consults the controller, and of the triangulation cache, neither of
// which the controller can see on its own.
// ============================================================================

} // namespace
} // namespace rive::gpu

// Null context whose steady clock advances a fixed amount on every read.
// PathDraw::Make brackets each interior triangulation with two consecutive
// secondsNow() reads, so every triangulation measures exactly m_perCallAdvance
// seconds -- letting the test dial triangulation cost up and down.
class AdvancingClockContextImpl : public RenderContextNULL
{
public:
    mutable double m_seconds = 0;
    double m_perCallAdvance = 0;
    double secondsNow() const override
    {
        double now = m_seconds;
        m_seconds += m_perCallAdvance;
        return now;
    }
};

class BudgetTestContext : public rive::gpu::RenderContext
{
public:
    BudgetTestContext() :
        RenderContext(std::make_unique<AdvancingClockContextImpl>())
    {}
    AdvancingClockContextImpl* clock()
    {
        return static_impl_cast<AdvancingClockContextImpl>();
    }
    // Shorthand: every check below goes through the controller.
    rive::gpu::TriangulationController& tri()
    {
        return triangulationController();
    }
};

namespace rive::gpu
{
namespace
{
struct FrameResult
{
    double seconds;
    size_t built;
    size_t cacheHits;
};

// A triangulation the path already has costs nothing to fetch, so it must not
// be charged to the frame's budget. Charging it would let a fully primed scene
// spend its whole budget on work it isn't doing.
TEST_CASE("a cached triangulation costs no budget", "[RenderContext]")
{
    BudgetTestContext ctx;
    ctx.clock()->m_perCallAdvance = 0.001;

    RiveRenderer renderer(&ctx);
    auto path = ctx.makeEmptyRenderPath();
    path->addRect(0, 0, 1024, 1024);
    auto paint = ctx.makeRenderPaint();
    auto renderTarget = ctx.clock()->makeRenderTarget(2048, 2048);

    const auto frame = [&](float budget) {
        ctx.beginFrame({
            .renderTargetWidth = 2048,
            .renderTargetHeight = 2048,
            .triangulationThresholds = {.frameBudgetMs = budget},
        });
        renderer.drawPath(path.get(), paint.get());
        FrameResult r = {ctx.tri().testingOnly_secondsThisFrame(),
                         ctx.tri().testingOnly_builtThisFrame(),
                         ctx.tri().testingOnly_cacheHitsThisFrame()};
        ctx.flush({.renderTarget = renderTarget.get()});
        return r;
    };

    // Prime: first sighting builds a throwaway, second promotes it. Both are
    // real work and cost time.
    CHECK(frame(1000.f).seconds > 0);
    CHECK(frame(1000.f).seconds > 0);

    // Now it's cached. The draw still triangulates, but by reuse and for free.
    const FrameResult cached = frame(1000.f);
    CHECK(cached.built == 0);
    CHECK(cached.cacheHits == 1);
    CHECK(cached.seconds == 0);
}

// The guards are about GPU cost, so they apply even once the CPU work is
// already done. A cached triangulation is free to reuse, but reusing it on a
// path too small to amortize triangulation's fixed backend cost would still be
// the wrong call.
TEST_CASE("a small path doesn't use its cached triangulation",
          "[RenderContext]")
{
    BudgetTestContext ctx;
    ctx.clock()->m_perCallAdvance = 0.001;

    RiveRenderer renderer(&ctx);
    auto path = ctx.makeEmptyRenderPath();
    path->addRect(0, 0, 1024, 1024); // 1024^2, comfortably over minArea
    auto paint = ctx.makeRenderPaint();
    auto renderTarget = ctx.clock()->makeRenderTarget(2048, 2048);

    const auto frame = [&](float scale) {
        ctx.beginFrame({
            .renderTargetWidth = 2048,
            .renderTargetHeight = 2048,
            .triangulationThresholds = {.frameBudgetMs = 1000.f},
        });
        renderer.save();
        renderer.transform(Mat2D(scale, 0, 0, scale, 0, 0));
        renderer.drawPath(path.get(), paint.get());
        renderer.restore();
        FrameResult r = {ctx.tri().testingOnly_secondsThisFrame(),
                         ctx.tri().testingOnly_builtThisFrame(),
                         ctx.tri().testingOnly_cacheHitsThisFrame()};
        ctx.flush({.renderTarget = renderTarget.get()});
        return r;
    };

    // Prime at full size: build, promote, then reuse.
    frame(1.f);
    frame(1.f);
    REQUIRE(frame(1.f).cacheHits == 1);

    // The same path at quarter scale covers 1/16 the area, which is under
    // minArea (512^2). The triangulation is still sitting there, and still
    // isn't used.
    const FrameResult small = frame(0.25f);
    CHECK(small.cacheHits == 0);
    CHECK(small.built == 0);
}

// A cached triangulation is free, so an exhausted budget is no reason to skip
// it -- doing so would drop a primed scene back to midpoint fans the moment any
// other path spent the budget.
TEST_CASE("an over-budget frame still uses a cached triangulation",
          "[RenderContext]")
{
    BudgetTestContext ctx;
    ctx.clock()->m_perCallAdvance = 0.005; // 5ms, over the budget below

    RiveRenderer renderer(&ctx);
    auto cachedPath = ctx.makeEmptyRenderPath();
    cachedPath->addRect(0, 0, 1024, 1024);
    auto freshPath = ctx.makeEmptyRenderPath();
    freshPath->addRect(0, 0, 1024, 1024);
    auto paint = ctx.makeRenderPaint();
    auto renderTarget = ctx.clock()->makeRenderTarget(2048, 2048);

    // Prime cachedPath over two generous frames.
    for (int i = 0; i < 2; ++i)
    {
        ctx.beginFrame({
            .renderTargetWidth = 2048,
            .renderTargetHeight = 2048,
            .triangulationThresholds = {.frameBudgetMs = 1000.f},
        });
        renderer.drawPath(cachedPath.get(), paint.get());
        ctx.flush({.renderTarget = renderTarget.get()});
    }

    // Now a tight frame: freshPath triangulates and blows the budget, then
    // cachedPath is drawn. It's free, so it must still be triangulated.
    ctx.beginFrame({
        .renderTargetWidth = 2048,
        .renderTargetHeight = 2048,
        .triangulationThresholds = {.frameBudgetMs = 2.f},
    });
    renderer.drawPath(freshPath.get(), paint.get());
    REQUIRE(ctx.tri().testingOnly_builtThisFrame() == 1);
    REQUIRE(ctx.tri().testingOnly_budgetExhausted());

    renderer.drawPath(cachedPath.get(), paint.get());
    // Reused, not rebuilt -- the budget is spent, so a rebuild would be wrong
    // too.
    CHECK(ctx.tri().testingOnly_cacheHitsThisFrame() == 1);
    CHECK(ctx.tri().testingOnly_builtThisFrame() == 1);
    // ...and it cost nothing, so the overrun didn't get worse.
    CHECK(ctx.tri().testingOnly_secondsThisFrame() == Approx(0.005));
    ctx.flush({.renderTarget = renderTarget.get()});
}

// The regime the controller can't reach on its own: a scene that stops
// changing. Triangulating it costs twice and is then free forever, so the
// steady state is no cost at all and the threshold belongs at the floor with
// everything admitted.
//
// Getting there is not automatic, because a raised threshold prevents the very
// priming that would make the scene cheap: nothing is admitted, so nothing
// reaches a second sighting, so nothing caches, and the cost stays at zero for
// the wrong reason. It resolves only by way of the loosening bracket -- zero
// cost decays the EWMA, the threshold drops to just under the highest score it
// rejected, that one path builds, and from there the scene primes itself.
TEST_CASE("the threshold relaxes once a scene settles and caches",
          "[RenderContext]")
{
    BudgetTestContext ctx;

    // Churn expensively until the threshold is high enough to reject the path
    // used below. The geometry is rebuilt every frame so it never reaches a
    // second sighting and is therefore never cached -- that's what generates
    // sustained triangulation cost for the controller to react to.
    RiveRenderer renderer(&ctx);
    auto renderTarget = ctx.clock()->makeRenderTarget(2048, 2048);
    auto paint = ctx.makeRenderPaint();
    ctx.clock()->m_perCallAdvance = 0.01;
    {
        auto churn = ctx.makeEmptyRenderPath();
        for (int f = 0; f < 40; ++f)
        {
            churn->rewind();
            churn->addRect(0, 0, 1024, 1024);
            ctx.beginFrame({
                .renderTargetWidth = 2048,
                .renderTargetHeight = 2048,
                .triangulationThresholds = {.frameBudgetMs = 2.f},
            });
            for (int d = 0; d < 8; ++d)
            {
                renderer.drawPath(churn.get(), paint.get());
            }
            ctx.flush({.renderTarget = renderTarget.get()});
        }
    }
    const float tightened = ctx.tri().testingOnly_scoreThreshold();
    REQUIRE(tightened > 5e4f);

    // Now the scene settles: same geometry every frame, so it can cache. Note
    // triangulating is still expensive, so nothing here is under budget until
    // the cache starts serving.
    auto path = ctx.makeEmptyRenderPath();
    path->addRect(0, 0, 1024, 1024);

    size_t built = 0, cacheHits = 0;
    for (int f = 0; f < 20; ++f)
    {
        ctx.beginFrame({
            .renderTargetWidth = 2048,
            .renderTargetHeight = 2048,
            .triangulationThresholds = {.frameBudgetMs = 2.f},
        });
        renderer.drawPath(path.get(), paint.get());
        built = ctx.tri().testingOnly_builtThisFrame();
        cacheHits = ctx.tri().testingOnly_cacheHitsThisFrame();
        ctx.flush({.renderTarget = renderTarget.get()});
    }

    // The scene primed itself and now runs free.
    CHECK(built == 0);
    CHECK(cacheHits == 1);
    // ...and with nothing left to charge the budget, the threshold came back
    // down rather than staying where the churn left it.
    CHECK(ctx.tri().testingOnly_scoreThreshold() < tightened);
}

// The wiring: FrameDescriptor::triangulationThresholds has to reach the
// controller, and the renderer's draw path has to honor what comes back. The
// decision logic itself is covered above; this is the trip between the two, and
// the special values are the cheapest way to observe it -- they're the ones
// where the answer can't be anything else.
TEST_CASE("frame thresholds reach the renderer's triangulation decision",
          "[RenderContext]")
{
    BudgetTestContext ctx;
    ctx.clock()->m_perCallAdvance = 1.0; // 1s -- over any finite budget.

    RiveRenderer renderer(&ctx);
    auto path = ctx.makeEmptyRenderPath();
    auto paint = ctx.makeRenderPaint();
    auto renderTarget = ctx.clock()->makeRenderTarget(2048, 2048);

    // Fresh geometry every frame, so nothing caches and every draw is a real
    // decision.
    const auto frame = [&](float budgetMs) {
        path->rewind();
        path->addRect(0, 0, 1024, 1024);
        ctx.beginFrame({
            .renderTargetWidth = 2048,
            .renderTargetHeight = 2048,
            .triangulationThresholds = {.frameBudgetMs = budgetMs},
        });
        renderer.drawPath(path.get(), paint.get());
        const size_t built = ctx.tri().testingOnly_builtThisFrame();
        ctx.flush({.renderTarget = renderTarget.get()});
        return built;
    };

    CHECK(frame(0.f) == 0);      // Never affordable.
    CHECK(frame(Infinity) == 1); // Guards alone, at any cost.
    // ...and neither budget is tunable, so nothing drifted on the way through.
    CHECK(ctx.tri().testingOnly_scoreThreshold() == 1.f);
}
} // namespace
} // namespace rive::gpu
