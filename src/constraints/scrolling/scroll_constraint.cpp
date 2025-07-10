#include "rive/constraints/scrolling/scroll_constraint.hpp"
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
    if (m_physics != nullptr)
    {
        m_physics->accumulate(delta, timeStamp);
    }
    scrollOffsetX(offsetX() + delta.x);
    scrollOffsetY(offsetY() + delta.y);
}

void ScrollConstraint::runPhysics()
{
    m_isDragging = false;
    std::vector<Vec2D> snappingPoints;
    if (snap())
    {
        for (auto child : content()->children())
        {
            auto c = LayoutNodeProvider::from(child);
            if (c != nullptr)
            {
                size_t count = c->numLayoutNodes();
                for (int j = 0; j < count; j++)
                {
                    auto bounds = c->layoutBoundsForNode(j);
                    snappingPoints.push_back(
                        Vec2D(bounds.left(), bounds.top()));
                }
            }
        }
    }
    if (m_physics != nullptr)
    {
        m_physics->run(Vec2D(maxOffsetX(), maxOffsetY()),
                       Vec2D(minOffsetX(), minOffsetY()),
                       Vec2D(offsetX(), offsetY()),
                       snap() ? snappingPoints : std::vector<Vec2D>(),
                       mainAxisIsColumn() ? contentHeight() : contentWidth());
    }
}

bool ScrollConstraint::advanceComponent(float elapsedSeconds,
                                        AdvanceFlags flags)
{
    if ((flags & AdvanceFlags::AdvanceNested) != AdvanceFlags::AdvanceNested)
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
    return m_physics->enabled();
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
    for (auto child : content()->children())
    {
        auto layout = LayoutNodeProvider::from(child);
        if (layout != nullptr)
        {
            addDependent(child);
            layout->addLayoutConstraint(static_cast<LayoutConstraint*>(this));
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

float ScrollConstraint::maxOffsetXForPercent()
{
    return infinite() ? contentWidth() : maxOffsetX();
}

float ScrollConstraint::maxOffsetYForPercent()
{
    return infinite() ? contentHeight() : maxOffsetY();
}

float ScrollConstraint::scrollPercentX()
{
    return maxOffsetX() != 0 ? scrollOffsetX() / maxOffsetXForPercent() : 0;
}

float ScrollConstraint::scrollPercentY()
{
    return maxOffsetY() != 0 ? scrollOffsetY() / maxOffsetYForPercent() : 0;
}

float ScrollConstraint::scrollIndex()
{
    return indexAtPosition(Vec2D(scrollOffsetX(), scrollOffsetY()));
}

void ScrollConstraint::setScrollPercentX(float value)
{
    if (m_isDragging)
    {
        return;
    }
    stopPhysics();
    float to = value * maxOffsetXForPercent();
    scrollOffsetX(to);
}

void ScrollConstraint::setScrollPercentY(float value)
{
    if (m_isDragging)
    {
        return;
    }
    stopPhysics();
    float to = value * maxOffsetYForPercent();
    scrollOffsetY(to);
}

void ScrollConstraint::setScrollIndex(float value)
{
    if (m_isDragging)
    {
        return;
    }
    stopPhysics();
    Vec2D to = positionAtIndex(value);
    if (constrainsHorizontal())
    {
        scrollOffsetX(to.x);
    }
    else if (constrainsVertical())
    {
        scrollOffsetY(to.y);
    }
}

Vec2D ScrollConstraint::positionAtIndex(float index)
{
    auto count = scrollItemCount();
    if (content() == nullptr || count == 0)
    {
        return Vec2D();
    }
    uint32_t i = 0;
    Vec2D contentGap = gap();
    float normalizedIndex = infinite() ? std::fmod(index, (float)count) : index;
    float floorIndex = std::floor(normalizedIndex);
    LayoutNodeProvider* lastChild = nullptr;
    for (auto child : scrollChildren())
    {
        if (child == nullptr)
        {
            continue;
        }
        size_t count = child->numLayoutNodes();
        if ((uint32_t)floorIndex < i + count)
        {
            float mod = normalizedIndex - floorIndex;
            auto bounds = child->layoutBoundsForNode(floorIndex - i);
            return Vec2D(-bounds.left() - (bounds.width() + contentGap.x) * mod,
                         -bounds.top() -
                             (bounds.height() + contentGap.y) * mod);
        }
        lastChild = child;
        i += count;
    }
    if (lastChild == nullptr)
    {
        return Vec2D();
    }

    auto bounds =
        lastChild->layoutBoundsForNode((int)lastChild->numLayoutNodes() - 1);
    return Vec2D(-bounds.left(), -bounds.top());
}

float ScrollConstraint::indexAtPosition(Vec2D pos)
{
    if (content() == nullptr || content()->children().size() == 0)
    {
        return 0;
    }
    float i = 0.0f;
    Vec2D contentGap = gap();
    if (constrainsHorizontal())
    {
        for (auto child : scrollChildren())
        {
            if (child == nullptr)
            {
                continue;
            }
            size_t count = child->numLayoutNodes();
            for (int j = 0; j < count; j++)
            {
                auto bounds = child->layoutBoundsForNode(j);
                if (pos.x > -bounds.left() - (bounds.width() + contentGap.x))
                {
                    return (i + j) + (-pos.x - bounds.left()) /
                                         (bounds.width() + contentGap.x);
                }
            }
            i += count;
        }
        return i;
    }
    else if (constrainsVertical())
    {
        for (auto child : scrollChildren())
        {
            if (child == nullptr)
            {
                continue;
            }
            size_t count = child->numLayoutNodes();
            for (int j = 0; j < count; j++)
            {
                auto bounds = child->layoutBoundsForNode(j);
                if (pos.y > -bounds.top() - (bounds.height() + contentGap.y))
                {
                    return (i + j) + (-pos.y - bounds.top()) /
                                         (bounds.height() + contentGap.y);
                }
            }
            i += count;
        }
        return i;
    }
    return 0;
}

size_t ScrollConstraint::scrollItemCount()
{
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

Vec2D ScrollConstraint::gap()
{
    if (content() == nullptr)
    {
        return Vec2D();
    }
    return Vec2D(content()->gapHorizontal(), content()->gapVertical());
}