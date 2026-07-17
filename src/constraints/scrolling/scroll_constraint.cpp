#include "rive/constraints/scrolling/scroll_constraint.hpp"
#include "rive/artboard_component_list.hpp"
#include "rive/constraints/scrolling/scroll_constraint_proxy.hpp"
#include "rive/constraints/scrolling/scroll_virtualizer.hpp"
#include "rive/constraints/transform_constraint.hpp"
#include "rive/core_context.hpp"
#include "rive/layout/layout_node_provider.hpp"
#include "rive/transform_component.hpp"
#include "rive/virtualizing_component.hpp"
#include "rive/math/mat2d.hpp"

using namespace rive;

ScrollConstraint::~ScrollConstraint()
{
    if (m_virtualizer != nullptr)
    {
        delete m_virtualizer;
        m_virtualizer = nullptr;
    }
    m_layoutChildren.clear();
    delete m_physics;
}

float ScrollConstraint::contentWidth()
{
    if (virtualize() && !mainAxisIsColumn())
    {
        auto contentSize = 0.0f;
        for (auto child : scrollChildren())
        {
            if (child == nullptr)
            {
                continue;
            }
            contentSize += child->layoutBounds().width();
        }
        auto lenOffset = infinite() ? 0 : 1;
        return contentSize + gap().x * (scrollChildren().size() - lenOffset);
    }
    return content()->layoutWidth();
}

float ScrollConstraint::contentHeight()
{
    if (virtualize() && mainAxisIsColumn())
    {
        auto contentSize = 0.0f;
        for (auto child : scrollChildren())
        {
            if (child == nullptr)
            {
                continue;
            }
            contentSize += child->layoutBounds().height();
        }
        auto lenOffset = infinite() ? 0 : 1;
        return contentSize + gap().y * (scrollChildren().size() - lenOffset);
    }
    return content()->layoutHeight();
}

float ScrollConstraint::viewportWidth()
{
    return direction() == DraggableConstraintDirection::vertical
               ? viewport()->layoutWidth()
               : std::max(0.0f,
                          viewport()->layoutWidth() - content()->layoutX());
}
float ScrollConstraint::viewportHeight()
{
    return direction() == DraggableConstraintDirection::horizontal
               ? viewport()->layoutHeight()
               : std::max(0.0f,
                          viewport()->layoutHeight() - content()->layoutY());
}
float ScrollConstraint::visibleWidthRatio()
{
    if (contentWidth() == 0)
    {
        return 1;
    }
    return std::min(1.0f, viewportWidth() / contentWidth());
}
float ScrollConstraint::visibleHeightRatio()
{
    if (contentHeight() == 0)
    {
        return 1;
    }
    return std::min(1.0f, viewportHeight() / contentHeight());
}
float ScrollConstraint::minOffsetX()
{
    if (infinite() && !mainAxisIsColumn())
    {
        return std::numeric_limits<float>::infinity();
    }
    return 0;
}
float ScrollConstraint::minOffsetY()
{
    if (infinite() && mainAxisIsColumn())
    {
        return std::numeric_limits<float>::infinity();
    }
    return 0;
}
float ScrollConstraint::maxOffsetX()
{
    if (infinite() && !mainAxisIsColumn())
    {
        return -std::numeric_limits<float>::infinity();
    }
    return std::min(0.0f,
                    viewportWidth() - contentWidth() -
                        viewport()->paddingRight());
}
float ScrollConstraint::maxOffsetY()
{
    if (infinite() && mainAxisIsColumn())
    {
        return -std::numeric_limits<float>::infinity();
    }
    return std::min(0.0f,
                    viewportHeight() - contentHeight() -
                        viewport()->paddingBottom());
}
float ScrollConstraint::clampedOffsetX()
{
    if (infinite())
    {
        return offsetX();
    }
    if (maxOffsetX() > 0)
    {
        return 0;
    }
    if (m_physics != nullptr && m_physics->enabled())
    {
        return m_physics
            ->clamp(Vec2D(maxOffsetX(), maxOffsetY()),
                    Vec2D(minOffsetX(), minOffsetY()),
                    Vec2D(m_offsetX, m_offsetY))
            .x;
    }
    return math::clamp(m_offsetX, maxOffsetX(), 0);
}
float ScrollConstraint::clampedOffsetY()
{
    if (infinite())
    {
        return offsetY();
    }
    if (maxOffsetY() > 0)
    {
        return 0;
    }
    if (m_physics != nullptr && m_physics->enabled())
    {
        return m_physics
            ->clamp(Vec2D(maxOffsetX(), maxOffsetY()),
                    Vec2D(minOffsetX(), minOffsetY()),
                    Vec2D(m_offsetX, m_offsetY))
            .y;
    }
    return math::clamp(m_offsetY, maxOffsetY(), 0);
}

