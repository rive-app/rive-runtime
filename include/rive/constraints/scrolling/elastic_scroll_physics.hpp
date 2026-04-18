#ifndef _RIVE_ELASTIC_SCROLL_PHYSICS_HPP_
#define _RIVE_ELASTIC_SCROLL_PHYSICS_HPP_
#include "rive/generated/constraints/scrolling/elastic_scroll_physics_base.hpp"
#include <memory>
#include <stdio.h>
namespace rive
{

class ElasticScrollPhysicsHelper
{
private:
    float m_friction = 8.0f;
    float m_speedMultiplier = 1.0f;
    float m_elasticFactor = 0.66f;
    float m_target = 0;
    float m_current = 0;
    float m_speed = 0;
    float m_snapTarget = NAN;
    float m_runRangeMin = 0;
    float m_runRangeMax = 0;
    bool m_isRunning = false;

public:
    ElasticScrollPhysicsHelper(float friction,
                               float speedMultiplier,
                               float elasticFactor)
    {
        m_friction = friction;
        m_speedMultiplier = speedMultiplier;
        m_elasticFactor = elasticFactor;
    }

    bool isRunning() { return m_isRunning; }
    float target() { return m_target; }
    float clamp(float rangeMin, float rangeMax, float value);
    void run(float acceleration,
             float rangeMin,
             float rangeMax,
             float value,
             std::vector<float> snappingPoints,
             float contentSize,
             float viewportSize);
    float advance(float elapsedSeconds);

    /// Initiate a settling animation from current position to target.
    /// This skips the velocity phase and goes directly to smooth settling.
    void scrollTo(float current, float target, float rangeMin, float rangeMax);
};

class ElasticScrollPhysics : public ElasticScrollPhysicsBase
{
private:
    std::unique_ptr<ElasticScrollPhysicsHelper> m_physicsX;
    std::unique_ptr<ElasticScrollPhysicsHelper> m_physicsY;

public:
    bool enabled() override
    {
        return m_physicsX != nullptr || m_physicsY != nullptr;
    }
    bool isRunning() override
    {
        return (m_physicsX != nullptr && m_physicsX->isRunning()) ||
               (m_physicsY != nullptr && m_physicsY->isRunning());
    }
    float targetX() override
    {
        return m_physicsX != nullptr ? m_physicsX->target() : 0.0f;
    }
    float targetY() override
    {
        return m_physicsY != nullptr ? m_physicsY->target() : 0.0f;
    }
    bool hasTargetX() override { return m_physicsX != nullptr; }
    bool hasTargetY() override { return m_physicsY != nullptr; }
    Vec2D advance(float elapsedSeconds) override;
    Vec2D clamp(Vec2D rangeMin, Vec2D rangeMax, Vec2D value) override;
    void run(Vec2D rangeMin,
             Vec2D rangeMax,
             Vec2D value,
             std::vector<Vec2D> snappingPoints,
             float contentSize,
             float viewportSize) override;
    void prepare(DraggableConstraintDirection dir) override;
    void reset() override;

    /// Initiate animated scroll to target position.
    void scrollToPosition(Vec2D current,
                          Vec2D target,
                          Vec2D rangeMin,
                          Vec2D rangeMax,
                          bool horizontal,
                          bool vertical) override;
};
} // namespace rive

#endif