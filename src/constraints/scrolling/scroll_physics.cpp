#include "rive/constraints/scrolling/scroll_constraint.hpp"
#include "rive/constraints/scrolling/scroll_physics.hpp"
#include "rive/core_context.hpp"
#include "math.h"

using namespace rive;

void ScrollPhysics::accumulate(Vec2D delta, float timeStamp)
{
    float elapsedSeconds = 0;
    if (File::deterministicMode)
    {
        auto now = timeStamp;
        elapsedSeconds = now - m_lastTime;
        m_lastTime = now;
    }
    else
    {
        auto now = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::microseconds>(
                      now.time_since_epoch())
                      .count();
        elapsedSeconds = (float)(ms - m_lastTime) / 1000000.0f;
        m_lastTime = ms;
    }
    if (elapsedSeconds > 0)
    {
        auto lastSpeed = m_speed;
        m_speed = Vec2D(delta.x / elapsedSeconds, delta.y / elapsedSeconds);
        m_acceleration = Vec2D((lastSpeed.x + m_speed.x) / elapsedSeconds,
                               (lastSpeed.y + m_speed.y) / elapsedSeconds);
    }
}

void ScrollPhysics::reset()
{
    if (File::deterministicMode)
    {
        m_lastTime = 0;
    }
    else
    {
        m_lastTime =
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch())
                .count();
    }
    m_acceleration = Vec2D();
    stop();
}

StatusCode ScrollPhysics::import(ImportStack& importStack)
{
    auto backboardImporter =
        importStack.latest<BackboardImporter>(BackboardBase::typeKey);
    if (backboardImporter != nullptr)
    {
        backboardImporter->addPhysics(this);
    }
    else
    {
        return StatusCode::MissingObject;
    }
    return StatusCode::Ok;
}