void ScrollConstraint::offsetX(float value)
{
    if (m_offsetX == value)
    {
        return;
    }
    m_offsetX = value;
    content()->markWorldTransformDirty();
}
void ScrollConstraint::offsetY(float value)
{
    if (m_offsetY == value)
    {
        return;
    }
    m_offsetY = value;
    content()->markWorldTransformDirty();
}

bool ScrollConstraint::mainAxisIsColumn()
{
    return content() != nullptr && content()->mainAxisIsColumn();
}

void ScrollConstraint::constrain(TransformComponent* component)
{
    resolveScrollIntents();
    m_scrollTransform =
        Mat2D::fromTranslate(constrainsHorizontal() ? clampedOffsetX() : 0,
                             constrainsVertical() ? clampedOffsetY() : 0);
    m_childConstraintAppliedCount = 0;
}

void ScrollConstraint::constrainChild(LayoutNodeProvider* child)
{
    auto component = child->transformComponent();
    if (component == nullptr)
    {
        return;
    }
    auto targetTransform =
        Mat2D::multiply(component->worldTransform(), m_scrollTransform);
    TransformConstraint::constrainWorld(component,
                                        component->worldTransform(),
                                        m_componentsA,
                                        targetTransform,
                                        m_componentsB,
                                        strength());
    m_childConstraintAppliedCount += 1;
    constrainVirtualized();
}

void ScrollConstraint::constrainVirtualized(bool force)
{
    if (virtualize() && m_virtualizer != nullptr)
    {
        auto children = scrollChildren();
        if (m_childConstraintAppliedCount < children.size() && !force)
        {
            return;
        }
        auto isColumn = mainAxisIsColumn();
        auto direction = isColumn ? VirtualizedDirection::vertical
                                  : VirtualizedDirection::horizontal;
        auto offset = isColumn ? clampedOffsetY() : clampedOffsetX();
        m_virtualizer->constrain(this, children, offset, direction);
    }
}

void ScrollConstraint::addLayoutChild(LayoutNodeProvider* child)
{
    m_layoutChildren.push_back(child);
}

void ScrollConstraint::dragView(Vec2D delta, float timeStamp)
{
    // Scale once so the inertial release phase matches the drag feel.
    Vec2D scaledDelta(delta.x * dragMultiplier(), delta.y * dragMultiplier());
    if (m_physics != nullptr)
    {
        m_physics->accumulate(scaledDelta, timeStamp);
    }
    scrollOffsetX(offsetX() + scaledDelta.x);
    scrollOffsetY(offsetY() + scaledDelta.y);
}

std::vector<Vec2D> ScrollConstraint::collectSnapPoints()
{
    std::vector<Vec2D> snappingPoints;
    if (content() == nullptr)
    {
        return snappingPoints;
    }
    for (auto child : scrollChildren())
    {
        if (child == nullptr)
        {
            continue;
        }
        int nodeCount = (int)child->numLayoutNodes();
        for (int j = 0; j < nodeCount; j++)
        {
            auto bounds = child->layoutBoundsForNode(j);
            if (isBoundsCollapsed(bounds))
            {
                continue;
            }
            snappingPoints.push_back(Vec2D(bounds.left(), bounds.top()));
        }
    }
    return snappingPoints;
}

void ScrollConstraint::runPhysics()
{
    m_isDragging = false;
    std::vector<Vec2D> snappingPoints =
        snap() ? collectSnapPoints() : std::vector<Vec2D>();
    if (m_physics != nullptr)
    {
        m_physics->run(Vec2D(maxOffsetX(), maxOffsetY()),
                       Vec2D(minOffsetX(), minOffsetY()),
                       Vec2D(offsetX(), offsetY()),
                       snappingPoints,
                       mainAxisIsColumn() ? contentHeight() : contentWidth(),
                       mainAxisIsColumn() ? viewportHeight() : viewportWidth());
    }
}

