#include "rive/focus_data.hpp"
#include "rive/artboard.hpp"
#include "rive/artboard_component_list.hpp"
#include "rive/artboard_host.hpp"
#include "rive/component.hpp"
#include "rive/constraints/scrolling/scroll_constraint.hpp"
#include "rive/drawable.hpp"
#include "rive/input/focus_input_traversal.hpp"
#include "rive/input/focusable.hpp"
#include "rive/input/focus_listener.hpp"
#include "rive/input/focus_manager.hpp"
#include "rive/layout_component.hpp"
#include "rive/math/mat2d.hpp"
#include "rive/nested_artboard.hpp"
#include "rive/node.hpp"
#include "rive/parent_traversal.hpp"
#include "rive/text/text_input.hpp"
#include "rive/transform_component.hpp"
#include "rive/world_transform_component.hpp"
#include <algorithm>

using namespace rive;

FocusData::~FocusData()
{
    if (m_focusNode != nullptr)
    {
        // Clear the focusable pointer first to prevent callbacks during removal
        m_focusNode->clearFocusable();

        // Remove from manager if registered
        auto* manager = m_focusNode->manager();
        if (manager != nullptr)
        {
            manager->removeChild(m_focusNode);
        }
        // m_focusNode (rcp) is released automatically when this destructor ends
    }
}

rcp<FocusNode> FocusData::focusNode()
{
    if (m_focusNode == nullptr)
    {
        m_focusNode = rcp<FocusNode>(new FocusNode(this));
        m_focusNode->canFocus(m_CanFocus);
        m_focusNode->canTouch(m_CanTouch);
        m_focusNode->canTraverse(m_CanTraverse);
        m_focusNode->edgeBehavior(
            static_cast<EdgeBehavior>(m_EdgeBehaviorValue));
        m_focusNode->name(name());
        // Set initial world bounds if available
        updateWorldBounds();
    }
    return m_focusNode;
}

void FocusData::addFocusListener(FocusListener* listener)
{
    m_focusListeners.push_back(listener);
}

void FocusData::removeFocusListener(FocusListener* listener)
{
    auto it =
        std::find(m_focusListeners.begin(), m_focusListeners.end(), listener);
    if (it != m_focusListeners.end())
    {
        m_focusListeners.erase(it);
    }
}

void FocusData::focus()
{
    // Note: In C++ runtime, focus() needs a FocusManager to set focus.
    // The StateMachineInstance will handle this through its own FocusManager.
    // This method is provided for API completeness but the actual focus
    // setting happens through StateMachineInstance::setFocus(FocusData*).
}

bool FocusData::keyInput(Key value,
                         KeyModifiers modifiers,
                         bool isPressed,
                         bool isRepeat)
{
    // Search only the children of this FocusData's owner (parent Node),
    // not the entire artboard.
    auto* parentNode = parent();
    if (parentNode == nullptr || !parentNode->is<Node>())
    {
        return false;
    }
    // If the parent is Focusable and immediately handles the input, we're done!
    auto* focusable = Focusable::from(parentNode);
    if (focusable != nullptr &&
        focusable->keyInput(value, modifiers, isPressed, isRepeat))
    {
        return true;
    }
    for (auto* child : parentNode->as<Node>()->children())
    {
        if (sendInputToFocusableChildren(child,
                                         &Focusable::keyInput,
                                         value,
                                         modifiers,
                                         isPressed,
                                         isRepeat))
        {
            return true;
        }
    }
    return false;
}

bool FocusData::textInput(const std::string& text)
{
    // Search only the children of this FocusData's owner (parent Node),
    // not the entire artboard.
    auto* parentNode = parent();
    if (parentNode == nullptr || !parentNode->is<Node>())
    {
        return false;
    }

    // If the parent is Focusable and immediately handles the input, we're done!
    auto* focusable = Focusable::from(parentNode);
    if (focusable != nullptr && focusable->textInput(text))
    {
        return true;
    }

    // Look through children for a Focusable to handle the text input.
    for (auto* child : parentNode->as<Node>()->children())
    {
        if (sendInputToFocusableChildren(child, &Focusable::textInput, text))
        {
            return true;
        }
    }
    return false;
}

