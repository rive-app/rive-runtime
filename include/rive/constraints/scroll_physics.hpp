#ifndef _RIVE_SCROLL_PHYSICS_HPP_
#define _RIVE_SCROLL_PHYSICS_HPP_
#include "rive/math/math_types.hpp"
#include <chrono>
#include <stdio.h>
#include <vector>
namespace rive
{

class ScrollPhysics
{
private:
    bool m_isRunning = false;
    long long m_lastTime =
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch())
            .count();

protected:
    float m_speed = 0;
    float m_acceleration = 0;

public:
    bool isRunning() { return m_isRunning; }
    virtual float clamp(float range, float value) { return 0; };
    virtual float advance(float elapsedSeconds) { return 0; };
    virtual void accumulate(float delta)
    {
        auto now = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::microseconds>(
                      now.time_since_epoch())
                      .count();
        float elapsedSeconds = (ms - m_lastTime) / 1000000.0f;
        auto lastSpeed = m_speed;
        m_speed = delta / elapsedSeconds;
        m_acceleration = (lastSpeed - m_speed) / elapsedSeconds;
        m_lastTime = ms;
    }
    virtual void run(float range,
                     float value,
                     std::vector<float> snappingPoints)
    {
        m_isRunning = true;
    }
    virtual void stop() { m_isRunning = false; }
};

class ClampedScrollPhysics : public ScrollPhysics
{
private:
    float m_value = 0;

public:
    float advance(float elapsedSeconds) override
    {
        stop();
        return m_value;
    }
    void run(float range,
             float value,
             std::vector<float> snappingPoints) override
    {
        ScrollPhysics::run(range, value, snappingPoints);
        m_value = clamp(range, value);
    }
    float clamp(float range, float value) override
    {
        return math::clamp(value, range, 0);
    }
};

class MelaScrollPhysics : public ScrollPhysics
{
private:
    float m_friction = 8.0;
    float m_target = 0;
    float m_current = 0;
    float m_speed = 0;
    float m_runRange = 0;
    float m_activeClampFactor = 0.66f;

public:
    float advance(float elapsedSeconds) override;
    float clamp(float range, float value) override;
    void run(float range,
             float value,
             std::vector<float> snappingPoints) override;
};
} // namespace rive

#endif