#include "rive/semantic/semantic_data.hpp"
#include "rive/artboard.hpp"
#include "rive/component_dirt.hpp"
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

// Read only when the node is created or its own visibility changes, never at
// flatten: ScriptedTransition flags a page Hidden while it composites the page
// itself, and that page is still on screen.
static bool isDrawableHidden(const Component* owner)
{
  return owner != nullptr && owner->is<Drawable>() &&
         owner->as<Drawable>()->isHidden();
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
    m_ownerHidden = isDrawableHidden(parent());
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
          initialTraits |= static_cast<uint32_t>(SemanticTrait::Focusable);
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

// Finds parent SemanticData by walking up the component hierarchy. The walk
// starts above the component this SemanticData is attached to: another
// SemanticData on that same component is a sibling, and treating siblings as
// parents links them into a cycle that detaches all of them from the tree.
SemanticData* SemanticData::findParentSemanticData() const
{
  auto* owner = parent();
  auto* current = owner != nullptr ? owner->parent() : nullptr;
  while (current != nullptr)
  {
    if (current->is<Node>())
    {
      if (auto* data = current->as<Node>()->firstChild<SemanticData>())
      {
        return data;
      }
    }
    current = current->parent();
  }
  return nullptr;
}

// Finds the node a hosted artboard's boundary hangs under: the closest
// SemanticData at or above the component in its own artboard, else that
// artboard's boundary (null for a root). It never looks past the artboard,
// so a host's subtree contains everything below it and hiding the host's
// boundary hides all of it. SemanticData on the artboard itself is skipped,
// as it always was, so where nested content reads is unchanged.
rcp<SemanticNode> SemanticData::findClosestSemanticNode(Component* component)
{
  for (auto* current = component; current != nullptr;
       current = current->parent())
  {
    if (current->is<Artboard>())
    {
      return current->as<Artboard>()->semanticBoundaryNode();
    }
    if (current->is<Node>())
    {
      if (auto* data = current->as<Node>()->firstChild<SemanticData>())
      {
        return data->semanticNode();
      }
    }
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

// A collapsed SemanticData stays in the tree but is skipped when the tree is
// flattened; see syncSemanticTreeVisibility.
bool SemanticData::collapse(bool value)
{
  if (!Super::collapse(value))
  {
    return false;
  }
  syncSemanticTreeVisibility();
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
  // Paint dirt (opacity/color) does not affect whether the node is shown
  // (see isExcludedFromSemanticTree). Only Collapsed dirt is handled here;
  // the Hidden state goes through stateFlagsChanged.
  if (hasDirt(value, ComponentDirt::Collapsed))
  {
    syncSemanticTreeVisibility();
  }

  if ((value & ComponentDirt::WorldTransform) == ComponentDirt::WorldTransform)
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
  const bool wasHidden = hasSemanticState(
      m_semanticNode->stateFlags(),
      SemanticState::Hidden);
  m_semanticNode->stateFlags(m_StateFlags);
  markContentDirty();
  // The Hidden bit decides whether the node is shown, not just its content.
  if (wasHidden != hasSemanticState(m_StateFlags, SemanticState::Hidden))
  {
    syncSemanticTreeVisibility();
  }
}

void SemanticData::traitFlagsChanged()
{
  if (m_semanticNode == nullptr || m_semanticNode->traitFlags() == m_TraitFlags)
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

bool SemanticData::isExcludedFromSemanticTree() const
{
  if (isCollapsed() || hasSemanticState(m_StateFlags, SemanticState::Hidden))
  {
    return true;
  }

  // A SemanticData with no owner has nothing hiding it.
  auto* current = parent();
  if (current == nullptr)
  {
    return false;
  }
  if (current->isCollapsed())
  {
    return true;
  }
  if (m_ownerHidden)
  {
    return true;
  }
  // Note: opacity 0 does NOT exclude from the semantic tree. A
  // visually hidden component may still be interactive and should
  // remain accessible to screen readers. Use the Hidden semantic
  // state flag for intentional exclusion.
  return false;
}

// Whether a node is shown is read when the tree is flattened (see
// isExcludedFromSemanticTree), so a change here only needs a re-flatten.
// Coming back, the node refreshes its bounds and content, which may have
// gone stale while it was hidden.
void SemanticData::syncSemanticTreeVisibility()
{
  // Read even while unregistered: a node is reused when a new state
  // machine registers it, without another sync.
  m_ownerHidden = isDrawableHidden(parent());
  if (m_semanticNode == nullptr)
  {
    return;
  }
  auto* manager = m_semanticNode->manager();
  if (manager == nullptr)
  {
    return;
  }
  manager->markDirty(SemanticDirt::Structure);
  if (isExcludedFromSemanticTree())
  {
    return;
  }
  m_boundsRetryPending = true;
  updateWorldBounds();
  addDirt(ComponentDirt::WorldTransform, false);
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
  auto it = std::find(
      m_semanticListeners.begin(),
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
