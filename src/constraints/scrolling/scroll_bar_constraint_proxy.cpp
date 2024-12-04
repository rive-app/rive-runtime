#include "rive/constraints/scrolling/scroll_bar_constraint.hpp"
#include "rive/constraints/scrolling/scroll_bar_constraint_proxy.hpp"
#include "rive/math/vec2d.hpp"

using namespace rive;

void ThumbDraggableProxy::drag(Vec2D mousePosition)
{
    m_constraint->dragThumb(mousePosition - m_lastPosition);
    m_lastPosition = mousePosition;
}

void ThumbDraggableProxy::startDrag(Vec2D mousePosition)
{
    m_lastPosition = mousePosition;
}

void TrackDraggableProxy::startDrag(Vec2D mousePosition)
{
    m_constraint->hitTrack(mousePosition);
}