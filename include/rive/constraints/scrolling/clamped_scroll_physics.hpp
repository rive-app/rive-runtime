#ifndef _RIVE_CLAMPED_SCROLL_PHYSICS_HPP_
#define _RIVE_CLAMPED_SCROLL_PHYSICS_HPP_
#include "rive/generated/constraints/scrolling/clamped_scroll_physics_base.hpp"
#include <stdio.h>
namespace rive
{
class ClampedScrollPhysics : public ClampedScrollPhysicsBase
{
private:
    Vec2D m_value;

public:
    Vec2D advance(float elapsedSeconds) override;
    void run(Vec2D range,
             Vec2D value,
             std::vector<Vec2D> snappingPoints) override;
    Vec2D clamp(Vec2D range, Vec2D value) override;
};
} // namespace rive

#endif