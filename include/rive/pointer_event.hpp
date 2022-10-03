#ifndef _RIVE_POINTER_EVENT_HPP_
#define _RIVE_POINTER_EVENT_HPP_

#include "rive/math/vec2d.hpp"

namespace rive
{

enum class PointerEventType
{
    down, // The button has gone from up to down
    move, // The pointer's position has changed
    up,   // The button has gone from down to up
};

struct PointerEvent
{
    PointerEventType m_Type;
    Vec2D m_Position;
    int m_PointerIndex;

    // add more fields as needed
};

} // namespace rive

#endif
