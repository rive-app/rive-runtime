#include "rive/semantic/semantic_data.hpp"
#include "rive/artboard.hpp"
#include "rive/artboard_host.hpp"
#include "rive/component_dirt.hpp"
#include "rive/container_component.hpp"
#include "rive/drawable.hpp"
#include "rive/focus_data.hpp"
#include "rive/input/focus_manager.hpp"
#include "rive/node.hpp"
#include "rive/semantic/semantic_listener.hpp"
#include "rive/semantic/semantic_manager.hpp"
#include "rive/semantic/semantic_node.hpp"
#include "rive/semantic/semantic_provider.hpp"
#include "rive/semantic/semantic_state.hpp"
#include "rive/semantic/semantic_trait.hpp"

#include "semantic_inference_registry.hpp"

using namespace rive;

SemanticData::~SemanticData()
{
    if (m_semanticNode != nullptr)
    {
        auto* manager = m_semanticNode->manager();
        if (manager != nullptr)
        {
            manager->removeChild(m_semanticNode);
        }
    }
}

// Lazily creates the SemanticNode with a globally unique ID, copies
// authored properties, applies text inference, and computes initial bounds.
rcp<SemanticNode> SemanticData::semanticNode()
{
    if (m_semanticNode == nullptr)
    {
        // Id is assigned by SemanticManager on first addChild(); until
        // then the node has id 0 and can't be looked up. All dirty-marking
        // call sites in this file guard against a null manager, so id 0
        // is safe to carry around pre-registration.
        m_semanticNode = rcp<SemanticNode>(new SemanticNode());
        m_semanticNode->coreOwner(parent());
        m_semanticNode->semanticData(this);
        m_semanticNode->role(m_Role);
        m_semanticNode->label(m_Label);
        m_semanticNode->value(m_Value);
        m_semanticNode->hint(m_Hint);
        m_semanticNode->headingLevel(m_HeadingLevel);
        m_semanticNode->stateFlags(m_StateFlags);
        // Check if a sibling FocusData exists — if so, set the Focusable
        // trait so the platform knows this node can receive focus.
        uint32_t initialTraits = m_TraitFlags;
        auto* parentNode = parent();
        if (parentNode != nullptr && parentNode->is<Node>())
        {
            for (auto* child : parentNode->as<Node>()->children())
            {
                if (child->is<FocusData>())
                {
                    initialTraits |=
                        static_cast<uint32_t>(SemanticTrait::Focusable);
                    break;
                }
            }
        }
        m_semanticNode->traitFlags(initialTraits);
        applyInferredSemanticsIfNeeded();
        // Set initial world bounds if available. Sibling shapes may not
        // have built their paths yet (deferred due to renderOpacity == 0
        // during nested artboard init). Flag a retry so updateWorldBounds
        // re-dirts once if the initial bounds are degenerate.
        m_boundsRetryPending = true;
        updateWorldBounds();
    }
    return m_semanticNode;
}

// Finds parent SemanticData by walking up the component hierarchy.
SemanticData* SemanticData::findParentSemanticData() const
{
    auto* current = parent();
    while (current != nullptr)
    {
        if (current->is<Node>())
        {
            auto* node = current->as<Node>();
            for (auto* child : node->children())
            {
                if (child->is<SemanticData>() && child != this)
                {
                    return child->as<SemanticData>();
                }
            }
        }
        current = current->parent();
    }
    return nullptr;
}

// Finds the closest SemanticNode for a component, crossing artboard
// boundaries via ArtboardHost. Walks up the component hierarchy; when
// it hits an Artboard, jumps to the host component in the parent artboard.
// Starts from the component itself so that SemanticData attached directly
// to the component (e.g. a NestedArtboard with role=button) is found.
rcp<SemanticNode> SemanticData::findClosestSemanticNode(Component* component)
{
    if (component == nullptr)
    {
        return nullptr;
    }

    auto* current = component;
    while (current != nullptr)
    {
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
            // No way to traverse up, stop searching
            return nullptr;
        }

        if (current->is<Node>())
        {
            if (auto* data = current->as<Node>()->firstChild<SemanticData>())
            {
                return data->semanticNode();
            }
        }

        current = current->parent();
    }
    return nullptr;
}

uint32_t SemanticData::semanticId() const
{
    if (m_semanticNode == nullptr)
    {
        return 0;
    }
    return m_semanticNode->id();
}

void SemanticData::setFocusedState(bool focused)
{
    if (m_semanticNode == nullptr)
    {
        return;
    }
    uint32_t flags = m_semanticNode->stateFlags();
    if (focused)
    {
        flags |= static_cast<uint32_t>(SemanticState::Focused);
    }
    else
    {
        flags &= ~static_cast<uint32_t>(SemanticState::Focused);
    }
    m_semanticNode->stateFlags(flags);
    markContentDirty();
}