void FocusData::scrollIntoView()
{
    // Find closest LayoutComponent ancestor
    Component* layoutComponent = parent();
    while (layoutComponent != nullptr &&
           !layoutComponent->is<LayoutComponent>())
    {
        layoutComponent = layoutComponent->parent();
    }
    if (layoutComponent == nullptr)
    {
        return;
    }

    // Get element bounds in its artboard's world space.
    // We'll transform these bounds as we cross artboard boundaries.
    AABB elementBounds = layoutComponent->as<LayoutComponent>()->worldBounds();

    // Walk up the hierarchy finding all ScrollConstraints.
    // This traversal crosses artboard boundaries via the host chain.
    ParentTraversal traversal(this);
    while (auto* p = traversal.next())
    {
        // When crossing an artboard boundary, transform element bounds from
        // nested artboard space to parent artboard space.
        if (traversal.didCrossBoundary())
        {
            auto* host = traversal.crossingHost();
            auto* sourceArtboard = traversal.sourceArtboard();
            auto* artboardInstance =
                sourceArtboard->is<ArtboardInstance>()
                    ? sourceArtboard->as<ArtboardInstance>()
                    : nullptr;
            Mat2D hostTransform =
                host->worldTransformForArtboard(artboardInstance);
            Vec2D min =
                hostTransform * Vec2D(elementBounds.minX, elementBounds.minY);
            Vec2D max =
                hostTransform * Vec2D(elementBounds.maxX, elementBounds.maxY);
            elementBounds = AABB(min.x, min.y, max.x, max.y);
        }

        // ScrollConstraint is a Constraint that targets Content, so we check
        // TransformComponent::constraints() to find ScrollConstraints.
        if (p->is<TransformComponent>())
        {
            auto* tc = p->as<TransformComponent>();

            for (auto* constraint : tc->constraints())
            {
                if (!constraint->is<ScrollConstraint>())
                {
                    continue;
                }

                scrollConstraintToShowBounds(constraint->as<ScrollConstraint>(),
                                             elementBounds);
            }
        }
    }
}

void FocusData::scrollConstraintToShowBounds(ScrollConstraint* constraint,
                                             const AABB& elementBounds)
{
    auto* content = constraint->content();
    auto* viewport = constraint->viewport();
    if (content == nullptr || viewport == nullptr)
    {
        return;
    }

    // Get viewport position in world space.
    const Mat2D& viewportTransform = viewport->worldTransform();
    float viewportWorldX = viewportTransform[4];
    float viewportWorldY = viewportTransform[5];

    float viewportWidth = constraint->viewportWidth();
    float viewportHeight = constraint->viewportHeight();

    // Element position relative to viewport in world space.
    // This directly tells us where the element appears in the viewport.
    float viewportLeft = elementBounds.minX - viewportWorldX;
    float viewportTop = elementBounds.minY - viewportWorldY;
    float viewportRight = elementBounds.maxX - viewportWorldX;
    float viewportBottom = elementBounds.maxY - viewportWorldY;

    // Use effective scroll offset (target if animating) to handle rapid focus
    // changes.
    float effectiveScrollX = constraint->effectiveScrollOffsetX();
    float effectiveScrollY = constraint->effectiveScrollOffsetY();

    float deltaX = 0.0f;
    float deltaY = 0.0f;

    // Calculate horizontal scroll adjustment.
    if (constraint->constrainsHorizontal())
    {
        if (viewportLeft < 0)
        {
            // Element is to the left of viewport, scroll right (increase
            // offset)
            deltaX = -viewportLeft;
        }
        else if (viewportRight > viewportWidth)
        {
            // Element is to the right of viewport, scroll left (decrease
            // offset)
            deltaX = -(viewportRight - viewportWidth);
        }
    }

    // Calculate vertical scroll adjustment
    if (constraint->constrainsVertical())
    {
        if (viewportTop < 0)
        {
            // Element is above viewport, scroll up (increase offset)
            deltaY = -viewportTop;
        }
        else if (viewportBottom > viewportHeight)
        {
            // Element is below viewport, scroll down (decrease offset)
            deltaY = -(viewportBottom - viewportHeight);
        }
    }

    // Apply scroll if needed (using animated scroll)
    if (deltaX != 0 || deltaY != 0)
    {
        // Add delta to effective scroll offset to get target position.
        float targetX = effectiveScrollX + deltaX;
        float targetY = effectiveScrollY + deltaY;
        constraint->scrollToPosition(targetX, targetY);
    }
}

void FocusData::focused()
{
    // Scroll this element into view, crossing artboard boundaries as needed
    scrollIntoView();

    // Notify listeners
    for (auto* listener : m_focusListeners)
    {
        listener->onFocused();
    }
}

void FocusData::blurred()
{
    for (auto* listener : m_focusListeners)
    {
        listener->onBlurred();
    }
}

void FocusData::canFocusChanged()
{
    if (m_focusNode != nullptr)
    {
        m_focusNode->canFocus(m_CanFocus);
    }
}

void FocusData::canTouchChanged()
{
    if (m_focusNode != nullptr)
    {
        m_focusNode->canTouch(m_CanTouch);
    }
}

void FocusData::canTraverseChanged()
{
    if (m_focusNode != nullptr)
    {
        m_focusNode->canTraverse(m_CanTraverse);
    }
}

void FocusData::edgeBehaviorValueChanged()
{
    if (m_focusNode != nullptr)
    {
        m_focusNode->edgeBehavior(
            static_cast<EdgeBehavior>(m_EdgeBehaviorValue));
    }
}

FocusData* FocusData::findParentFocusData() const
{
    // FocusData's parent is typically a Node. Walk up from the parent
    // to find any ancestor Node that has a FocusData child.
    auto* current = parent();
    while (current != nullptr)
    {
        if (current->is<Node>())
        {
            auto* node = current->as<Node>();
            for (auto* child : node->children())
            {
                if (child->is<FocusData>() && child != this)
                {
                    return child->as<FocusData>();
                }
            }
        }
        current = current->parent();
    }
    return nullptr;
}

