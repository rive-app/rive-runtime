#include "rive/constraints/scrolling/scroll_bar_constraint.hpp"
#include "rive/constraints/scrolling/scroll_constraint.hpp"
#include "rive/constraints/scrolling/scroll_constraint_proxy.hpp"
#include "rive/math/vec2d.hpp"

using namespace rive;

void ViewportDraggableProxy::drag(Vec2D mousePosition, float timeStamp)
{
    m_constraint->dragView(mousePosition - m_lastPosition, timeStamp);
    m_lastPosition = mousePosition;
}

void ViewportDraggableProxy::startDrag(Vec2D mousePosition, float timeStamp)
{
    m_constraint->initPhysics();
    m_lastPosition = mousePosition;
}

void ViewportDraggableProxy::endDrag(Vec2D mousePosition, float timeStamp)
{
    m_constraint->runPhysics();
}