bool SemanticData::requestFocus()
{
    auto* parentNode = parent();
    if (parentNode == nullptr || !parentNode->is<Node>())
    {
        return false;
    }
    auto* focusData = parentNode->as<Node>()->firstChild<FocusData>();
    if (focusData == nullptr)
    {
        return false;
    }
    auto* ab = artboard();
    if (ab == nullptr)
    {
        return false;
    }
    auto* focusManager = ab->focusManager();
    if (focusManager == nullptr)
    {
        return false;
    }
    focusManager->setFocus(focusData->focusNode());
    return true;
}

// Collapse/uncollapse: removes or re-adds SemanticNode to the tree.
// Collapse caches the manager before removal. Uncollapse re-adds with
// correct parent, recomputes bounds, and forces a WorldTransform dirt.
bool SemanticData::collapse(bool value)
{
    if (!Super::collapse(value))
    {
        return false;
    }

    if (m_semanticNode == nullptr)
    {
        return true;
    }

    if (value)
    {
        // Capture the manager before removeChild clears it.
        auto* manager = m_semanticNode->manager();
        if (manager != nullptr)
        {
            m_semanticManager = manager;
            manager->removeChild(m_semanticNode);
        }
    }
    else
    {
        // Re-add to semantic tree using the cached manager.
        auto* manager = m_semanticManager;
        if (manager == nullptr)
        {
            manager = m_semanticNode->manager();
        }
        if (manager != nullptr)
        {
            m_semanticManager = manager;
            manager->addChild(resolveParentNode(), m_semanticNode);

            // Re-apply current bounds and content so the node is up to date.
            m_boundsRetryPending = true;
            updateWorldBounds();

            // Force a WorldTransform update so bounds are recomputed
            // in the next update cycle after layout has been recalculated.
            addDirt(ComponentDirt::WorldTransform, false);

            applyInferredSemanticsIfNeeded();
        }
    }

    return true;
}

// Register dependency on parent's world transform so update() fires
// when the parent node moves.
void SemanticData::buildDependencies()
{
    Super::buildDependencies();
    auto* parentComponent = parent();
    if (parentComponent != nullptr)
    {
        parentComponent->addDependent(this);
    }
}

// Update cycle handler. Called when the parent node's world transform
// changes or when a sibling Shape's geometry is rebuilt. Re-evaluates
// inferred semantics and recomputes world-space bounds.
void SemanticData::update(ComponentDirt value)
{
    // Paint dirt (opacity/color) does not affect tree membership — see
    // shouldExcludeFromSemanticTree(). Only Collapsed + Hidden-state matter.
    if (hasDirt(value, ComponentDirt::Collapsed))
    {
        syncSemanticTreeVisibility();
    }

    if ((value & ComponentDirt::WorldTransform) ==
        ComponentDirt::WorldTransform)
    {
        applyInferredSemanticsIfNeeded();
        updateWorldBounds();
    }

    if ((value & ComponentDirt::Path) == ComponentDirt::Path)
    {
        updateWorldBounds();
    }
}

// Applies inferred semantics if no explicit role/label was authored.
// Currently supports Text → role=label with concatenated run text.
void SemanticData::applyInferredSemanticsIfNeeded()
{
    if (m_Role != 0 || !m_Label.empty() || m_semanticNode == nullptr)
    {
        return;
    }
    ResolvedSemanticData inferred;
    if (!resolveInferredSemantics(parent(), inferred))
    {
        return;
    }
    if (m_semanticNode->role() == inferred.role &&
        m_semanticNode->label() == inferred.label)
    {
        return;
    }
    m_semanticNode->role(inferred.role);
    m_semanticNode->label(inferred.label);
    markContentDirty();
}

// Marks semantic content as dirty so manager emits updated diffs.
void SemanticData::markContentDirty()
{
    if (m_semanticNode == nullptr)
    {
        return;
    }
    auto* manager = m_semanticNode->manager();
    if (manager != nullptr && parent() != nullptr)
    {
        manager->markNodeDirty(semanticId(), SemanticDirt::Content);
    }
}

// Property change handlers. Called when role/label/stateFlags change from
// deserialization, animation, or data binding. Pushes the new value to the
// SemanticNode and marks content-dirty for the next diff.
void SemanticData::roleChanged()
{
    if (m_semanticNode == nullptr || m_semanticNode->role() == m_Role)
    {
        return;
    }
    m_semanticNode->role(m_Role);
    markContentDirty();
}

void SemanticData::labelChanged()
{
    if (m_semanticNode == nullptr || m_semanticNode->label() == m_Label)
    {
        return;
    }
    m_semanticNode->label(m_Label);
    markContentDirty();
}

void SemanticData::stateFlagsChanged()
{
    if (m_semanticNode == nullptr)
    {
        return;
    }
    if (m_semanticNode->stateFlags() == m_StateFlags)
    {
        return;
    }
    m_semanticNode->stateFlags(m_StateFlags);
    markContentDirty();
    // Hidden bit changes affect tree membership, not just content.
    syncSemanticTreeVisibility();
}

