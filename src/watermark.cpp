#include "rive/watermark.hpp"

#include "rive/animation/state_machine_instance.hpp"
#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include "rive/layout.hpp"
#include "rive/renderer.hpp"
#include "rive/factory.hpp"

#include <chrono>

using namespace rive;

namespace
{
// A watermark whose state machine never settles (it loops, or rests in a state
// with a looping animation) would otherwise hold the user's file hostage. Cap
// how long the pre-roll can run.
constexpr float watermarkMaxSeconds = 10.0f;

// How far ahead of the wall clock the pre-roll's *total* consumption may run
// and still be taken at face value. Wide enough for clock jitter and for the
// gap between a host measuring its frame time and calling us, narrow enough
// that it can't hide a frame's worth of fast-forward. Cumulative, not per
// call -- see clampElapsed.
constexpr float watermarkClockToleranceSeconds = 0.004f;

// No single frame may carry the watermark further than this, whichever clock
// the delta came from. Stops a stall, a breakpoint, or a backgrounded tab from
// resuming into a leap that swallows the pre-roll whole.
constexpr float watermarkMaxFrameSeconds = 0.25f;

// The pre-roll is played against solid black, so the watermark reads the same
// regardless of what the file behind it looks like.
constexpr ColorInt watermarkBackdropColor = 0xFF000000;

// steady_clock, not high_resolution_clock: the latter aliases the adjustable
// wall clock on some standard libraries, and this clock is what stops the
// pre-roll being seeked past. A clock that can be set backwards would hand
// back a negative delta and rewind the watermark.
int64_t nowMicros()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
} // namespace

Watermark::Watermark(std::unique_ptr<ArtboardInstance> artboard,
                     std::unique_ptr<StateMachineInstance> stateMachine) :
    m_artboard(std::move(artboard)),
    m_stateMachine(std::move(stateMachine)),
    m_lastTimeMicros(nowMicros())
{
    // The host's own frame origin is applied by whoever draws us, so the
    // watermark's content sits at its own (0, 0) and we align from there.
    m_artboard->frameOrigin(false);
}

Watermark::~Watermark() {}

float Watermark::clampElapsed(float elapsedSeconds)
{
    if (elapsedSeconds < 0.0f)
    {
        return 0.0f;
    }

    if (File::deterministicMode)
    {
        // The host owns the timeline: goldens, silvers and offline renders step
        // at whatever cadence they like and there is no wall clock to measure
        // them against. Neither guard below applies.
        return elapsedSeconds;
    }

    int64_t now = nowMicros();
    float wallSeconds = (float)(now - m_lastTimeMicros) / 1000000.0f;
    m_lastTimeMicros = now;
    if (wallSeconds < 0.0f)
    {
        // steady_clock should never go backwards; if a platform's does, treat
        // it as no time passing rather than letting it unwind the pre-roll.
        wallSeconds = 0.0f;
    }

    // Credit at most one frame's worth of real time per call, the same ceiling
    // a single advance may consume. Banking the whole gap let idle time be
    // spent all at once: an instance left alone for ten seconds -- a
    // backgrounded tab, a file loaded and not yet advanced -- funded a budget
    // that forty immediate advances could drain a frame at a time, finishing
    // the pre-roll without ever showing it. Capping the credit at the same
    // ceiling as the spend makes that arithmetic impossible.
    m_wallSeconds += wallSeconds > watermarkMaxFrameSeconds
                         ? watermarkMaxFrameSeconds
                         : wallSeconds;

    // What the pre-roll may still consume without outrunning real time. The
    // budget is cumulative and the tolerance is granted against the total, not
    // per call: comparing each delta on its own let a caller spend the
    // tolerance again on every frame, so a loop of advances at or just under
    // it -- advanceAndApply(0.004f) against a tolerance of 0.004 -- was never
    // clamped and could burn the whole pre-roll in microseconds of real time.
    float budget =
        m_wallSeconds + watermarkClockToleranceSeconds - m_elapsedSeconds;
    if (budget < 0.0f)
    {
        budget = 0.0f;
    }

    // The host's delta is honored while it fits the budget, so a host with a
    // better notion of frame time still drives playback; past that it gets
    // what has actually elapsed.
    float seconds = elapsedSeconds > budget ? budget : elapsedSeconds;
    return seconds > watermarkMaxFrameSeconds ? watermarkMaxFrameSeconds
                                              : seconds;
}

bool Watermark::advance(float elapsedSeconds)
{
    if (m_finished)
    {
        // The watermark drew its last frame already; this one is the host's.
        return false;
    }
    m_started = true;

    float seconds = clampElapsed(elapsedSeconds);

    // A settled state machine is the completion signal. Note that advancing by
    // zero always reports "keep going", so the priming zero-advance embedders
    // do right after instancing can't complete the watermark early.
    bool more = m_stateMachine->advanceAndApply(seconds);
    m_elapsedSeconds += seconds;
    m_finished = !more || m_elapsedSeconds >= watermarkMaxSeconds;

    // Even the finishing frame belongs to the watermark, so its last authored
    // frame is the one on screen rather than being skipped.
    return true;
}

void Watermark::drawBackdrop(Renderer* renderer, const AABB& hostBounds)
{
    auto factory = m_artboard->factory();
    if (factory == nullptr)
    {
        return;
    }
    if (m_backdropPaint == nullptr)
    {
        m_backdropPaint = factory->makeRenderPaint();
        m_backdropPaint->style(RenderPaintStyle::fill);
        m_backdropPaint->color(watermarkBackdropColor);
    }
    if (m_backdropPath == nullptr || !(m_backdropBounds == hostBounds))
    {
        m_backdropPath = factory->makeRenderPath(hostBounds);
        m_backdropBounds = hostBounds;
    }
    if (m_backdropPaint != nullptr && m_backdropPath != nullptr)
    {
        renderer->drawPath(m_backdropPath.get(), m_backdropPaint.get());
    }
}

void Watermark::draw(Renderer* renderer, const AABB& hostBounds)
{
    // Covers the host's whole box before the watermark is fit inside it, so
    // neither the frozen host nor the letterbox around the watermark shows.
    drawBackdrop(renderer, hostBounds);

    renderer->save();
    renderer->transform(computeAlignment(Fit::contain,
                                         Alignment::center,
                                         hostBounds,
                                         m_artboard->bounds()));
    // drawInternal, not draw: whoever called us already bumped the frame id.
    m_artboard->drawInternal(renderer);
    renderer->restore();
}
