#include "rive/constraints/draggable_constraint.hpp"
#include "rive/constraints/scrolling/scroll_bar_constraint.hpp"
#include "rive/constraints/scrolling/scroll_bar_constraint_proxy.hpp"
#include "rive/constraints/transform_constraint.hpp"
#include "rive/core_context.hpp"
#include "rive/layout_component.hpp"
#include "rive/transform_component.hpp"
#include "rive/math/mat2d.hpp"

using namespace rive;

float ScrollBarConstraint::computedThumbWidth()
{
    if (autoSize() && m_scrollConstraint != nullptr)
    {
        return track()->innerWidth() * m_scrollConstraint->visibleWidthRatio();
    }
    return thumb()->layoutWidth();
}

float ScrollBarConstraint::computedThumbHeight()
{
    if (autoSize() && m_scrollConstraint != nullptr)
    {
        return track()->innerHeight() *
               m_scrollConstraint->visibleHeightRatio();
    }
    return thumb()->layoutHeight();
}

std::vector<DraggableProxy*> ScrollBarConstraint::draggables()
{
    std::vector<DraggableProxy*> items;
    if (parent()->is<LayoutComponent>())
    {
        items.push_back(
            new ThumbDraggableProxy(this,
                                    parent()->as<LayoutComponent>()->proxy()));
    }
    if (parent()->parent() != nullptr &&
        parent()->parent()->is<LayoutComponent>())
    {
        items.push_back(new TrackDraggableProxy(
            this,
            parent()->parent()->as<LayoutComponent>()->proxy()));
    }
    return items;
}

void ScrollBarConstraint::constrain(TransformComponent* component)
{
    if (m_scrollConstraint == nullptr || track() == nullptr ||
        thumb() == nullptr)
    {
        return;
    }
    float thumbOffsetX = 0;
    float thumbOffsetY = 0;
    if (constrainsHorizontal())
    {
        auto innerWidth = track()->innerWidth();
        auto thumbWidth = computedThumbWidth();
        auto maxThumbOffset = innerWidth - thumbWidth;
        thumbOffsetX = (m_scrollConstraint->maxOffsetX() == 0)
                           ? 0
                           : m_scrollConstraint->clampedOffsetX() /
                                 m_scrollConstraint->maxOffsetX() *
                                 maxThumbOffset;
        if (thumbOffsetX < 0)
        {
            thumbWidth += thumbOffsetX;
            thumbOffsetX = 0;
        }
        else if (thumbOffsetX > maxThumbOffset)
        {
            thumbWidth -= thumbOffsetX - maxThumbOffset;
            thumbOffsetX = autoSize() ? thumbOffsetX : maxThumbOffset;
        }
        if (autoSize())
        {
            thumb()->forcedWidth(thumbWidth);
        }
    }
    if (constrainsVertical())
    {
        auto innerHeight = track()->innerHeight();
        auto thumbHeight = computedThumbHeight();
        auto maxThumbOffset = innerHeight - thumbHeight;
        thumbOffsetY = (m_scrollConstraint->maxOffsetY() == 0)
                           ? 0
                           : m_scrollConstraint->clampedOffsetY() /
                                 m_scrollConstraint->maxOffsetY() *
                                 maxThumbOffset;
        if (thumbOffsetY < 0)
        {
            thumbHeight += thumbOffsetY;
            thumbOffsetY = 0;
        }
        else if (thumbOffsetY > maxThumbOffset)
        {
            thumbHeight -= thumbOffsetY - maxThumbOffset;
            thumbOffsetY = autoSize() ? thumbOffsetY : maxThumbOffset;
        }
        if (autoSize())
        {
            thumb()->forcedHeight(thumbHeight);
        }
    }
    auto targetTransform =
        Mat2D::multiply(component->worldTransform(),
                        Mat2D::fromTranslate(thumbOffsetX, thumbOffsetY));
    TransformConstraint::constrainWorld(component,
                                        component->worldTransform(),
                                        m_componentsA,
                                        targetTransform,
                                        m_componentsB,
                                        strength());
}

