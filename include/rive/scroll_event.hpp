#ifndef _RIVE_SCROLL_EVENT_HPP_
#define _RIVE_SCROLL_EVENT_HPP_

#include "rive/math/vec2d.hpp"
#include <stdint.h>

namespace rive
{
/// Where a scroll event sits within its gesture. A phaseless wheel only ever
/// reports update.
enum class ScrollPhase : uint8_t
{
    /// Stops any running fling and primes velocity tracking.
    begin = 0,

    update = 1,

    /// Contact ended; the runtime flings from the velocity it accumulated.
    /// Hosts whose platform generates its own momentum never send this.
    end = 2,

    /// Platform-generated after release. Applied directly, never flung on top
    /// of; closed by going idle rather than by an end.
    momentum = 3,

    /// Contact landed during momentum. Halts motion; delta is ignored.
    inertiaCancel = 4,
};

/// A scroll delta from indirect input: a wheel, or a trackpad gesture the
/// platform already classified as a scroll. Direct touch stays on the pointer
/// path.
struct ScrollEvent
{
    /// Distance the content moves, in artboard-space pixels -- the same sense
    /// as ScrollConstraint::dragView, so revealing later content is negative.
    /// Hosts normalize; the runtime never sees platform units.
    Vec2D delta;

    ScrollPhase phase = ScrollPhase::update;

    /// Pixel-precise source (trackpad) rather than a detented wheel.
    bool precise = false;
};
} // namespace rive

#endif
