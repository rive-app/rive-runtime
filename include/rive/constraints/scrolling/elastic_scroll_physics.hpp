#ifndef _RIVE_ELASTIC_SCROLL_PHYSICS_HPP_
#define _RIVE_ELASTIC_SCROLL_PHYSICS_HPP_
#include "rive/generated/constraints/scrolling/elastic_scroll_physics_base.hpp"
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
    float m_runRange = 0;
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
    float clamp(float range, float value);
    void run(float acceleration,
             float range,
             float value,
             std::vector<float> snappingPoints);
    float advance(float elapsedSeconds);
};

class ElasticScrollPhysics : public ElasticScrollPhysicsBase
{
private:
    ElasticScrollPhysicsHelper* m_physicsX;
    ElasticScrollPhysicsHelper* m_physicsY;

public:
    ~ElasticScrollPhysics();
    bool enabled() override
    {
        return m_physicsX != nullptr || m_physicsY != nullptr;
    }
    bool isRunning() override
    {
        return (m_physicsX != nullptr && m_physicsX->isRunning()) ||
               (m_physicsY != nullptr && m_physicsY->isRunning());
    }
    Vec2D advance(float elapsedSeconds) override;
    Vec2D clamp(Vec2D range, Vec2D value) override;
    void run(Vec2D range,
             Vec2D value,
             std::vector<Vec2D> snappingPoints) override;
    void prepare(DraggableConstraintDirection dir) override;
    void reset() override;
};
} // namespace rive

#endif