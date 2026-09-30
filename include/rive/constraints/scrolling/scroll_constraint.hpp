#ifndef _RIVE_SCROLL_CONSTRAINT_HPP_
#define _RIVE_SCROLL_CONSTRAINT_HPP_
#include "rive/generated/constraints/scrolling/scroll_constraint_base.hpp"
#include "rive/advance_flags.hpp"
#include "rive/advancing_component.hpp"
#include "rive/constraints/layout_constraint.hpp"
#include "rive/constraints/scrolling/scroll_physics.hpp"
#include "rive/layout_component.hpp"
#include "rive/math/math_types.hpp"
#include "rive/math/transform_components.hpp"
#include <stdio.h>
namespace rive
{
class LayoutNodeProvider;
class ScrollVirtualizer;

class ScrollConstraint : public ScrollConstraintBase,
                         public AdvancingComponent,
                         public LayoutConstraint
{
private:
    // Coordinate space a scroll write was expressed in.
    enum class ScrollSpace : uint8_t
    {
        none,
        percent,
        index,
    };
    // A percent/index write awaiting conversion to an offset.
    struct ScrollAxisIntent
    {
        ScrollSpace space = ScrollSpace::none;
        float value = 0;
    };

    ScrollPhysics* m_physics = nullptr;
    ScrollVirtualizer* m_virtualizer = nullptr;
    std::vector<LayoutNodeProvider*> m_layoutChildren;

    TransformComponents m_componentsA;
    TransformComponents m_componentsB;
    Mat2D m_scrollTransform;

    float m_offsetX = 0;
    float m_offsetY = 0;
    float m_lastFrameOffsetX = 0;
    float m_lastFrameOffsetY = 0;
    int m_childConstraintAppliedCount = 0;

    bool m_isDragging = false;
    bool m_isScrollBarDragging = false;
    bool m_hasListChildren = false;
    // A wheel or trackpad gesture owns this view. Distinct from m_isDragging:
    // no pointer is down, and a phaseless wheel ends by going idle.
    bool m_isScrolling = false;
    float m_scrollIdleSeconds = 0;

    // Unresolved writes per axis; held until layout can convert.
    ScrollAxisIntent m_intentX;
    ScrollAxisIntent m_intentY;

    AABB boundsForFlatIndex(size_t index);
    /// Returns false when layout can't resolve the index yet; out-of-range
    /// indices clamp to the ends.
    bool positionAtIndex(float index, Vec2D& outPosition);
    float indexAtPosition(Vec2D pos);
    float maxOffsetXForPercent();
    float maxOffsetYForPercent();
    bool isBoundsCollapsed(AABB bounds);
    std::vector<Vec2D> collectSnapPoints();
    bool scrollLayoutResolvable(bool isX);
    float clampResolvedOffset(float value, bool isX);
    bool resolveIntent(const ScrollAxisIntent& intent,
                       bool isX,
                       float& outOffset);
    void setIntentX(ScrollAxisIntent intent);
    void setIntentY(ScrollAxisIntent intent);
    void resolveScrollIntents();
    void clearScrollIntents()
    {
        m_intentX.space = ScrollSpace::none;
        m_intentY.space = ScrollSpace::none;
    }

public:
    ~ScrollConstraint();
    void constrain(TransformComponent* component) override;
    std::vector<DraggableProxy*> draggables() override;
    void buildDependencies() override;
    StatusCode import(ImportStack& importStack) override;
    StatusCode onAddedDirty(CoreContext* context) override;
    Core* clone() const override;
    void dragView(Vec2D delta, float timeStamp, bool trackVelocity = true);
    /// Whether wheel and trackpad input may drive this view. interactive
    /// gates every kind of input; wheelInteractive gates the indirect kind.
    bool wheelEnabled() { return interactive() && wheelInteractive(); }
    /// Whether the delta would actually move this view; an edge-stuck one
    /// declines so the gesture chains outward.
    bool canConsume(Vec2D delta);
    /// Whether a precise gesture can stretch this view at its edge.
    bool canStretch(Vec2D delta);
    /// Whether the offset currently sits outside the scrollable range.
    bool isOverscrolled();
    /// The delta as it will actually be applied, after dragMultiplier.
    Vec2D scaledDelta(Vec2D delta);
    /// The wheel path: clamped, and no velocity accumulation, since a detent
    /// never flings and must not feed the velocity passthroughs.
    void scrollBy(Vec2D delta);
    /// True while a wheel or trackpad gesture owns this view.
    bool isScrolling() const { return m_isScrolling; }
    /// True while a pointer drag owns this view.
    bool isDragging() const { return m_isDragging; }
    /// True when this opened a new gesture rather than continuing one.
    bool beginScrollGesture();
    void endScrollGesture();
    /// Resets the idle timer that closes a phaseless gesture.
    void markScrollActivity() { m_scrollIdleSeconds = 0; }
    void runPhysics();
    void constrainChild(LayoutNodeProvider* child) override;
    void addLayoutChild(LayoutNodeProvider* child) override;
    Constraint* constraint() override { return this; }
    void constrainVirtualized(bool force = false);
    bool advanceComponent(float elapsedSeconds,
                          AdvanceFlags flags = AdvanceFlags::Animate |
                                               AdvanceFlags::NewFrame) override;

    ScrollPhysics* physics() const { return m_physics; }
    void physics(ScrollPhysics* physics) { m_physics = physics; }
    /// Pointer drag: primes physics and marks the drag.
    void initPhysics();
    /// Primes the simulation and its velocity clock without claiming a
    /// pointer drag; the scroll path uses this so a wheel gesture ending
    /// cannot end a pointer drag that overlaps it.
    void primePhysics();
    /// Runs the simulation without touching the drag flag.
    void startPhysics();
    /// Re-primes physics when a settle has torn it down mid-gesture. True when
    /// it primed, meaning the velocity clock has no earlier sample.
    bool ensurePhysicsPrimed();
    void stopPhysics();

    void clearVelocity();

    ScrollPhysicsType physicsType() const
    {
        return ScrollPhysicsType(physicsTypeValue());
    }

    bool hasLayoutParent()
    {
        return parent() != nullptr && parent()->is<LayoutComponent>();
    }
    LayoutComponent* content() { return parent()->as<LayoutComponent>(); }
    LayoutComponent* viewport()
    {
        return parent()->parent()->as<LayoutComponent>();
    }
    float contentWidth();
    float contentHeight();
    // Passthrough properties: binds read the live extents directly.
    float computedContentWidth() override
    {
        return hasLayoutParent() ? contentWidth() : 0.0f;
    }
    float computedContentHeight() override
    {
        return hasLayoutParent() ? contentHeight() : 0.0f;
    }
    void setComputedContentWidth(float value) override {}
    void setComputedContentHeight(float value) override {}
    float viewportWidth();
    float viewportHeight();
    float visibleWidthRatio();
    float visibleHeightRatio();
    float minOffsetX();
    float minOffsetY();
    float maxOffsetX();
    float maxOffsetY();
    float clampedOffsetX();
    float clampedOffsetY();
#ifdef TESTING
    // Children constrained since the content last was: one per child per
    // update once they have all run, which is when virtualization kicks in.
    int childConstraintAppliedCount() const
    {
        return m_childConstraintAppliedCount;
    }
#endif

    float offsetX() { return m_offsetX; }
    float offsetY() { return m_offsetY; }
    void offsetX(float value);
    void offsetY(float value);

    void scrollOffsetXChanged() override { offsetX(scrollOffsetX()); }
    void scrollOffsetYChanged() override { offsetY(scrollOffsetY()); }

    float scrollPercentX() override;
    float scrollPercentY() override;
    float scrollIndex() override;
    void setScrollPercentX(float value) override;
    void setScrollPercentY(float value) override;
    void setScrollIndex(float value) override;

    float velocityX() override;
    float velocityY() override;
    void setVelocityX(float value) override {}
    void setVelocityY(float value) override {}

    bool scrollActive() override;
    void setScrollActive(bool value) override {}

    bool isScrollBarDragging() const { return m_isScrollBarDragging; }
    void isScrollBarDragging(bool value)
    {
        if (!m_isScrollBarDragging && value)
        {
            // User interaction supersedes held intents.
            clearScrollIntents();
            // Prime the snapshot so the first advance comparison is
            // meaningful (otherwise a stale value falsely flags motion).
            m_lastFrameOffsetX = scrollOffsetX();
            m_lastFrameOffsetY = scrollOffsetY();
        }
        m_isScrollBarDragging = value;
    }

    size_t scrollItemCount();
    std::vector<LayoutNodeProvider*>& scrollChildren()
    {
        return m_layoutChildren;
    }
    Vec2D gap();
    bool mainAxisIsColumn();

    /// Animate scroll to the target position using the physics settling
    /// animation.
    void scrollToPosition(float targetX, float targetY);

    /// Given a target offset reached by scrolling from current, return the
    /// nearest snap offset in the direction of travel, or target unchanged
    /// if snap() is false, no snap points exist, or none lie in the scroll
    /// direction. The returned snap is at least as far as target — so an
    /// element brought into view by target stays in view.
    Vec2D nearestSnapOffsetInDirection(Vec2D current, Vec2D target);

    /// Get the effective scroll offset X (target if animating, current
    /// otherwise). Use this to check element visibility during rapid focus
    /// changes.
    float effectiveScrollOffsetX();

    /// Get the effective scroll offset Y (target if animating, current
    /// otherwise). Use this to check element visibility during rapid focus
    /// changes.
    float effectiveScrollOffsetY();
};
} // namespace rive

#endif