void ScrollBarConstraint::buildDependencies()
{
    m_scrollConstraint->addDependent(this);
    Super::buildDependencies();
}

StatusCode ScrollBarConstraint::onAddedDirty(CoreContext* context)
{
    StatusCode result = Super::onAddedDirty(context);
    if (result != StatusCode::Ok)
    {
        return result;
    }
    auto coreObject = context->resolve(scrollConstraintId());
    if (coreObject == nullptr || !coreObject->is<ScrollConstraint>())
    {
        return StatusCode::MissingObject;
    }
    m_scrollConstraint = static_cast<ScrollConstraint*>(coreObject);
    return StatusCode::Ok;
}

void ScrollBarConstraint::hitTrack(Vec2D worldPosition)
{
    if (m_scrollConstraint == nullptr || track() == nullptr)
    {
        return;
    }
    Mat2D inverseWorld;
    if (!track()->worldTransform().invert(&inverseWorld))
    {
        return;
    }
    auto localPosition = inverseWorld * worldPosition;
    if (constrainsHorizontal())
    {
        localPosition.x -= track()->paddingLeft();
        auto innerWidth = track()->innerWidth();
        auto thumbWidth = computedThumbWidth();
        auto trackRange = innerWidth - thumbWidth;
        auto maxOffsetX = m_scrollConstraint->maxOffsetX();
        m_scrollConstraint->scrollOffsetX(
            math::clamp(localPosition.x / trackRange * maxOffsetX,
                        maxOffsetX,
                        0));
    }
    if (constrainsVertical())
    {
        localPosition.y -= track()->paddingTop();
        auto innerHeight = track()->innerHeight();
        auto thumbHeight = computedThumbHeight();
        auto trackRange = innerHeight - thumbHeight;
        auto maxOffsetY = m_scrollConstraint->maxOffsetY();
        m_scrollConstraint->scrollOffsetY(
            math::clamp(localPosition.y / trackRange * maxOffsetY,
                        maxOffsetY,
                        0));
    }
}

void ScrollBarConstraint::dragThumb(Vec2D delta)
{
    if (m_scrollConstraint == nullptr || thumb() == nullptr ||
        track() == nullptr)
    {
        return;
    }
    if (constrainsHorizontal())
    {
        auto innerWidth = track()->innerWidth();
        auto thumbWidth = computedThumbWidth();
        if (autoSize())
        {
            thumb()->forcedWidth(thumbWidth);
        }

        auto trackRange = innerWidth - thumbWidth;
        auto maxOffsetX = m_scrollConstraint->maxOffsetX();
        auto thumbOffset =
            (m_scrollConstraint->offsetX() / maxOffsetX * trackRange) + delta.x;
        m_scrollConstraint->scrollOffsetX(
            math::clamp((thumbOffset / trackRange * maxOffsetX),
                        maxOffsetX,
                        0));
    }
    if (constrainsVertical())
    {
        auto innerHeight = track()->innerHeight();
        auto thumbHeight = computedThumbHeight();
        if (autoSize())
        {
            thumb()->forcedHeight(thumbHeight);
        }

        auto trackRange = innerHeight - thumbHeight;
        auto maxOffsetY = m_scrollConstraint->maxOffsetY();
        auto thumbOffset =
            (m_scrollConstraint->offsetY() / maxOffsetY * trackRange) + delta.y;
        m_scrollConstraint->scrollOffsetY(
            math::clamp((thumbOffset / trackRange * maxOffsetY),
                        maxOffsetY,
                        0));
    }
}

bool ScrollBarConstraint::validate(CoreContext* context)
{
    auto coreObject = context->resolve(scrollConstraintId());
    return coreObject != nullptr && coreObject->is<ScrollConstraint>();
}