bool ScrollConstraint::advanceComponent(float elapsedSeconds,
                                        AdvanceFlags flags)
{
    if ((flags & AdvanceFlags::AdvanceNested) != AdvanceFlags::AdvanceNested ||
        isCollapsed())
    {
        // offsetX(0);
        // offsetY(0);
        return false;
    }
    if (m_physics == nullptr)
    {
        return false;
    }
    if (m_physics->isRunning())
    {
        auto offset = m_physics->advance(elapsedSeconds);
        scrollOffsetX(offset.x);
        scrollOffsetY(offset.y);
    }
    // Detect a paused pointer during drag: if scrollOffset hasn't changed
    // since the last new-frame advance, accumulate() didn't fire and speed
    // is stale.
    if ((flags & AdvanceFlags::NewFrame) == AdvanceFlags::NewFrame)
    {
        bool hasMoved = scrollOffsetX() != m_lastFrameOffsetX ||
                        scrollOffsetY() != m_lastFrameOffsetY;
        if ((m_isScrollBarDragging || m_isDragging) && !hasMoved)
        {
            clearVelocity();
        }
        m_lastFrameOffsetX = scrollOffsetX();
        m_lastFrameOffsetY = scrollOffsetY();
    }
    // Keep advancing while a drag is active so the next new-frame advance can
    // detect a paused pointer and clear velocity when the pointer holds still.
    return m_physics->enabled() || m_isScrollBarDragging || m_isDragging;
}

std::vector<DraggableProxy*> ScrollConstraint::draggables()
{
    std::vector<DraggableProxy*> items;
    items.push_back(new ViewportDraggableProxy(this, viewport()->proxy()));
    return items;
}

void ScrollConstraint::buildDependencies()
{
    Super::buildDependencies();
    m_hasListChildren = false;
    for (auto child : content()->children())
    {
        auto layout = LayoutNodeProvider::from(child);
        if (layout != nullptr)
        {
            addDependent(child);
            layout->addLayoutConstraint(static_cast<LayoutConstraint*>(this));
        }
        if (child->is<ArtboardComponentList>())
        {
            m_hasListChildren = true;
        }
    }
}

Core* ScrollConstraint::clone() const
{
    auto cloned = ScrollConstraintBase::clone();
    if (physics() != nullptr)
    {
        auto constraint = cloned->as<ScrollConstraint>();
        auto clonedPhysics = physics()->clone()->as<ScrollPhysics>();
        constraint->physics(clonedPhysics);
    }
    return cloned;
}

StatusCode ScrollConstraint::import(ImportStack& importStack)
{
    auto backboardImporter =
        importStack.latest<BackboardImporter>(BackboardBase::typeKey);
    if (backboardImporter != nullptr)
    {
        std::vector<ScrollPhysics*> physicsObjects =
            backboardImporter->physics();
        if (physicsId() != -1 && physicsId() < physicsObjects.size())
        {
            auto phys = physicsObjects[physicsId()];
            if (phys != nullptr)
            {
                auto cloned = phys->clone()->as<ScrollPhysics>();
                physics(cloned);
            }
        }
    }
    else
    {
        return StatusCode::MissingObject;
    }
    return Super::import(importStack);
}

StatusCode ScrollConstraint::onAddedDirty(CoreContext* context)
{
    StatusCode result = Super::onAddedDirty(context);
    if (virtualize())
    {
        m_virtualizer = new ScrollVirtualizer();
    }
    offsetX(scrollOffsetX());
    offsetY(scrollOffsetY());
    return result;
}

void ScrollConstraint::initPhysics()
{
    m_isDragging = true;
    // User interaction supersedes held intents.
    clearScrollIntents();
    m_lastFrameOffsetX = scrollOffsetX();
    m_lastFrameOffsetY = scrollOffsetY();
    if (m_physics != nullptr)
    {
        m_physics->prepare(direction());
    }
}

void ScrollConstraint::stopPhysics()
{
    if (m_physics != nullptr)
    {
        m_physics->reset();
    }
}

void ScrollConstraint::clearVelocity()
{
    if (m_physics != nullptr)
    {
        m_physics->clearVelocity();
    }
}

