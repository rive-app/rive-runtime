#include "rive/constraints/scrolling/elastic_scroll_physics.hpp"
#include "math.h"

using namespace rive;

ElasticScrollPhysics::~ElasticScrollPhysics()
{
    delete m_physicsX;
    delete m_physicsY;
}

Vec2D ElasticScrollPhysics::advance(float elapsedSeconds)
{
    float advanceX =
        m_physicsX != nullptr ? m_physicsX->advance(elapsedSeconds) : 0.0f;
    float advanceY =
        m_physicsY != nullptr ? m_physicsY->advance(elapsedSeconds) : 0.0f;
    bool isRunningX = m_physicsX != nullptr && m_physicsX->isRunning();
    bool isRunningY = m_physicsY != nullptr && m_physicsY->isRunning();
    if (!isRunningX && !isRunningY)
    {
        reset();
    }
    return Vec2D(advanceX, advanceY);
}

Vec2D ElasticScrollPhysics::clamp(Vec2D rangeMin, Vec2D rangeMax, Vec2D value)
{
    float clampX = m_physicsX != nullptr
                       ? m_physicsX->clamp(rangeMin.x, rangeMax.x, value.x)
                       : 0.0f;
    float clampY = m_physicsY != nullptr
                       ? m_physicsY->clamp(rangeMin.y, rangeMax.y, value.y)
                       : 0.0f;
    return Vec2D(clampX, clampY);
}

void ElasticScrollPhysics::run(Vec2D rangeMin,
                               Vec2D rangeMax,
                               Vec2D value,
                               std::vector<Vec2D> snappingPoints)
{
    Super::run(rangeMin, rangeMax, value, snappingPoints);
    std::vector<float> xPoints;
    std::vector<float> yPoints;
    for (auto pt : snappingPoints)
    {
        xPoints.push_back(pt.x);
        yPoints.push_back(pt.y);
    }
    if (m_physicsX != nullptr)
    {
        m_physicsX->run(m_acceleration.x,
                        rangeMin.x,
                        rangeMax.x,
                        value.x,
                        xPoints);
    }
    if (m_physicsY != nullptr)
    {
        m_physicsY->run(m_acceleration.y,
                        rangeMin.y,
                        rangeMax.y,
                        value.y,
                        yPoints);
    }
}

void ElasticScrollPhysics::prepare(DraggableConstraintDirection dir)
{
    Super::prepare(dir);
    if (dir == DraggableConstraintDirection::horizontal ||
        dir == DraggableConstraintDirection::all)
    {
        m_physicsX = new ElasticScrollPhysicsHelper(friction(),
                                                    speedMultiplier(),
                                                    elasticFactor());
    }
    if (dir == DraggableConstraintDirection::vertical ||
        dir == DraggableConstraintDirection::all)
    {
        m_physicsY = new ElasticScrollPhysicsHelper(friction(),
                                                    speedMultiplier(),
                                                    elasticFactor());
    }
}

void ElasticScrollPhysics::reset()
{
    Super::reset();
    m_physicsX = nullptr;
    m_physicsY = nullptr;
}

float ElasticScrollPhysicsHelper::advance(float elapsedSeconds)
{
    if (m_speed != 0)
    {
        m_current += m_speed * elapsedSeconds;

        auto friction = m_friction;
        if (m_current < m_runRangeMin)
        {
            friction *= 4;
        }
        else if (m_current > m_runRangeMax)
        {
            friction *= 4;
        }

        m_speed += -m_speed * std::min(1.0f, elapsedSeconds * friction);

        if (abs(m_speed) < 5)
        {
            m_speed = 0;
            if (m_current < m_runRangeMin)
            {
                m_target = m_runRangeMin;
            }
            else if (m_current > m_runRangeMax)
            {
                m_target = m_runRangeMax;
            }
            else
            {
                m_target = m_current;
            }
        }
        return m_current;
    }
    auto diff = m_target - m_current;
    if (abs(diff) < 0.1)
    {
        m_current = m_target;
        m_isRunning = false;
    }
    else
    {
        m_current += diff * std::min(1.0f, elapsedSeconds * 15.0f);
    }
    return m_current;
}

float ElasticScrollPhysicsHelper::clamp(float rangeMin,
                                        float rangeMax,
                                        float value)
{
    if (value < rangeMin)
    {
        return rangeMin - pow(-(value - rangeMin), m_elasticFactor);
    }
    else if (value > rangeMax)
    {
        return rangeMax + pow(value + rangeMax, m_elasticFactor);
    }
    return value;
}

void ElasticScrollPhysicsHelper::run(float acceleration,
                                     float rangeMin,
                                     float rangeMax,
                                     float value,
                                     std::vector<float> snappingPoints)
{
    m_isRunning = true;
    m_runRangeMin = rangeMin;
    m_runRangeMax = rangeMax;
    if (abs(acceleration) > 100)
    {
        m_speed = acceleration * 0.16f * 0.16f * 0.1f * m_speedMultiplier;
    }
    else
    {
        m_speed = 0;
    }
    if (value < rangeMin)
    {
        m_target = rangeMin;
    }
    else if (value > rangeMax)
    {
        m_target = rangeMax;
    }
    else
    {
        m_target = value;
    }
    m_current = value;

    if (!snappingPoints.empty())
    {
        float endTarget = -(m_current + m_speed / m_friction);
        float closest = std::numeric_limits<float>::max();
        float snapTarget = 0;
        for (auto snap : snappingPoints)
        {
            float diff = std::abs(snap - endTarget);
            if (diff < closest)
            {
                closest = diff;
                snapTarget = snap;
            }
        }
        m_speed = -(snapTarget + m_current) * m_friction;
    }
}