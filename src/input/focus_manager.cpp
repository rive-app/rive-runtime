/*
 * Copyright 2024 Rive
 */

#include "rive/input/focus_manager.hpp"
#include "rive/artboard.hpp"
#include "rive/artboard_host.hpp"
#include "rive/math/aabb.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace rive
{

Artboard* FocusManager::primaryFocusArtboard() const
{
    if (m_primaryFocus == nullptr || m_primaryFocus->focusable() == nullptr)
    {
        return nullptr;
    }

    // Get the immediate artboard containing the focused element
    Artboard* artboard = m_primaryFocus->focusable()->focusableArtboard();
    if (artboard == nullptr)
    {
        return nullptr;
    }

    // Walk up the nested artboard chain to find the root (one with no host)
    // The root artboard is the one mounted by Dart, which has no host.
    while (artboard->host() != nullptr &&
           artboard->host()->parentArtboard() != nullptr)
    {
        artboard = artboard->host()->parentArtboard();
    }

    return artboard;
}

FocusManager::~FocusManager()
{
    // Don't call clearFocus() here - it would invoke callbacks on FocusNodes,
    // but during destruction (especially Dart finalizers) callbacks may be
    // invalid. Just clear the reference directly.
    m_primaryFocus = nullptr;
}

void FocusManager::setFocus(rcp<FocusNode> node)
{
    if (node == m_primaryFocus)
    {
        return;
    }

    if (node && !node->canFocus())
    {
        return;
    }

    FocusNode* oldFocus = m_primaryFocus.get();
    m_primaryFocus = std::move(node);
    notifyFocusChange(oldFocus, m_primaryFocus.get());
}

void FocusManager::clearFocus()
{
    if (m_primaryFocus)
    {
        // Move to local variable to keep node alive during notification.
        // Setting m_primaryFocus to nullptr first ensures hasFocus() returns
        // false during the blurred() callback, but the node stays alive until
        // notification completes.
        auto oldFocus = std::move(m_primaryFocus);
        notifyFocusChange(oldFocus.get(), nullptr);
        // oldFocus is released here after notification is complete
    }
}

bool FocusManager::hasFocus(rcp<FocusNode> node) const
{
    // The hasFocus flag on the node is maintained by notifyFocusChange
    return node && node->hasFocus();
}

bool FocusManager::hasPrimaryFocus(rcp<FocusNode> node) const
{
    return m_primaryFocus == node;
}

void FocusManager::addChild(rcp<FocusNode> parent, rcp<FocusNode> child)
{
    if (!child)
    {
        return;
    }

    // Remove from old location first
    if (child->parent())
    {
        child->removeFromParent();
    }
    else
    {
        // Remove from root nodes if it was a root
        auto it = std::find(m_rootNodes.begin(), m_rootNodes.end(), child);
        if (it != m_rootNodes.end())
        {
            m_rootNodes.erase(it);
        }
    }

    // Set the manager reference on the child
    child->m_manager = this;

    if (parent)
    {
        parent->addChild(std::move(child));
    }
    else
    {
        // Add to root nodes
        m_rootNodes.push_back(std::move(child));
    }
}

void FocusManager::removeChild(rcp<FocusNode> child)
{
    if (!child)
    {
        return;
    }

    // Clear focus if this node or descendant has focus
    if (hasFocus(child))
    {
        clearFocus();
    }

    // Clear the manager reference
    child->m_manager = nullptr;

    if (child->parent())
    {
        child->removeFromParent();
    }
    else
    {
        // Remove from root nodes
        auto it = std::find(m_rootNodes.begin(), m_rootNodes.end(), child);
        if (it != m_rootNodes.end())
        {
            m_rootNodes.erase(it);
        }
    }
}

bool FocusManager::focusNext()
{
    return findNextFocusable(m_primaryFocus.get(), true) != nullptr;
}

bool FocusManager::focusPrevious()
{
    return findNextFocusable(m_primaryFocus.get(), false) != nullptr;
}

// Helper to get root-space world position from FocusNode
// Computes center from bounds if available, falls back to Focusable
static bool getRootPosition(FocusNode* node, Vec2D& outPosition)
{
    if (!node)
    {
        return false;
    }
    // First try bounds stored directly on FocusNode (set during update cycle)
    if (node->hasWorldBounds())
    {
        outPosition = node->worldBounds().center();
        return true;
    }
    // Fall back to Focusable for legacy implementations
    if (node->focusable())
    {
        return node->focusable()->worldPosition(outPosition);
    }
    return false;
}

// Helper to get root-space world bounds from FocusNode
// First checks bounds stored on FocusNode, falls back to Focusable
static bool getRootBounds(FocusNode* node, AABB& outBounds)
{
    if (!node)
    {
        return false;
    }
    // First try bounds stored directly on FocusNode (set during update cycle)
    if (node->hasWorldBounds())
    {
        outBounds = node->worldBounds();
        return true;
    }
    // Fall back to Focusable for legacy implementations
    if (node->focusable())
    {
        return node->focusable()->worldBounds(outBounds);
    }
    return false;
}

// Helper to check if a node is a leaf (no traversable children)
static bool isLeaf(FocusNode* node)
{
    for (const auto& child : node->children())
    {
        if (child->canFocus() && child->canTraverse())
        {
            return false;
        }
    }
    return true;
}

// Helper to collect all traversable leaf focus nodes recursively
// Only collects leaves (nodes with no traversable children) to match
// next/prev behavior
static void collectAllTraversableNodes(const std::vector<rcp<FocusNode>>& nodes,
                                       std::vector<FocusNode*>& result)
{
    for (const auto& node : nodes)
    {
        if (node->canFocus() && node->canTraverse() && isLeaf(node.get()))
        {
            result.push_back(node.get());
        }
        // Recurse into children
        collectAllTraversableNodes(node->children(), result);
    }
}

// Calculate overlap on the orthogonal axis (perpendicular to navigation)
// Returns the length of overlap, or 0 if no overlap
static float calculateOverlap(float aMin, float aMax, float bMin, float bMax)
{
    float overlapMin = std::max(aMin, bMin);
    float overlapMax = std::min(aMax, bMax);
    return std::max(0.0f, overlapMax - overlapMin);
}

// CSS Spatial Navigation-inspired scoring for bounds-aware navigation
// Formula: distance = displacement + orthogonalWeight * orthogonalDistance -
// sqrt(overlap)
struct ScoreBreakdown
{
    float displacement;
    float orthogonalDistance;
    float overlap;
    float orthogonalWeight;
    float total;
    bool rejected;
};

static ScoreBreakdown scoreCandidateBoundsDetailed(const AABB& current,
                                                   const AABB& candidate,
                                                   Direction direction)
{
    // CSS Spatial Navigation weights
    const float horizontalWeight = 30.0f;
    const float verticalWeight = 2.0f;

    ScoreBreakdown result = {};

    switch (direction)
    {
        case Direction::left:
            // Displacement: distance from current's left edge to candidate's
            // right edge
            result.displacement = current.left() - candidate.right();
            if (result.displacement < 0)
            {
                result.rejected = true;
                result.total = std::numeric_limits<float>::max();
                return result;
            }
            // Orthogonal: vertical distance between closest edges
            result.orthogonalDistance =
                std::max(0.0f,
                         std::max(candidate.top() - current.bottom(),
                                  current.top() - candidate.bottom()));
            result.overlap = calculateOverlap(current.top(),
                                              current.bottom(),
                                              candidate.top(),
                                              candidate.bottom());
            result.orthogonalWeight = horizontalWeight;
            break;

        case Direction::right:
            result.displacement = candidate.left() - current.right();
            if (result.displacement < 0)
            {
                result.rejected = true;
                result.total = std::numeric_limits<float>::max();
                return result;
            }
            result.orthogonalDistance =
                std::max(0.0f,
                         std::max(candidate.top() - current.bottom(),
                                  current.top() - candidate.bottom()));
            result.overlap = calculateOverlap(current.top(),
                                              current.bottom(),
                                              candidate.top(),
                                              candidate.bottom());
            result.orthogonalWeight = horizontalWeight;
            break;

        case Direction::up:
            result.displacement = current.top() - candidate.bottom();
            if (result.displacement < 0)
            {
                result.rejected = true;
                result.total = std::numeric_limits<float>::max();
                return result;
            }
            result.orthogonalDistance =
                std::max(0.0f,
                         std::max(candidate.left() - current.right(),
                                  current.left() - candidate.right()));
            result.overlap = calculateOverlap(current.left(),
                                              current.right(),
                                              candidate.left(),
                                              candidate.right());
            result.orthogonalWeight = verticalWeight;
            break;

        case Direction::down:
            result.displacement = candidate.top() - current.bottom();
            if (result.displacement < 0)
            {
                result.rejected = true;
                result.total = std::numeric_limits<float>::max();
                return result;
            }
            result.orthogonalDistance =
                std::max(0.0f,
                         std::max(candidate.left() - current.right(),
                                  current.left() - candidate.right()));
            result.overlap = calculateOverlap(current.left(),
                                              current.right(),
                                              candidate.left(),
                                              candidate.right());
            result.orthogonalWeight = verticalWeight;
            break;
    }

    // CSS-inspired formula: displacement + weighted orthogonal - sqrt(overlap)
    // The sqrt(overlap) bonus favors candidates that are "in line" with current
    result.total = result.displacement +
                   result.orthogonalWeight * result.orthogonalDistance -
                   std::sqrt(result.overlap);
    return result;
}

static float scoreCandidateBounds(const AABB& current,
                                  const AABB& candidate,
                                  Direction direction)
{
    return scoreCandidateBoundsDetailed(current, candidate, direction).total;
}

// Point-based scoring fallback for nodes without bounds
static float scoreCandidatePoint(const Vec2D& currentPos,
                                 const Vec2D& candidatePos,
                                 Direction direction)
{
    const float horizontalWeight = 30.0f;
    const float verticalWeight = 2.0f;

    Vec2D delta = candidatePos - currentPos;
    float primary, orthogonal, orthogonalWeight;

    switch (direction)
    {
        case Direction::left:
            primary = -delta.x;
            orthogonal = std::abs(delta.y);
            orthogonalWeight = horizontalWeight;
            break;
        case Direction::right:
            primary = delta.x;
            orthogonal = std::abs(delta.y);
            orthogonalWeight = horizontalWeight;
            break;
        case Direction::up:
            primary = -delta.y;
            orthogonal = std::abs(delta.x);
            orthogonalWeight = verticalWeight;
            break;
        case Direction::down:
            primary = delta.y;
            orthogonal = std::abs(delta.x);
            orthogonalWeight = verticalWeight;
            break;
    }

    if (primary <= 0)
    {
        return std::numeric_limits<float>::max();
    }

    return primary + orthogonalWeight * orthogonal;
}

FocusNode* FocusManager::findNodeInDirection(FocusNode* current,
                                             Direction direction) const
{
    if (!current)
    {
        return nullptr;
    }

    std::vector<FocusNode*> candidates;
    collectAllTraversableNodes(m_rootNodes, candidates);

    FocusNode* best = nullptr;
    float bestScore = std::numeric_limits<float>::max();

    // Try to get bounds for current node
    AABB currentBounds;
    bool currentHasBounds = getRootBounds(current, currentBounds);

    // Fallback to position if no bounds
    Vec2D currentPos;
    if (!currentHasBounds && !getRootPosition(current, currentPos))
    {
        return nullptr;
    }

    for (FocusNode* candidate : candidates)
    {
        if (candidate == current)
        {
            continue;
        }

        float score;

        // Try bounds-based scoring first
        AABB candidateBounds;
        if (currentHasBounds && getRootBounds(candidate, candidateBounds))
        {
            score =
                scoreCandidateBounds(currentBounds, candidateBounds, direction);
        }
        else
        {
            // Fall back to point-based scoring
            Vec2D candidatePos;
            if (!getRootPosition(candidate, candidatePos))
            {
                continue;
            }

            if (currentHasBounds)
            {
                // Use center of current bounds
                score = scoreCandidatePoint(currentBounds.center(),
                                            candidatePos,
                                            direction);
            }
            else
            {
                score =
                    scoreCandidatePoint(currentPos, candidatePos, direction);
            }
        }

        if (score < bestScore)
        {
            bestScore = score;
            best = candidate;
        }
    }

    return best;
}

bool FocusManager::focusLeft()
{
    FocusNode* next =
        findNodeInDirection(m_primaryFocus.get(), Direction::left);
    if (next)
    {
        setFocus(ref_rcp(next));
        return true;
    }
    return false;
}

bool FocusManager::focusRight()
{
    FocusNode* next =
        findNodeInDirection(m_primaryFocus.get(), Direction::right);
    if (next)
    {
        setFocus(ref_rcp(next));
        return true;
    }
    return false;
}

bool FocusManager::focusUp()
{
    FocusNode* next = findNodeInDirection(m_primaryFocus.get(), Direction::up);
    if (next)
    {
        setFocus(ref_rcp(next));
        return true;
    }
    return false;
}

bool FocusManager::focusDown()
{
    FocusNode* next =
        findNodeInDirection(m_primaryFocus.get(), Direction::down);
    if (next)
    {
        setFocus(ref_rcp(next));
        return true;
    }
    return false;
}

bool FocusManager::keyInput(Key key,
                            KeyModifiers modifiers,
                            bool isPressed,
                            bool isRepeat)
{
    // Bubble up through focus tree until someone handles the input
    FocusNode* node = m_primaryFocus.get();
    while (node != nullptr)
    {
        if (node->keyInput(key, modifiers, isPressed, isRepeat))
        {
            return true;
        }
        node = node->parent();
    }
    return false;
}

bool FocusManager::textInput(const std::string& text)
{
    // Bubble up through focus tree until someone handles the input
    FocusNode* node = m_primaryFocus.get();
    while (node != nullptr)
    {
        if (node->textInput(text))
        {
            return true;
        }
        node = node->parent();
    }
    return false;
}

void FocusManager::notifyFocusChange(FocusNode* oldFocus, FocusNode* newFocus)
{
    // Find the common ancestor to avoid unnecessary blur/focus notifications
    // on shared ancestors
    FocusNode* commonAncestor = nullptr;
    if (oldFocus != nullptr && newFocus != nullptr)
    {
        // Build a set of ancestors from oldFocus
        std::unordered_set<FocusNode*> oldAncestors;
        for (FocusNode* node = oldFocus; node != nullptr; node = node->parent())
        {
            oldAncestors.insert(node);
        }
        // Find the first ancestor of newFocus that's also an ancestor of
        // oldFocus
        for (FocusNode* node = newFocus; node != nullptr; node = node->parent())
        {
            if (oldAncestors.count(node) > 0)
            {
                commonAncestor = node;
                break;
            }
        }
    }

    // Walk up from oldFocus, clear hasFocus flag and notify blurred
    // Stop at common ancestor (don't blur it or its ancestors)
    FocusNode* current = oldFocus;
    while (current != nullptr && current != commonAncestor &&
           current->hasFocus())
    {
        current->setHasFocus(false);
        current->blurred();
        current = current->parent();
    }

    // Walk up from newFocus, set hasFocus flag and notify focused
    // Stop at common ancestor (don't re-focus it or its ancestors)
    current = newFocus;
    while (current != nullptr && current != commonAncestor &&
           !current->hasFocus())
    {
        current->setHasFocus(true);
        current->focused();
        current = current->parent();
    }

#ifdef WITH_RIVE_TOOLS
    if (m_focusChangedCallback)
    {
        m_focusChangedCallback();
    }

    // Check if we should fire scroll-into-view callback for Dart-mounted
    // artboards. This happens when the focused element is in an artboard
    // whose root has no host (mounted by Dart).
    if (m_scrollIntoViewCallback && newFocus != nullptr)
    {
        AABB bounds;
        if (getRootBounds(newFocus, bounds))
        {
            // Get the immediate artboard containing the focused element
            Artboard* artboard =
                newFocus->focusable() != nullptr
                    ? newFocus->focusable()->focusableArtboard()
                    : nullptr;
            if (artboard != nullptr)
            {
                // Walk up to find the highest artboard (host == nullptr).
                // This is the artboard that Dart is hosting.
                while (artboard->host() != nullptr &&
                       artboard->host()->parentArtboard() != nullptr)
                {
                    artboard = artboard->host()->parentArtboard();
                }

                // If this artboard has no host, it's Dart-mounted
                // Pass it to the callback so Dart can find and scroll it
                if (artboard->host() == nullptr)
                {
                    m_scrollIntoViewCallback(bounds, artboard);
                }
            }
        }
    }
#endif
}

std::vector<FocusNode*> FocusManager::getTraversableNodes(
    FocusNode* scope) const
{
    std::vector<FocusNode*> result;

    // Get children from scope or root nodes
    const std::vector<rcp<FocusNode>>* childList =
        scope ? &scope->children() : &m_rootNodes;

    for (const auto& child : *childList)
    {
        if (child->canFocus() && child->canTraverse())
        {
            result.push_back(child.get());
        }
    }

    // Sort by tabIndex, then by tree order (which is insertion order)
    std::stable_sort(result.begin(),
                     result.end(),
                     [](FocusNode* a, FocusNode* b) {
                         return a->tabIndex() < b->tabIndex();
                     });

    return result;
}

// Helper to get the first focusable leaf (deepest first child)
static FocusNode* getFirstLeaf(FocusNode* node, const FocusManager* manager)
{
    if (!node)
        return nullptr;

    // If node has traversable children, descend to first child
    auto children = manager->getTraversableNodes(node);
    if (!children.empty())
    {
        return getFirstLeaf(children.front(), manager);
    }
    // No traversable children - this is a leaf
    return node;
}

// Helper to get the last focusable leaf (deepest last child)
static FocusNode* getLastLeaf(FocusNode* node, const FocusManager* manager)
{
    if (!node)
        return nullptr;

    // If node has traversable children, descend to last child
    auto children = manager->getTraversableNodes(node);
    if (!children.empty())
    {
        return getLastLeaf(children.back(), manager);
    }
    // No traversable children - this is a leaf
    return node;
}

FocusNode* FocusManager::findNextFocusable(FocusNode* current,
                                           bool forward) const
{
    FocusNode* scope = current ? current->parent() : nullptr;
    auto traversable = getTraversableNodes(scope);

    if (traversable.empty())
    {
        // No traversable nodes at this level, try parent scope
        if (scope)
        {
            return findNextFocusable(scope, forward);
        }
        return nullptr;
    }

    // Find current position
    auto it = std::find(traversable.begin(), traversable.end(), current);

    FocusNode* next = nullptr;

    if (it == traversable.end())
    {
        // No current focus or current not in this list, pick first or last leaf
        FocusNode* candidate =
            forward ? traversable.front() : traversable.back();
        next = forward ? getFirstLeaf(candidate, this)
                       : getLastLeaf(candidate, this);
    }
    else
    {
        if (forward)
        {
            ++it;
            if (it == traversable.end())
            {
                // At end, check edge behavior
                EdgeBehavior edge =
                    scope ? scope->edgeBehavior() : EdgeBehavior::parentScope;
                switch (edge)
                {
                    case EdgeBehavior::closedLoop:
                    {
                        FocusNode* candidate = traversable.front();
                        next = getFirstLeaf(candidate, this);
                        break;
                    }
                    case EdgeBehavior::stop:
                        next = current; // Stay on current
                        break;
                    case EdgeBehavior::parentScope:
                        // Exit to parent scope - find scope's next sibling
                        if (scope)
                        {
                            return findNextFocusable(scope, forward);
                        }
                        // No parent, wrap to first leaf
                        next = getFirstLeaf(traversable.front(), this);
                        break;
                }
            }
            else
            {
                // Move to next sibling, descend to its first leaf
                next = getFirstLeaf(*it, this);
            }
        }
        else
        {
            // Going backward
            if (it == traversable.begin())
            {
                // At beginning of scope, check edge behavior
                EdgeBehavior edge =
                    scope ? scope->edgeBehavior() : EdgeBehavior::parentScope;
                switch (edge)
                {
                    case EdgeBehavior::closedLoop:
                    {
                        FocusNode* candidate = traversable.back();
                        next = getLastLeaf(candidate, this);
                        break;
                    }
                    case EdgeBehavior::stop:
                        next = current;
                        break;
                    case EdgeBehavior::parentScope:
                        // Exit to parent scope - find scope's previous sibling
                        if (scope)
                        {
                            return findNextFocusable(scope, forward);
                        }
                        // No parent, wrap to last leaf
                        next = getLastLeaf(traversable.back(), this);
                        break;
                }
            }
            else
            {
                // Move to previous sibling, descend to its last leaf
                --it;
                next = getLastLeaf(*it, this);
            }
        }
    }

    if (next && next != current)
    {
        const_cast<FocusManager*>(this)->setFocus(ref_rcp(next));
        return next;
    }
    return nullptr;
}

} // namespace rive
