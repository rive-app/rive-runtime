#ifndef _RIVE_SCROLL_PHYSICS_HPP_
#define _RIVE_SCROLL_PHYSICS_HPP_
#include "rive/artboard.hpp"
#include "rive/backboard.hpp"
#include "rive/constraints/draggable_constraint.hpp"
#include "rive/generated/constraints/scrolling/scroll_physics_base.hpp"
#include "rive/importers/artboard_importer.hpp"
#include "rive/importers/backboard_importer.hpp"
#include "rive/importers/import_stack.hpp"
#include "rive/math/math_types.hpp"
#include "rive/math/vec2d.hpp"
#include <chrono>
#include <stdio.h>
#include <vector>
namespace rive
{
class ScrollConstraint;

enum class ScrollPhysicsType : uint8_t
{
    elastic,
    clamped
};

class ScrollPhysics : public ScrollPhysicsBase
{
private:
    bool m_isRunning = false;
    long long m_lastTime =
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch())
            .count();

protected:
    DraggableConstraintDirection m_direction;
    Vec2D m_speed;
    Vec2D m_acceleration;

public:
    virtual bool enabled() { return isRunning(); }
    virtual bool isRunning() { return m_isRunning; }
    virtual void prepare(DraggableConstraintDirection dir)
    {
        m_direction = dir;
    }
    virtual Vec2D clamp(Vec2D range, Vec2D value) { return Vec2D(); };
    virtual Vec2D advance(float elapsedSeconds) { return Vec2D(); };
    virtual void accumulate(Vec2D delta);
    virtual void run(Vec2D range,
                     Vec2D value,
                     std::vector<Vec2D> snappingPoints)
    {
        m_isRunning = true;
    }
    virtual void stop() { m_isRunning = false; }
    virtual void reset()
    {
        m_acceleration = Vec2D();
        stop();
    }
    StatusCode import(ImportStack& importStack) override;
};
} // namespace rive

#endif