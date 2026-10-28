#include "rive/constraints/scrolling/scroll_constraint.hpp"
#include "rive/constraints/scrolling/scroll_physics.hpp"
#include "rive/core_context.hpp"
#include "math.h"

using namespace rive;

long long ScrollPhysics::clockMicros(float timeStamp)
{
  return File::deterministicMode ? (long long)(timeStamp * 1000000.0f)
                                 : steadyMicros();
}

void ScrollPhysics::markTime(float timeStamp)
{
  m_lastTime = clockMicros(timeStamp);
}

void ScrollPhysics::accumulate(Vec2D delta, float timeStamp)
{
  long long now = clockMicros(timeStamp);
  float elapsedSeconds = (float)(now - m_lastTime) / 1000000.0f;
  m_lastTime = now;
  if (elapsedSeconds > 0)
  {
    auto lastSpeed = m_speed;
    m_speed = Vec2D(delta.x / elapsedSeconds, delta.y / elapsedSeconds);
    m_acceleration = Vec2D(
        (lastSpeed.x + m_speed.x) / elapsedSeconds,
        (lastSpeed.y + m_speed.y) / elapsedSeconds);
  }
}

void ScrollPhysics::reset()
{
  m_lastTime = File::deterministicMode ? 0 : steadyMicros();
  m_speed = Vec2D();
  m_acceleration = Vec2D();
  stop();
}

StatusCode ScrollPhysics::import(ImportStack& importStack)
{
  auto backboardImporter = importStack.latest<BackboardImporter>(
      BackboardBase::typeKey);
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