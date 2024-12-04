#include "rive/constraints/scrolling/clamped_scroll_physics.hpp"
#include "math.h"

using namespace rive;

Vec2D ClampedScrollPhysics::advance(float elapsedSeconds)
{
    stop();
    return m_value;
}

void ClampedScrollPhysics::run(Vec2D range,
                               Vec2D value,
                               std::vector<Vec2D> snappingPoints)
{
    ScrollPhysics::run(range, value, snappingPoints);
    m_value = clamp(range, value);
}

Vec2D ClampedScrollPhysics::clamp(Vec2D range, Vec2D value)
{
    return Vec2D(math::clamp(value.x, range.x, 0),
                 math::clamp(value.y, range.y, 0));
}