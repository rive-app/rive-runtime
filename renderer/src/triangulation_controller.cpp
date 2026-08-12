/*
 * Copyright 2026 Rive
 */

#include "rive/renderer/triangulation_controller.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace rive::gpu
{
bool TriangulationController::admits(float area, size_t verbCount)
{
    assert(isEligible(area, verbCount));
    if (std::isinf(m_thresholds.frameBudgetMs))
    {
        // The guards decide alone. There's no budget to spend down and
        // endFrame() won't tune, so there's no score or bracket worth tracking
        // either.
        return true;
    }
    if (budgetExhausted())
    {
        // Out of budget. That's no verdict on the path's score, so don't record
        // it as a rejection: it would tell endFrame() to loosen when the real
        // problem is that we already admitted too much.
        return false;
    }

    // Triangulate based on value density: the biggest wins are large-area /
    // low-verb fills, so score = area / cost, where cost models triangulation's
    // ~N-log-N verb complexity.
    assert(verbCount > 1);
    const float n = static_cast<float>(verbCount);
    const float score = area / (n * std::log2(n));
    if (score > m_scoreThreshold)
    {
        m_frameMinAdmittedScore = std::min(m_frameMinAdmittedScore, score);
        return true;
    }
    else
    {
        m_frameMaxRejectedScore = std::max(m_frameMaxRejectedScore, score);
        return false;
    }
}

void TriangulationController::endFrame()
{
    const double frameBudgetMs = m_thresholds.frameBudgetMs;
    if (frameBudgetMs <= 0 || std::isinf(frameBudgetMs))
    {
        // Nothing to tune: at 0 we never triangulate, and at infinity the
        // guards decide alone. Leave the threshold where it is so the decision
        // stays independent of measured time.
        return;
    }

    const double frameMs = m_frameSeconds * 1e3;
    // Low-pass (EWMA) the noisy per-frame signal so the controller tracks the
    // trend -- including slow thermal/DVFS drift -- rather than frame-to-frame
    // jitter in which paths happen to be onscreen.
    constexpr double Alpha = 0.1;
    m_timeEwmaMs += Alpha * (frameMs - m_timeEwmaMs);

    // Nudge the value-density threshold toward the budget, with a hysteresis
    // dead-band. Over budget -> raise it (admit fewer, shedding the
    // lowest-value paths first); under -> lower it.
    const double ratio = m_timeEwmaMs / frameBudgetMs;
    if (ratio > 1.05 || ratio < 0.95)
    {
        // Both sides aim at a score measured on this frame's bracket, betting
        // it recurs in the next one. Overshoot a little so the decision still
        // flips if it drifts.
        constexpr float BracketBackoff = .99f;
        // The sqrt makes the step large when far from budget and gentle near
        // it, so it converges in a handful of frames without oscillating.
        const double factor = std::clamp(std::sqrt(ratio), 0.5, 2.0);
        if (factor > 1)
        {
            // EWMA called for us to tighten (reject more triangulations).
            if (m_frameMinAdmittedScore !=
                std::numeric_limits<float>::infinity()) // If we admitted
                                                        // nothing, we already
                                                        // rejected everything,
                                                        // so tightening further
                                                        // does nothing.
            {
                // The multiplicative step is blind to the actual triangulations
                // being scored, operating solely on the time ratio to tighten
                // the heuristic. However, we do track the frame's min admitted
                // triangulation score, which lets us tighten enough to reject
                // at least one more path when the step alone wouldn't make a
                // difference.
                // NOTE: The EWMA would converge either way, but the bracket
                // gets us there in one frame instead of a geometric climb, and
                // doesn't overrun the budget along the way.
                m_scoreThreshold =
                    std::max(m_scoreThreshold * static_cast<float>(factor),
                             m_frameMinAdmittedScore / BracketBackoff);
            }
        }
        else
        {
            // EWMA called for us to loosen (admit more triangulations).
            if (m_frameMaxRejectedScore != 0) // If we rejected nothing, we
                                              // already admitted everything, so
                                              // loosening further does nothing.
            {
                // The multiplicative step is blind to the actual triangulations
                // being scored, operating solely on the time ratio to loosen
                // the heuristic. However, we do track the frame's max rejected
                // triangulation score, which lets us loosen enough to admit at
                // least one more path when the step alone wouldn't make a
                // difference.
                // NOTE: The EWMA would converge either way, but the bracket
                // gets us there in one frame instead of a geometric descent,
                // and doesn't leave triangulations on the table along the way.
                m_scoreThreshold =
                    std::min(m_scoreThreshold * static_cast<float>(factor),
                             m_frameMaxRejectedScore * BracketBackoff);
            }
        }
    }
    m_scoreThreshold = std::clamp(m_scoreThreshold, 1.f, 1e9f);
}
} // namespace rive::gpu
