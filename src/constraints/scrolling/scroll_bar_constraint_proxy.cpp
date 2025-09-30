#include "rive/constraints/scrolling/scroll_bar_constraint.hpp"
#include "rive/constraints/scrolling/scroll_bar_constraint_proxy.hpp"
#include "rive/math/vec2d.hpp"

using namespace rive;

bool ThumbDraggableProxy::drag(Vec2D mousePosition, float timeStamp)
{
    m_constraint->dragThumb(mousePosition - m_lastPosition);
    m_lastPosition = mousePosition;
    return true;
}

bool ThumbDraggableProxy::startDrag(Vec2D mousePosition, float timeStamp)
{
    m_lastPosition = mousePosition;
    return true;
}

bool TrackDraggableProxy::startDrag(Vec2D mousePosition, float timeStamp)
{
    m_constraint->hitTrack(mousePosition);
    return true;
}