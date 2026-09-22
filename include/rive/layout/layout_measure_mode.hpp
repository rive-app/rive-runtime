#ifndef _RIVE_LAYOUT_MEASURE_MODE_HPP_
#define _RIVE_LAYOUT_MEASURE_MODE_HPP_
namespace rive
{
enum class LayoutMeasureMode : uint8_t
{
    undefined = 0,
    exactly = 1,
    atMost = 2
};

// Drops the available-space bound for hugUnbounded. Only atMost is a bound;
// exactly means the size is already decided, so it has to survive.
inline LayoutMeasureMode unboundMeasureMode(LayoutMeasureMode mode)
{
    return mode == LayoutMeasureMode::atMost ? LayoutMeasureMode::undefined
                                             : mode;
}

// During a grid's min-content probe, atMost(0) is not an offer of no space:
// it is yoga asking for the item's MIN-CONTENT contribution, which is what an
// intrinsic track (1fr, auto) floors at. Every leaf measure answers
// min(available, own) -- zero here -- and a track that floors at zero collapses
// rather than holding its content. Widen that one axis so the leaf reports what
// it actually is. [probing] is YGConfigIsMeasuringMinContent: outside a probe
// an atMost(0) really is a zero-sized box and must stay one.
inline LayoutMeasureMode measureModeForContent(LayoutMeasureMode mode,
                                               float available,
                                               bool probing)
{
    return (probing && mode == LayoutMeasureMode::atMost && !(available > 0))
               ? LayoutMeasureMode::undefined
               : mode;
}
} // namespace rive
#endif