void SemanticData::traitFlagsChanged()
{
    if (m_semanticNode == nullptr ||
        m_semanticNode->traitFlags() == m_TraitFlags)
    {
        return;
    }
    m_semanticNode->traitFlags(m_TraitFlags);
    markContentDirty();
}

void SemanticData::valueChanged()
{
    if (m_semanticNode == nullptr || m_semanticNode->value() == m_Value)
    {
        return;
    }
    m_semanticNode->value(m_Value);
    markContentDirty();
}

void SemanticData::hintChanged()
{
    if (m_semanticNode == nullptr || m_semanticNode->hint() == m_Hint)
    {
        return;
    }
    m_semanticNode->hint(m_Hint);
    markContentDirty();
}

void SemanticData::headingLevelChanged()
{
    if (m_semanticNode == nullptr ||
        m_semanticNode->headingLevel() == m_HeadingLevel)
    {
        return;
    }
    m_semanticNode->headingLevel(m_HeadingLevel);
    markContentDirty();
}

rcp<SemanticNode> SemanticData::resolveParentNode() const
{
    auto* localParent = findParentSemanticData();
    rcp<SemanticNode> parentNode =
        localParent != nullptr ? localParent->semanticNode() : nullptr;
    if (parentNode == nullptr)
    {
        auto* ab = artboard();
        if (ab != nullptr)
        {
            parentNode = ab->semanticBoundaryNode();
        }
    }
    return parentNode;
}

bool SemanticData::shouldExcludeFromSemanticTree() const
{
    if (hasSemanticState(m_StateFlags, SemanticState::Hidden))
    {
        return true;
    }

    auto* current = parent();
    if (current == nullptr)
    {
        return true;
    }
    if (current->isCollapsed())
    {
        return true;
    }
    if (current->is<Drawable>() && current->as<Drawable>()->isHidden())
    {
        return true;
    }
    // Note: opacity 0 does NOT exclude from the semantic tree. A
    // visually hidden component may still be interactive and should
    // remain accessible to screen readers. Use the Hidden semantic
    // state flag for intentional exclusion.
    return false;
}

void SemanticData::syncSemanticTreeVisibility()
{
    if (m_semanticNode == nullptr)
    {
        return;
    }

    const bool shouldExclude = shouldExcludeFromSemanticTree();
    if (shouldExclude == m_excludedFromTree)
    {
        return;
    }
    m_excludedFromTree = shouldExclude;

    auto* manager = m_semanticNode->manager();
    if (shouldExclude)
    {
        if (manager != nullptr)
        {
            m_semanticManager = manager;
            manager->removeChild(m_semanticNode);
        }
        return;
    }

    // Already tracked in the semantic tree.
    if (manager != nullptr)
    {
        return;
    }

    manager = m_semanticManager;

    if (manager == nullptr)
    {
        return;
    }

    manager->addChild(resolveParentNode(), m_semanticNode);

    m_boundsRetryPending = true;
    updateWorldBounds();
    applyInferredSemanticsIfNeeded();
}

void SemanticData::updateWorldBounds()
{
    if (m_semanticNode == nullptr)
    {
        return;
    }
    auto* parentComponent = parent();
    if (parentComponent == nullptr)
    {
        return;
    }
    auto bounds = SemanticProvider::semanticBounds(parentComponent);
    if (bounds.isEmptyOrNaN() && m_boundsRetryPending)
    {
        // Bounds not ready (e.g. sibling Shape paths still deferred).
        // Re-dirty so updateComponents() runs another iteration after
        // sibling paths have been rebuilt in this same pass.
        addDirt(ComponentDirt::WorldTransform, false);
        return;
    }
    m_boundsRetryPending = false;
    if (bounds == m_semanticNode->bounds())
    {
        return;
    }
    m_semanticNode->bounds(bounds);
    auto* manager = m_semanticNode->manager();
    if (manager != nullptr)
    {
        manager->markNodeDirty(semanticId(), SemanticDirt::Bounds);
    }
}

void SemanticData::addSemanticListener(SemanticListener* listener)
{
    m_semanticListeners.push_back(listener);
}

void SemanticData::removeSemanticListener(SemanticListener* listener)
{
    auto it = std::find(m_semanticListeners.begin(),
                        m_semanticListeners.end(),
                        listener);
    if (it != m_semanticListeners.end())
    {
        m_semanticListeners.erase(it);
    }
}

void SemanticData::fireSemanticTap()
{
    for (auto* listener : m_semanticListeners)
    {
        listener->onSemanticTap();
    }
}

void SemanticData::fireSemanticIncrease()
{
    for (auto* listener : m_semanticListeners)
    {
        listener->onSemanticIncrease();
    }
}

void SemanticData::fireSemanticDecrease()
{
    for (auto* listener : m_semanticListeners)
    {
        listener->onSemanticDecrease();
    }
}
