/*
 * Copyright 2026 Rive
 */

#pragma once

#include <cstddef>
#include <limits>

namespace rive::gpu
{
// Specifies when to use interior triangulation on a filled path. Triangulation
// is CPU work that buys cheaper GPU work on large fills, so these decide when
// that trade is worth making.
struct TriangulationThresholds
{
    // Below this area, a fill can't amortize triangulation's fixed backend
    // cost. (In some modes this is a triangles<->cubics pipeline switch, or
    // worse, a barrier.)
    float minArea = 512.f * 512.f;

    // Absolute-cost ceiling.
    // NOTE: Triangulation is ~N-log-N in verb count.
    size_t maxVerbs = 256;

    // A given frame's CPU-time budget for interior triangulation
    // (milliseconds). The controller live-tunes a heuristic to decide whether a
    // path should be triangulated based on this budget, but once the budget is
    // spent, all remaining paths in the frame are drawn as midpoint fans.
    //
    // Two special values:
    //   0        -- never triangulate; nothing is affordable.
    //   infinity -- triangulate on the guards alone. No score, no overrun
    //               cutoff, and the heuristic stops tuning, so the decision
    //               doesn't depend on measured time. Golden/regression tests
    //               use this so paths meant to exercise triangulation do so on
    //               every run and machine.
    float frameBudgetMs = 2.f;
};

// Decides which filled paths get an interior triangulation, and keeps the CPU
// time spent building them near TriangulationThresholds::frameBudgetMs.
//
// Within the hard guards of isEligible(), triangulations are admitted by value
// density: score = area / cost, where cost models triangulation's ~N-log-N verb
// complexity. Admission is score > the tuned threshold; nothing here relies on
// that being strict.
//
// The threshold persists across frames. endFrame() nudges it from a low-pass of
// measured triangulation time to keep that time near frameBudgetMs. (But we
// never overrun the budget; once it's spent, all remaining paths use midpoint
// fans for the rest of the frame, regardless of score.)
class TriangulationController
{
public:
    void beginFrame(const TriangulationThresholds& thresholds)
    {
        m_thresholds = thresholds;
        m_frameCacheHits = 0;
        m_frameSeconds = 0;
        m_frameBuilt = 0;
        m_frameMinAdmittedScore = std::numeric_limits<float>::infinity();
        m_frameMaxRejectedScore = 0;
    }

    // Retunes the score threshold for future frames from this frame's
    // measurements.
    void endFrame();

    // Is interior triangulation ever worth it for a path this shape? These
    // guards are about the backend's fixed cost of drawing one, not the CPU
    // cost of building it, so they apply to cached triangulations too.
    bool isEligible(float area, size_t verbCount) const
    {
        return m_thresholds.frameBudgetMs > 0 && area >= m_thresholds.minArea &&
               verbCount <= m_thresholds.maxVerbs;
    }

    // Can we afford to build one right now? Recording the decision is what
    // tunes the threshold, so only ask for paths whose answer will be honored.
    // The path must have cleared isEligible() and come up empty on a cached
    // triangulation.
    bool admits(float area, size_t verbCount);

    // A draw reused a triangulation its path already had. Free, so it costs no
    // budget, but we count it so tests can tell cache hits apart from draws
    // that fell back to midpoint fans.
    void recordCacheHit() { ++m_frameCacheHits; }

    // A triangulation that admits() cleared got built, taking `seconds` of wall
    // time. Charged against the frame's budget.
    void recordBuilt(double seconds)
    {
        ++m_frameBuilt;
        m_frameSeconds += seconds;
    }

#ifdef WITH_RIVE_TOOLS
    // The tuned value-density that admits() compares against.
    float testingOnly_scoreThreshold() const { return m_scoreThreshold; }

    // Wall time spent building interior triangulations so far this frame.
    double testingOnly_secondsThisFrame() const { return m_frameSeconds; }

    // Interior triangulations the CPU actually built this frame.
    size_t testingOnly_builtThisFrame() const { return m_frameBuilt; }

    // Draws this frame that reused a path's cached triangulation.
    // NOTE: Just having a cached triangulation available isn't enough to be
    // counted here: isEligible() applies to every draw, so a cached path drawn
    // small enough that triangulation won't amortize on the GPU still falls
    // back to a midpoint fan.
    size_t testingOnly_cacheHitsThisFrame() const { return m_frameCacheHits; }

    bool testingOnly_budgetExhausted() const { return budgetExhausted(); }
#endif

private:
    // True once this frame's triangulations have spent frameBudgetMs. From then
    // on admits() turns every path away and they all use midpoint fans.
    bool budgetExhausted() const
    {
        return m_frameSeconds * 1e3 >= m_thresholds.frameBudgetMs;
    }

    TriangulationThresholds m_thresholds;

    // Persist across frames. m_scoreThreshold starts fully permissive so
    // triangulations get cached as fast as the budget allows. Once the cache is
    // primed, it converges to a value that favors the highest-value non-cached
    // (i.e., animating) triangulations for the given frame budget.
    float m_scoreThreshold = 1.f;
    double m_timeEwmaMs = 0;

    // Reset every beginFrame(). The min admitted and max rejected scores
    // bracket the gap m_scoreThreshold is sitting in, which is what lets
    // endFrame() move it far enough to flip a decision.
    size_t m_frameCacheHits = 0;
    double m_frameSeconds = 0;
    size_t m_frameBuilt = 0;
    float m_frameMinAdmittedScore = std::numeric_limits<float>::infinity();
    float m_frameMaxRejectedScore = 0;
};
} // namespace rive::gpu