float ScrollConstraint::maxOffsetXForPercent()
{
    return infinite() ? contentWidth() : maxOffsetX();
}

float ScrollConstraint::maxOffsetYForPercent()
{
    return infinite() ? contentHeight() : maxOffsetY();
}

float ScrollConstraint::velocityX()
{
    return m_physics != nullptr ? m_physics->speed().x : 0.0f;
}

float ScrollConstraint::velocityY()
{
    return m_physics != nullptr ? m_physics->speed().y : 0.0f;
}

bool ScrollConstraint::scrollActive()
{
    return m_isDragging || m_isScrollBarDragging ||
           (m_physics != nullptr && m_physics->isRunning());
}

float ScrollConstraint::scrollPercentX()
{
    if (m_intentX.space == ScrollSpace::percent)
    {
        return m_intentX.value;
    }
    return maxOffsetX() != 0 ? scrollOffsetX() / maxOffsetXForPercent() : 0;
}

float ScrollConstraint::scrollPercentY()
{
    if (m_intentY.space == ScrollSpace::percent)
    {
        return m_intentY.value;
    }
    return maxOffsetY() != 0 ? scrollOffsetY() / maxOffsetYForPercent() : 0;
}

float ScrollConstraint::scrollIndex()
{
    const ScrollAxisIntent& intent =
        constrainsHorizontal() ? m_intentX : m_intentY;
    if (intent.space == ScrollSpace::index)
    {
        return intent.value;
    }
    return indexAtPosition(Vec2D(scrollOffsetX(), scrollOffsetY()));
}

void ScrollConstraint::setScrollPercentX(float value)
{
    if (m_isDragging)
    {
        return;
    }
    stopPhysics();
    setIntentX({ScrollSpace::percent, value});
}

void ScrollConstraint::setScrollPercentY(float value)
{
    if (m_isDragging)
    {
        return;
    }
    stopPhysics();
    setIntentY({ScrollSpace::percent, value});
}

void ScrollConstraint::setScrollIndex(float value)
{
    if (m_isDragging)
    {
        return;
    }
    stopPhysics();
    if (constrainsHorizontal())
    {
        setIntentX({ScrollSpace::index, value});
    }
    if (constrainsVertical())
    {
        setIntentY({ScrollSpace::index, value});
    }
}

// False while hidden or before the first layout pass on the given axis.
bool ScrollConstraint::scrollLayoutResolvable(bool isX)
{
    auto vp = viewport();
    if (vp == nullptr)
    {
        return false;
    }
    return isX ? vp->layoutWidth() > 0 : vp->layoutHeight() > 0;
}

// Resolved offsets clamp to the scrollable range.
float ScrollConstraint::clampResolvedOffset(float value, bool isX)
{
    if (infinite())
    {
        return value;
    }
    return math::clamp(value, isX ? maxOffsetX() : maxOffsetY(), 0.0f);
}

// Convert an intent to an offset; false if layout can't resolve yet.
bool ScrollConstraint::resolveIntent(const ScrollAxisIntent& intent,
                                     bool isX,
                                     float& outOffset)
{
    // Non-finite indices resolve to the origin without needing layout.
    if (intent.space == ScrollSpace::index &&
        (std::isnan(intent.value) ||
         (infinite() && !std::isfinite(intent.value))))
    {
        outOffset = 0;
        return true;
    }
    // Gate both spaces: virtualized lists compute item bounds from authored
    // sizes, which resolve even while the viewport layout is collapsed.
    if (!scrollLayoutResolvable(isX))
    {
        return false;
    }
    switch (intent.space)
    {
        case ScrollSpace::percent:
        {
            float contentSize = isX ? contentWidth() : contentHeight();
            if (contentSize <= 0)
            {
                // No measured content — can't resolve.
                return false;
            }
            float maxForPercent =
                isX ? maxOffsetXForPercent() : maxOffsetYForPercent();
            outOffset = clampResolvedOffset(intent.value * maxForPercent, isX);
            return true;
        }
        case ScrollSpace::index:
        {
            Vec2D to;
            if (!positionAtIndex(intent.value, to))
            {
                return false;
            }
            outOffset = clampResolvedOffset(isX ? to.x : to.y, isX);
            return true;
        }
        case ScrollSpace::none:
            break;
    }
    return false;
}

