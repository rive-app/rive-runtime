#include "rive/constraints/scroll_physics.hpp"
#include "math.h"

using namespace rive;

float MelaScrollPhysics::advance(float elapsedSeconds)
{
    if (m_speed != 0)
    {
        m_current += m_speed * elapsedSeconds;

        auto friction = m_friction;
        if (m_current < m_runRange)
        {
            friction *= 4;
        }
        else if (m_current > 0)
        {
            friction *= 4;
        }

        m_speed += -m_speed * std::min(1.0f, elapsedSeconds * friction);

        if (abs(m_speed) < 5)
        {
            m_speed = 0;
            if (m_current < m_runRange)
            {
                m_target = m_runRange;
            }
            else if (m_current > 0)
            {
                m_target = 0;
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
        stop();
    }
    else
    {
        m_current += diff * std::min(1.0f, elapsedSeconds * 15.0f);
    }
    return m_current;
}

float MelaScrollPhysics::clamp(float range, float value)
{
    if (value < range)
    {
        return range - pow(-(value - range), m_activeClampFactor);
    }
    else if (value > 0)
    {
        return pow(value, m_activeClampFactor);
    }
    return value;
}

void MelaScrollPhysics::run(float range,
                            float value,
                            std::vector<float> snappingPoints)
{
    m_runRange = range;
    ScrollPhysics::run(range, value, snappingPoints);
    if (abs(m_acceleration) > 100)
    {
        m_speed = m_acceleration * 0.16f * 0.16f * 0.1f;
    }
    else
    {
        m_speed = 0;
    }
    if (value < range)
    {
        m_target = range;
    }
    else if (value > 0)
    {
        m_target = 0;
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