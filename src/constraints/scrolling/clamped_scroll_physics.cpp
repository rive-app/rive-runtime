#include "rive/constraints/scrolling/clamped_scroll_physics.hpp"
#include "math.h"

using namespace rive;

Vec2D ClampedScrollPhysics::advance(float elapsedSeconds)
{
    stop();
    return m_value;
}

void ClampedScrollPhysics::run(Vec2D rangeMin,
                               Vec2D rangeMax,
                               Vec2D value,
                               std::vector<Vec2D> snappingPoints,
                               float contentSize,
                               float viewportSize)
{
    ScrollPhysics::run(rangeMin,
                       rangeMax,
                       value,
                       snappingPoints,
                       contentSize,
                       viewportSize);
    m_value = clamp(rangeMin, rangeMax, value);
}

Vec2D ClampedScrollPhysics::clamp(Vec2D rangeMin, Vec2D rangeMax, Vec2D value)
{
    return Vec2D(math::clamp(value.x, rangeMin.x, rangeMax.x),
                 math::clamp(value.y, rangeMin.y, rangeMax.y));
}