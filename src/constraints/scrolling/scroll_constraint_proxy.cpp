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
    auto deltaPosition = mousePosition - m_lastPosition;
    if (!m_isDragging)
    {
        switch (m_constraint->direction())
        {
            case DraggableConstraintDirection::vertical:
            {
                if (std::abs(deltaPosition.y) > m_constraint->threshold())
                {
                    m_isDragging = true;
                }
                else
                {
                    return false;
                }
            }
            break;
            case DraggableConstraintDirection::horizontal:
            {
                if (std::abs(deltaPosition.x) > m_constraint->threshold())
                {
                    m_isDragging = true;
                }
                else
                {
                    return false;
                }
            }
            break;
            case DraggableConstraintDirection::all:
            {
                if (deltaPosition.length() > m_constraint->threshold())
                {
                    m_isDragging = true;
                }
                else
                {
                    return false;
                }
            }
            break;
        }
    }
    m_constraint->dragView(deltaPosition, timeStamp);
    m_lastPosition = mousePosition;
    return true;
}

bool ViewportDraggableProxy::startDrag(Vec2D mousePosition, float timeStamp)
{
    if (!m_constraint->interactive())
    {
        return false;
    }
    m_isDragging = false;
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