// Resolve eagerly; unresolved intents are held and retried in constrain().
void ScrollConstraint::setIntentX(ScrollAxisIntent intent)
{
    float resolved;
    if (!resolveIntent(intent, true, resolved))
    {
        m_intentX = intent;
        return;
    }
    m_intentX.space = ScrollSpace::none;
    scrollOffsetX(resolved);
}

void ScrollConstraint::setIntentY(ScrollAxisIntent intent)
{
    float resolved;
    if (!resolveIntent(intent, false, resolved))
    {
        m_intentY = intent;
        return;
    }
    m_intentY.space = ScrollSpace::none;
    scrollOffsetY(resolved);
}

// Retry held intents; called from constrain() where layout is fresh.
void ScrollConstraint::resolveScrollIntents()
{
    if (m_intentX.space != ScrollSpace::none)
    {
        setIntentX(m_intentX);
    }
    if (m_intentY.space != ScrollSpace::none)
    {
        setIntentY(m_intentY);
    }
}

bool ScrollConstraint::positionAtIndex(float index, Vec2D& outPosition)
{
    if (std::isnan(index) || (infinite() && !std::isfinite(index)))
    {
        outPosition = Vec2D();
        return true;
    }
    auto count = scrollItemCount();
    if (content() == nullptr || count == 0)
    {
        return false;
    }
    Vec2D contentGap = gap();
    float normalizedIndex;
    if (infinite())
    {
        normalizedIndex = std::fmod(index, (float)count);
        if (normalizedIndex < 0)
        {
            normalizedIndex += (float)count;
        }
    }
    else
    {
        normalizedIndex = std::max(index, 0.0f);
        // Past the last item: target the far end (clamps to max scroll).
        if (normalizedIndex >= (float)count)
        {
            if (contentWidth() <= 0 && contentHeight() <= 0)
            {
                // No measured content — can't resolve.
                return false;
            }
            outPosition = Vec2D(-contentWidth(), -contentHeight());
            return true;
        }
    }
    float floorIndex = std::floor(normalizedIndex);
    float mod = normalizedIndex - floorIndex;
    uint32_t targetIndex = (uint32_t)floorIndex;

    // No list children: O(1) direct index into scrollChildren.
    if (!m_hasListChildren)
    {
        // Target is visible — return its position with fractional offset.
        auto bounds = boundsForFlatIndex(targetIndex);
        if (!isBoundsCollapsed(bounds))
        {
            outPosition =
                Vec2D(-bounds.left() - (bounds.width() + contentGap.x) * mod,
                      -bounds.top() - (bounds.height() + contentGap.y) * mod);
            return true;
        }
        // Target is collapsed — walk forward to next visible item.
        for (uint32_t k = targetIndex + 1; k < (uint32_t)count; k++)
        {
            auto b = boundsForFlatIndex(k);
            if (!isBoundsCollapsed(b))
            {
                outPosition = Vec2D(-b.left(), -b.top());
                return true;
            }
        }
        if (infinite())
        {
            // Carousel: wrap around from the beginning.
            for (uint32_t k = 0; k < targetIndex; k++)
            {
                auto b = boundsForFlatIndex(k);
                if (!isBoundsCollapsed(b))
                {
                    outPosition = Vec2D(-b.left(), -b.top());
                    return true;
                }
            }
        }
        else
        {
            // End of list: walk backward to last visible item.
            for (int k = (int)targetIndex - 1; k >= 0; k--)
            {
                auto b = boundsForFlatIndex(k);
                if (!isBoundsCollapsed(b))
                {
                    outPosition = Vec2D(-b.left(), -b.top());
                    return true;
                }
            }
        }
        // Nothing visible — can't resolve.
        return false;
    }

    // Has list children: single-pass through nested child→node structure.
    auto& children = scrollChildren();
    uint32_t flatIndex = 0;
    Vec2D lastVisibleBeforeTarget;
    bool hasVisibleBeforeTarget = false;
    bool reachedTarget = false;

    for (auto child : children)
    {
        if (child == nullptr)
        {
            continue;
        }
        int nodeCount = (int)child->numLayoutNodes();
        for (int j = 0; j < nodeCount; j++, flatIndex++)
        {
            auto bounds = child->layoutBoundsForNode(j);
            // Before target: track last visible for backward fallback.
            if (flatIndex < targetIndex)
            {
                if (!isBoundsCollapsed(bounds))
                {
                    lastVisibleBeforeTarget =
                        Vec2D(-bounds.left(), -bounds.top());
                    hasVisibleBeforeTarget = true;
                }
                continue;
            }
            // At target: return position if visible, otherwise keep walking.
            if (flatIndex == targetIndex)
            {
                reachedTarget = true;
                if (!isBoundsCollapsed(bounds))
                {
                    outPosition = Vec2D(
                        -bounds.left() - (bounds.width() + contentGap.x) * mod,
                        -bounds.top() - (bounds.height() + contentGap.y) * mod);
                    return true;
                }
                continue;
            }
            // Past target: return first visible item found.
            if (!isBoundsCollapsed(bounds))
            {
                outPosition = Vec2D(-bounds.left(), -bounds.top());
                return true;
            }
        }
    }

    if (!reachedTarget)
    {
        return false;
    }

    // No visible item after target.
    if (infinite())
    {
        // Carousel: wrap around from the beginning.
        flatIndex = 0;
        for (auto child : children)
        {
            if (child == nullptr)
            {
                continue;
            }
            int nodeCount = (int)child->numLayoutNodes();
            for (int j = 0; j < nodeCount; j++, flatIndex++)
            {
                if (flatIndex >= targetIndex)
                {
                    // Nothing visible — can't resolve.
                    return false;
                }
                auto bounds = child->layoutBoundsForNode(j);
                if (!isBoundsCollapsed(bounds))
                {
                    outPosition = Vec2D(-bounds.left(), -bounds.top());
                    return true;
                }
            }
        }
    }
    else if (hasVisibleBeforeTarget)
    {
        // End of list: fall back to last visible item before target.
        outPosition = lastVisibleBeforeTarget;
        return true;
    }

    // Nothing visible — can't resolve.
    return false;
}

