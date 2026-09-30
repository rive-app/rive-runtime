#include "rive/constraints/scrolling/scroll_bar_constraint.hpp"
#include "rive/constraints/scrolling/scroll_constraint.hpp"
#include "rive/constraints/scrolling/scroll_constraint_proxy.hpp"
#include "rive/constraints/scrolling/scroll_physics.hpp"
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
bool ViewportDraggableProxy::wantsScroll(const ScrollEvent& event)
{
    // A zero-delta event can't say which axis the gesture is on, so every
    // branch tests the delta and it declines; the first directional one picks.
    if (event.phase == ScrollPhase::inertiaCancel)
    {
        // Only matters to a view already coasting.
        return m_constraint->isScrolling() ||
               (m_constraint->physics() != nullptr &&
                m_constraint->physics()->isRunning());
    }
    if (event.precise)
    {
        // An elastic view stretches at its edge instead of chaining.
        return m_constraint->canConsume(event.delta) ||
               m_constraint->canStretch(event.delta);
    }
    return m_constraint->canConsume(event.delta);
}

bool ViewportDraggableProxy::acceptsScroll()
{
    return m_constraint->wheelEnabled();
}

void ViewportDraggableProxy::cancelScroll()
{
    m_constraint->stopPhysics();
    m_constraint->clearVelocity();
    m_constraint->endScrollGesture();
}

bool ViewportDraggableProxy::isScrollGestureActive()
{
    return m_constraint->isScrolling();
}

bool ViewportDraggableProxy::scroll(const ScrollEvent& event, float timeStamp)
{
    if (!m_constraint->wheelEnabled())
    {
        return false;
    }
    switch (event.phase)
    {
        case ScrollPhase::inertiaCancel:
            m_constraint->stopPhysics();
            m_constraint->clearVelocity();
            m_constraint->endScrollGesture();
            return true;
        case ScrollPhase::begin:
            m_constraint->beginScrollGesture();
            m_constraint->primePhysics();
            return true;
        case ScrollPhase::update:
            if (event.precise)
            {
                // Re-prime for a new gesture, or to take the view back from a
                // running simulation before it overwrites this drag.
                bool started = m_constraint->beginScrollGesture();
                bool running = m_constraint->physics() != nullptr &&
                               m_constraint->physics()->isRunning();
                bool primed = started || running;
                if (primed)
                {
                    m_constraint->primePhysics();
                }
                else
                {
                    // A settle can strip the helpers mid-gesture.
                    primed = m_constraint->ensurePhysicsPrimed();
                }
                // A primed clock has no earlier sample, so measuring this
                // delta against it would divide by microseconds.
                m_constraint->dragView(event.delta, timeStamp, !primed);
            }
            else
            {
                // A detent cancels inertia rather than being overwritten by
                // it on the next advance.
                if (m_constraint->physics() != nullptr &&
                    m_constraint->physics()->isRunning())
                {
                    m_constraint->stopPhysics();
                }
                m_constraint->beginScrollGesture();
                m_constraint->scrollBy(event.delta);
            }
            return true;
        case ScrollPhase::momentum:
        {
            bool settling = m_constraint->physics() != nullptr &&
                            m_constraint->physics()->isRunning();
            if (m_constraint->isOverscrolled() || settling)
            {
                // Inertia at the edge bounces now rather than stretching for
                // the whole tail. Stays live so the latch holds.
                if (!settling)
                {
                    m_constraint->primePhysics();
                    m_constraint->startPhysics();
                }
                m_constraint->markScrollActivity();
                return true;
            }
            // Platform momentum lands as-is; flinging on top would double it.
            m_constraint->beginScrollGesture();
            m_constraint->scrollBy(event.delta);
            return true;
        }
        case ScrollPhase::end:
            m_constraint->startPhysics();
            m_constraint->endScrollGesture();
            return true;
    }
    return false;
}