rcp<FocusNode> FocusData::findClosestFocusNode(Component* component)
{
    if (component == nullptr)
    {
        return nullptr;
    }

    auto* current = component->parent();
    while (current != nullptr)
    {
        // Check if we've hit an artboard boundary
        if (current->is<Artboard>())
        {
            auto* artboard = current->as<Artboard>();
            // Try to cross the artboard boundary via the host component
            auto* host = artboard->host();
            if (host != nullptr)
            {
                auto* hostComponent = host->hostComponent();
                if (hostComponent != nullptr &&
                    hostComponent->is<ContainerComponent>())
                {
                    // Continue searching from the host component (e.g.,
                    // NestedArtboard)
                    current = hostComponent->as<ContainerComponent>();
                    continue;
                }
            }
#ifdef WITH_RIVE_TOOLS
            // Fall back to externally-set parent focus node (editor scenario)
            auto externalNode = artboard->externalParentFocusNode();
            if (externalNode != nullptr)
            {
                return externalNode;
            }
#endif
            // No way to traverse up, stop searching
            return nullptr;
        }

        // Check if this node has a FocusData child
        if (current->is<Node>())
        {
            for (auto* child : current->as<Node>()->children())
            {
                if (child->is<FocusData>())
                {
                    return child->as<FocusData>()->focusNode();
                }
            }
        }

        current = current->parent();
    }
    return nullptr;
}

namespace
{

bool componentAllowsFocusTraversal(const Component* c)
{
    if (c == nullptr || c->isCollapsed())
    {
        return false;
    }
    if (c->is<Drawable>())
    {
        if (c->as<Drawable>()->isHidden())
        {
            return false;
        }
    }
    if (c->is<TransformComponent>())
    {
        if (c->as<TransformComponent>()->renderOpacity() <= 0.0f)
        {
            return false;
        }
    }
    return true;
}

} // namespace

bool FocusData::isEligibleForFocusTraversal() const
{
    if (isCollapsed())
    {
        return false;
    }
    Component* start = parent();
    if (start == nullptr)
    {
        return true;
    }
    if (!componentAllowsFocusTraversal(start))
    {
        return false;
    }
    ParentTraversal traversal(start);
    while (true)
    {
        ContainerComponent* p = traversal.next();
        if (p == nullptr)
        {
            break;
        }
        if (!componentAllowsFocusTraversal(p))
        {
            return false;
        }
        if (traversal.didCrossBoundary())
        {
            ArtboardHost* host = traversal.crossingHost();
            if (host != nullptr)
            {
                Component* hc = host->hostComponent();
                if (hc != nullptr)
                {
                    if (!componentAllowsFocusTraversal(hc))
                    {
                        return false;
                    }
                    if (hc->is<NestedArtboard>() &&
                        hc->as<NestedArtboard>()->isPaused())
                    {
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

bool FocusData::worldPosition(Vec2D& outPosition)
{
    // Get local position from parent transform component
    auto* parentComponent = parent();
    if (!parentComponent || !parentComponent->is<WorldTransformComponent>())
    {
        return false;
    }
    Vec2D localPos =
        parentComponent->as<WorldTransformComponent>()->worldTranslation();

    // Transform to root artboard space (handles nested artboards)
    auto* ab = artboard();
    if (ab)
    {
        outPosition = ab->rootTransform(localPos);
    }
    else
    {
        outPosition = localPos;
    }
    return true;
}

void FocusData::nameChanged()
{
    if (m_focusNode != nullptr)
    {
        m_focusNode->name(name());
    }
}

void FocusData::buildDependencies()
{
    Super::buildDependencies();
    // Depend on parent's world transform so we update when it moves
    auto* parentComponent = parent();
    if (parentComponent != nullptr)
    {
        parentComponent->addDependent(this);
    }
}

void FocusData::update(ComponentDirt value)
{
    if ((value & ComponentDirt::WorldTransform) ==
        ComponentDirt::WorldTransform)
    {
        updateWorldBounds();
    }
}

void FocusData::updateWorldBounds()
{
    if (m_focusNode == nullptr)
    {
        return;
    }

    auto* parentComponent = parent();
    if (parentComponent != nullptr && parentComponent->is<LayoutComponent>())
    {
        // LayoutComponent has worldBounds based on its layout dimensions
        AABB bounds = parentComponent->as<LayoutComponent>()->worldBounds();
        // Transform to root artboard space (handles nested artboards)
        auto* ab = artboard();
        if (ab != nullptr)
        {
            Vec2D min = ab->rootTransform(Vec2D(bounds.minX, bounds.minY));
            Vec2D max = ab->rootTransform(Vec2D(bounds.maxX, bounds.maxY));
            bounds = AABB(min.x, min.y, max.x, max.y);
        }
        m_focusNode->worldBounds(bounds);
    }
    else
    {
        // For non-layout parents, clear bounds (will fall back to position)
        m_focusNode->clearWorldBounds();
    }
}