float ScrollConstraint::indexAtPosition(Vec2D pos)
{
    if (content() == nullptr || content()->children().size() == 0)
    {
        return 0;
    }
    Vec2D contentGap = gap();
    if (!m_hasListChildren)
    {
        size_t count = scrollChildren().size();
        if (constrainsHorizontal())
        {
            for (size_t i = 0; i < count; i++)
            {
                auto bounds = scrollChildren()[i]->layoutBoundsForNode(0);
                float step = bounds.width() + contentGap.x;
                if (pos.x > -bounds.left() - step)
                {
                    return step != 0
                               ? (float)i + (-pos.x - bounds.left()) / step
                               : (float)i;
                }
            }
            return (float)count;
        }
        else if (constrainsVertical())
        {
            for (size_t i = 0; i < count; i++)
            {
                auto bounds = scrollChildren()[i]->layoutBoundsForNode(0);
                float step = bounds.height() + contentGap.y;
                if (pos.y > -bounds.top() - step)
                {
                    return step != 0 ? (float)i + (-pos.y - bounds.top()) / step
                                     : (float)i;
                }
            }
            return (float)count;
        }
        return 0;
    }
    // Has list children: nested iteration visiting each node once.
    float flatIndex = 0.0f;
    if (constrainsHorizontal())
    {
        for (auto child : scrollChildren())
        {
            if (child == nullptr)
            {
                continue;
            }
            int nodeCount = (int)child->numLayoutNodes();
            for (int j = 0; j < nodeCount; j++)
            {
                auto bounds = child->layoutBoundsForNode(j);
                float step = bounds.width() + contentGap.x;
                if (pos.x > -bounds.left() - step)
                {
                    return step != 0 ? (flatIndex + j) +
                                           (-pos.x - bounds.left()) / step
                                     : (flatIndex + j);
                }
            }
            flatIndex += nodeCount;
        }
        return flatIndex;
    }
    else if (constrainsVertical())
    {
        for (auto child : scrollChildren())
        {
            if (child == nullptr)
            {
                continue;
            }
            int nodeCount = (int)child->numLayoutNodes();
            for (int j = 0; j < nodeCount; j++)
            {
                auto bounds = child->layoutBoundsForNode(j);
                float step = bounds.height() + contentGap.y;
                if (pos.y > -bounds.top() - step)
                {
                    return step != 0 ? (flatIndex + j) +
                                           (-pos.y - bounds.top()) / step
                                     : (flatIndex + j);
                    (bounds.height() + contentGap.y);
                }
            }
            flatIndex += nodeCount;
        }
        return flatIndex;
    }
    return 0;
}

