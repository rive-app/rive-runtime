#include "rive/constraints/scrolling/scroll_bar_constraint.hpp"
#include "rive/constraints/scrolling/scroll_constraint.hpp"
#include "rive/constraints/scrolling/scroll_constraint_proxy.hpp"
#include "rive/math/vec2d.hpp"

using namespace rive;

bool ViewportDraggableProxy::drag(Vec2D mousePosition, float timeStamp)
{
    if (!m_constraint->interactive())
    {
        return false;
    }
    m_constraint->dragView(mousePosition - m_lastPosition, timeStamp);
    m_lastPosition = mousePosition;
    return true;
}

bool ViewportDraggableProxy::startDrag(Vec2D mousePosition, float timeStamp)
{
    if (!m_constraint->interactive())
    {
        return false;
    }
    m_constraint->initPhysics();
    m_lastPosition = mousePosition;
    return true;
}

bool ViewportDraggableProxy::endDrag(Vec2D mousePosition, float timeStamp)
{
    if (!m_constraint->interactive())
    {
        return false;
    }
    m_constraint->runPhysics();
    return true;
}