bool ScrollConstraint::isBoundsCollapsed(AABB bounds)
{
    return (constrainsHorizontal() && bounds.width() <= 0) ||
           (constrainsVertical() && bounds.height() <= 0);
}

size_t ScrollConstraint::scrollItemCount()
{
    if (!m_hasListChildren)
    {
        return scrollChildren().size();
    }
    size_t count = 0;
    for (auto child : scrollChildren())
    {
        if (child == nullptr)
        {
            continue;
        }
        count += child->numLayoutNodes();
    }
    return count;
}

AABB ScrollConstraint::boundsForFlatIndex(size_t index)
{
    auto& children = scrollChildren();
    if (!m_hasListChildren)
    {
        if (index < children.size() && children[index] != nullptr)
        {
            return children[index]->layoutBoundsForNode(0);
        }
        return AABB();
    }
    size_t flatIndex = 0;
    for (auto child : children)
    {
        if (child == nullptr)
        {
            continue;
        }
        size_t nodeCount = child->numLayoutNodes();
        if (index < flatIndex + nodeCount)
        {
            return child->layoutBoundsForNode((int)(index - flatIndex));
        }
        flatIndex += nodeCount;
    }
    return AABB();
}

Vec2D ScrollConstraint::gap()
{
    if (content() == nullptr)
    {
        return Vec2D();
    }
    return Vec2D(content()->gapHorizontal(), content()->gapVertical());
}

void ScrollConstraint::scrollToPosition(float targetX, float targetY)
{
    // Direct offset writes supersede held intents.
    clearScrollIntents();
    if (m_physics == nullptr)
    {
        // No physics, just set offset directly
        scrollOffsetX(targetX);
        scrollOffsetY(targetY);
        return;
    }

    Vec2D current(m_offsetX, m_offsetY);
    Vec2D target(targetX, targetY);
    Vec2D rangeMin(maxOffsetX(), maxOffsetY());
    Vec2D rangeMax(0.0f, 0.0f);

    m_physics->scrollToPosition(current,
                                target,
                                rangeMin,
                                rangeMax,
                                constrainsHorizontal(),
                                constrainsVertical());
}

static float nearestSnapInDirection(float current,
                                    float target,
                                    const std::vector<Vec2D>& snapPoints,
                                    bool useX)
{
    if (current == target)
    {
        return target;
    }
    bool scrollingNegative = target < current;
    float best = target;
    bool found = false;
    float bestDist = 0.0f;
    for (const auto& p : snapPoints)
    {
        float c = useX ? -p.x : -p.y;
        if (scrollingNegative ? c > target : c < target)
        {
            continue;
        }
        float d = scrollingNegative ? target - c : c - target;
        if (!found || d < bestDist)
        {
            bestDist = d;
            best = c;
            found = true;
        }
    }
    return found ? best : target;
}

Vec2D ScrollConstraint::nearestSnapOffsetInDirection(Vec2D current,
                                                     Vec2D target)
{
    if (!snap())
    {
        return target;
    }
    auto snapPoints = collectSnapPoints();
    if (snapPoints.empty())
    {
        return target;
    }
    float snappedX =
        constrainsHorizontal()
            ? nearestSnapInDirection(current.x, target.x, snapPoints, true)
            : target.x;
    float snappedY =
        constrainsVertical()
            ? nearestSnapInDirection(current.y, target.y, snapPoints, false)
            : target.y;
    return Vec2D(snappedX, snappedY);
}

float ScrollConstraint::effectiveScrollOffsetX()
{
    if (m_physics != nullptr && m_physics->isRunning() &&
        m_physics->hasTargetX())
    {
        return m_physics->targetX();
    }
    return scrollOffsetX();
}

float ScrollConstraint::effectiveScrollOffsetY()
{
    if (m_physics != nullptr && m_physics->isRunning() &&
        m_physics->hasTargetY())
    {
        return m_physics->targetY();
    }
    return scrollOffsetY();
}