/*
 * Copyright 2024 Rive
 */

#include "rive/input/focus_manager.hpp"
#include "rive/artboard.hpp"
#include "rive/artboard_host.hpp"
#include "rive/animation/listener_invocation.hpp"
#include "rive/focus_data.hpp"
#include "rive/input/focusable.hpp"
#include "rive/math/aabb.hpp"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace rive
{

static bool focusNodeEligibleForFocus(FocusNode* node)
{
  if (node == nullptr || !node->canFocus())
  {
    return false;
  }
#ifdef WITH_RIVE_TOOLS
  if (node->isCollapsed())
  {
    return false;
  }
#endif
  Focusable* f = node->focusable();
  if (f == nullptr)
  {
    // A node that never had a Focusable is externally managed — a
    // structural scope, or a target a host created through the FocusNode
    // API. There is nothing to ask about visibility, so treat it as
    // eligible, as before.
    //
    // A node that HAD one and lost it is defunct: ~FocusData clears the
    // backing when the FocusData dies. Such a node must not stay a focus
    // stop. Nothing is left that could ever report it collapsed or hidden,
    // so it would be permanently eligible — and, being unreachable through
    // its own FocusData, unremovable. Traversal still descends *through*
    // it, as it does through any non-stop, so live children stay
    // reachable.
    return !node->hadFocusable();
  }
  return f->isEligibleForFocusTraversal();
}

// A focus stop is any node navigation may land on: it opts into traversal, it
// can hold focus, and it is currently visible. Being a stop says nothing about
// whether the node has children — a container that is itself focusable and
// traversable is a stop AND a scope, and the walk visits it before its
// children.
static bool isFocusStop(FocusNode* node)
{
  if (node == nullptr || !node->canTraverse())
  {
    return false;
  }
  return focusNodeEligibleForFocus(node);
}

// `children` in traversal order. Stable, so equal tabIndex keeps hierarchy
// order. Deliberately unfiltered: the walk decides per subtree whether
// anything in it is reachable, and filtering here would answer that question a
// second time for every child it then descends into.
static std::vector<FocusNode*> sortedByTabIndex(
    const std::vector<rcp<FocusNode>>& children)
{
  std::vector<FocusNode*> result;
  result.reserve(children.size());
  for (const auto& child : children)
  {
    result.push_back(child.get());
  }
  std::stable_sort(
      result.begin(),
      result.end(),
      [](FocusNode* a, FocusNode* b) { return a->tabIndex() < b->tabIndex(); });
  return result;
}

// Whether `node` stands for content its scope hasn't built: the Focusable
// backing its parent says so. Only a virtualized list's scope is backed by
// one that can. Asked of a node with no stop in it, which needn't be empty:
// a row that was built and then recycled can keep structural nodes.
static bool isLatent(FocusNode* node)
{
  if (node->parent() == nullptr)
  {
    return false;
  }
  Focusable* owner = node->parent()->focusable();
  return owner != nullptr && owner->revealFocusChild(node, false);
}

// The first stop in `node`'s subtree in pre-order: `node` itself when it is a
// stop, else the first stop under its children in tab order. Null when the
// subtree holds none.
//
// Nothing is pruned on the way down. A node that is not a stop — a structural
// scope, an authored canFocus=false container, a node whose FocusData died —
// still has its children visited, so a parent's own flags never decide for its
// children.
//
// With `latent`, the walk also ends at the first node with no stop whose scope
// says its content isn't built yet (see Focusable::revealFocusChild): null is
// returned and `*latent` is that node. A caller that passes it checks it
// before reading null as "no stop here".
static FocusNode* firstStopInSubtree(
    FocusNode* node,
    FocusNode** latent = nullptr)
{
  if (node == nullptr)
  {
    return nullptr;
  }
  if (isFocusStop(node))
  {
    return node;
  }
  for (FocusNode* child : sortedByTabIndex(node->children()))
  {
    FocusNode* stop = firstStopInSubtree(child, latent);
    if (stop != nullptr || (latent != nullptr && *latent != nullptr))
    {
      return stop;
    }
  }
  if (latent != nullptr && isLatent(node))
  {
    *latent = node;
  }
  return nullptr;
}

// Mirror of firstStopInSubtree: the last stop in reverse pre-order, so
// children (reverse tab order) come before `node` itself.
static FocusNode* lastStopInSubtree(
    FocusNode* node,
    FocusNode** latent = nullptr)
{
  if (node == nullptr)
  {
    return nullptr;
  }
  auto children = sortedByTabIndex(node->children());
  for (auto it = children.rbegin(); it != children.rend(); ++it)
  {
    FocusNode* stop = lastStopInSubtree(*it, latent);
    if (stop != nullptr || (latent != nullptr && *latent != nullptr))
    {
      return stop;
    }
  }
  if (isFocusStop(node))
  {
    return node;
  }
  if (latent != nullptr && isLatent(node))
  {
    *latent = node;
  }
  return nullptr;
}

static FocusNode* firstStopAmong(
    const std::vector<FocusNode*>& siblings,
    FocusNode** latent = nullptr)
{
  for (FocusNode* sibling : siblings)
  {
    FocusNode* stop = firstStopInSubtree(sibling, latent);
    if (stop != nullptr || (latent != nullptr && *latent != nullptr))
    {
      return stop;
    }
  }
  return nullptr;
}

static FocusNode* lastStopAmong(
    const std::vector<FocusNode*>& siblings,
    FocusNode** latent = nullptr)
{
  for (auto it = siblings.rbegin(); it != siblings.rend(); ++it)
  {
    FocusNode* stop = lastStopInSubtree(*it, latent);
    if (stop != nullptr || (latent != nullptr && *latent != nullptr))
    {
      return stop;
    }
  }
  return nullptr;
}

// The root artboard whose tree `node` sits in, or nullptr when it can't be
// attributed to one. Walks up the focus tree to the nearest node backed by a
// Focusable — a structural scope has no artboard of its own, but something
// above it does — then up the nested-artboard chain, matching
// StateMachineInstance::rootArtboard so the two agree on what "root" means.
static const Artboard* focusRootArtboard(FocusNode* node)
{
  Artboard* artboard = nullptr;
  for (FocusNode* n = node; n != nullptr && artboard == nullptr;
       n = n->parent())
  {
    if (n->focusable() != nullptr)
    {
      artboard = n->focusable()->focusableArtboard();
    }
  }
  if (artboard == nullptr)
  {
    return nullptr;
  }
  while (artboard->host() != nullptr &&
         artboard->host()->parentArtboard() != nullptr)
  {
    artboard = artboard->host()->parentArtboard();
  }
  return artboard;
}

// True when `node` belongs to a root OTHER than the one whose update pass just
// ran, and so has to wait for its own. A node no artboard backs — one a host
// created through the FocusNode API — belongs to no root and is never
// deferred: no root's pass would ever claim it, so deferring would mean never.
static bool belongsToAnotherRoot(FocusNode* node, const Artboard* rootArtboard)
{
  const Artboard* root = focusRootArtboard(node);
  return root != nullptr && root != rootArtboard;
}

void FocusManager::dropFocusIfFocusTargetHidden()
{
  // Tested for FOCUS, not for traversal. canTraverse only means "not in
  // navigation"; a node a script, a pointer or a FocusAction focused keeps
  // that focus while it is visible, even though Tab would never pick it.
  if (m_primaryFocus == nullptr ||
      focusNodeEligibleForFocus(m_primaryFocus.get()))
  {
    return;
  }

  // Walk ancestors outward and take the first that can still offer a focus
  // stop. Children first, the ancestor itself only as a fallback: plain
  // pre-order would hand back the ancestor whenever it is a stop, and focus
  // should keep its depth — a hidden list row hands over to a sibling row,
  // not to the list.
  for (FocusNode* ancestor = m_primaryFocus->parent(); ancestor != nullptr;
       ancestor = ancestor->parent())
  {
    FocusNode* stop = firstStopAmong(sortedByTabIndex(ancestor->children()));
    if (stop == nullptr && isFocusStop(ancestor))
    {
      stop = ancestor;
    }
    if (stop != nullptr)
    {
      setFocus(ref_rcp(stop), false);
      return;
    }
  }
  clearFocus(false);
}

void FocusManager::dropFocusIfFocusTargetHidden(const Artboard* rootArtboard)
{
  // Scoped for the same reason as processPendingFocusRequests: whether the
  // target still counts as visible is read from renderOpacity and collapse,
  // and only its own root's update pass has refreshed those. Testing the
  // target itself is the right question here — unlike the descent below,
  // this is a claim about where focus already sits.
  if (m_primaryFocus == nullptr ||
      belongsToAnotherRoot(m_primaryFocus.get(), rootArtboard))
  {
    return;
  }
  dropFocusIfFocusTargetHidden();
}

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
  for (auto node : m_rootNodes)
  {
    removeManager(node);
  }
  m_primaryFocus = nullptr;
}

void FocusManager::removeManager(rcp<FocusNode> node)
{
  for (const auto& child : node->children())
  {
    removeManager(child);
  }
  node->m_manager = nullptr;
}

// Counterpart to removeManager: a node joining this manager brings its whole
// subtree with it, so every descendant's manager pointer has to be restored
// too — not just the node handed to addChild.
//
// Without this, a detach/re-add cycle leaves descendants pointing at no
// manager.
void FocusManager::assignManager(rcp<FocusNode> node)
{
  for (const auto& child : node->children())
  {
    assignManager(child);
  }
  node->m_manager = this;
}

void FocusManager::setFocus(rcp<FocusNode> node) { setFocus(node, true); }

void FocusManager::setFocus(rcp<FocusNode> node, bool endsReveal)
{
  // Focus lands exactly where it was asked to. There is no descent: a node
  // that can be focused is focused, children or not, and a node that cannot
  // (canFocus==false, collapsed, hidden, opacity 0, a defunct FocusData) is
  // a no-op rather than a redirect to some descendant the caller never
  // named. focusNodeEligibleForFocus already covers canFocus.
  if (node != nullptr && !focusNodeEligibleForFocus(node.get()))
  {
    return;
  }
  // Focus was sent somewhere, so a traversal still waiting for its target
  // (see resolveReveal) has been overtaken. Not by a move the manager makes
  // itself for a stop that left: scrolling the target into a virtualized
  // list's window can take the row focus was on out of it. Decided before
  // the change is announced, so a callback that sends focus elsewhere from
  // inside it still ends the wait.
  if (endsReveal)
  {
    m_revealTarget = nullptr;
  }

  if (node == m_primaryFocus)
  {
    return;
  }

  FocusNode* oldFocus = m_primaryFocus.get();
  m_primaryFocus = std::move(node);
  notifyFocusChange(oldFocus, m_primaryFocus.get());
}

void FocusManager::enqueueFocusRequest(PendingFocusRequest request)
{
  if (m_pendingFocusRequests.size() >= maxPendingFocusRequests)
  {
    m_pendingFocusRequests.erase(m_pendingFocusRequests.begin());
  }
  m_pendingFocusRequests.push_back(std::move(request));
}

bool FocusManager::hasPendingFocusRequests(const Artboard* rootArtboard) const
{
  for (const auto& request : m_pendingFocusRequests)
  {
    if (request.rootArtboard == rootArtboard)
    {
      return true;
    }
  }
  // A traversal waiting for its target is settled by the same drains, and
  // what is asked for meanwhile queues behind it.
  return m_revealTarget != nullptr &&
         !belongsToAnotherRoot(m_revealTarget.get(), rootArtboard);
}

bool FocusManager::applyFocusTraversal(uint32_t traversalKind)
{
  switch (traversalKind)
  {
    case 1:
      return focusPrevious();
    case 2:
      return focusUp();
    case 3:
      return focusDown();
    case 4:
      return focusLeft();
    case 5:
      return focusRight();
    case 0:
    default:
      return focusNext();
  }
}

void FocusManager::requestFocus(
    rcp<FocusNode> node,
    const Artboard* rootArtboard)
{
  if (node == nullptr)
  {
    return;
  }
  // Try now: when the target is already focusable this lands on the frame it
  // was asked for, which is what pointer- and script-driven focus expect.
  // Only a failed attempt is deferred, and a failed attempt is a true no-op
  // (setFocus rejects an ineligible node before touching primary focus), so
  // trying costs nothing. Skipped while requests are already pending for
  // this root, so a queued request can't be overtaken by a later one.
  if (!hasPendingFocusRequests(rootArtboard))
  {
    setFocus(node);
    if (hasFocus(node))
    {
      return;
    }
  }
  enqueueFocusRequest(
      {PendingFocusRequest::Kind::target, std::move(node), 0, rootArtboard});
}

void FocusManager::requestClearFocus(const Artboard* rootArtboard)
{
  // Clearing can't be blocked by stale components, so it never needs to be
  // retried — it only queues to stay behind requests already pending.
  if (!hasPendingFocusRequests(rootArtboard))
  {
    clearFocus();
    return;
  }
  enqueueFocusRequest(
      {PendingFocusRequest::Kind::clear, nullptr, 0, rootArtboard});
}

void FocusManager::requestTraversal(
    uint32_t traversalKind,
    const Artboard* rootArtboard)
{
  if (!hasPendingFocusRequests(rootArtboard) &&
      applyFocusTraversal(traversalKind))
  {
    return;
  }
  enqueueFocusRequest(
      {PendingFocusRequest::Kind::traverse,
       nullptr,
       traversalKind,
       rootArtboard});
}

bool FocusManager::applyPendingFocusRequest(const PendingFocusRequest& request)
{
  switch (request.kind)
  {
    case PendingFocusRequest::Kind::target:
      // A FocusData destroyed between queueing and draining clears its
      // node's focusable. Report that as done, not as "didn't take":
      // the target is gone, so retrying would never help.
      if (request.node == nullptr || request.node->focusable() == nullptr)
      {
        return true;
      }
      setFocus(request.node);
      return hasFocus(request.node);
    case PendingFocusRequest::Kind::clear:
      clearFocus();
      return true;
    case PendingFocusRequest::Kind::traverse:
      return applyFocusTraversal(request.traversalKind);
  }
  return true;
}

void FocusManager::processPendingFocusRequests(const Artboard* rootArtboard)
{
  drainPendingFocusRequests(
      rootArtboard,
      /*keepUnapplied=*/true,
      /*allRoots=*/false);
  resolveReveal(rootArtboard, /*allRoots=*/false, /*endOfFrame=*/false);
}

void FocusManager::processAllPendingFocusRequests()
{
  drainPendingFocusRequests(
      nullptr,
      /*keepUnapplied=*/true,
      /*allRoots=*/true);
  resolveReveal(nullptr, /*allRoots=*/true, /*endOfFrame=*/false);
}

void FocusManager::finishPendingFocusRequests(const Artboard* rootArtboard)
{
  drainPendingFocusRequests(
      rootArtboard,
      /*keepUnapplied=*/false,
      /*allRoots=*/false);
  resolveReveal(rootArtboard, /*allRoots=*/false, /*endOfFrame=*/true);
}

void FocusManager::finishAllPendingFocusRequests()
{
  drainPendingFocusRequests(
      nullptr,
      /*keepUnapplied=*/false,
      /*allRoots=*/true);
  resolveReveal(nullptr, /*allRoots=*/true, /*endOfFrame=*/true);
}

void FocusManager::drainPendingFocusRequests(
    const Artboard* rootArtboard,
    bool keepUnapplied,
    bool allRoots)
{
  if (m_pendingFocusRequests.empty())
  {
    return;
  }

  // Applying a request notifies focus/blur listeners, which can queue more
  // work, so move the pending list out before draining it. Requests from
  // other roots go back on the queue for their own root to drain.
  //
  // Everything put back goes through enqueueFocusRequest, not a bare
  // push_back: a listener firing mid-drain can enqueue alongside us, so the
  // queue has to stay under its cap here too.
  auto requests = std::move(m_pendingFocusRequests);
  m_pendingFocusRequests.clear();

  for (auto& request : requests)
  {
    if (!allRoots && request.rootArtboard != rootArtboard)
    {
      enqueueFocusRequest(std::move(request));
      continue;
    }
    if (!applyPendingFocusRequest(request) && keepUnapplied)
    {
      enqueueFocusRequest(std::move(request));
    }
  }
}

void FocusManager::clearFocus() { clearFocus(true); }

void FocusManager::clearFocus(bool endsReveal)
{
  if (endsReveal)
  {
    m_revealTarget = nullptr;
  }
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

  size_t index = parent ? parent->children().size() : m_rootNodes.size();
  addChild(std::move(parent), std::move(child), index);
}

void FocusManager::addChild(
    rcp<FocusNode> parent,
    rcp<FocusNode> child,
    size_t index)
{
  if (!child)
  {
    return;
  }
  // The child (and its subtree) joins this manager; root insertions below
  // don't pass through FocusNode::insertChild, so invalidate here.
  markFocusableContentDirty();
  if (child->parent())
  {
    child->removeFromParent();
  }
  else if (child->m_manager != nullptr && child->m_manager != this)
  {
    // The child is a root of a DIFFERENT manager (e.g. a scope migrating
    // from a nested state machine's internal manager to the parent). Erase
    // it from that manager's root list so it belongs to exactly one
    // manager.
    child->m_manager->eraseRoot(child);
  }
  else
  {
    eraseRoot(child);
  }
  // The whole subtree joins this manager, not just `child`.
  assignManager(child);
  if (parent)
  {
    parent->insertChild(index, std::move(child));
  }
  else
  {
    if (index > m_rootNodes.size())
    {
      index = m_rootNodes.size();
    }
    m_rootNodes.insert(
        m_rootNodes.begin() + static_cast<ptrdiff_t>(index),
        std::move(child));
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
    clearFocus(false);
  }

  detachChild(std::move(child));
}

void FocusManager::replaceChildren(
    rcp<FocusNode> parent,
    size_t index,
    size_t removeCount,
    const rcp<FocusNode>* nodes,
    size_t nodeCount)
{
  if (!parent)
  {
    return;
  }
  auto& children = parent->m_children;
  index = std::min(index, children.size());
  removeCount = std::min(removeCount, children.size() - index);
  if (removeCount == 0 && nodeCount == 0)
  {
    return;
  }

  // Focus held inside a child that leaves is cleared first, while the tree
  // is whole, so the blur reaches every ancestor. At most one child holds
  // it.
  for (size_t i = index; i < index + removeCount; i++)
  {
    FocusNode* child = children[i].get();
    if (!child->hasFocus())
    {
      continue;
    }
    bool stays = false;
    for (size_t j = 0; j < nodeCount && !stays; j++)
    {
      stays = nodes[j].get() == child;
    }
    if (!stays)
    {
      // No blur handler edits the focus tree as it runs: a listener's
      // actions are queued for the state machine's next advance, and the
      // rest only set state. The range is therefore still the one asked
      // for. One that did edit parent's children would need the range
      // found again by node rather than by index; the clamp only keeps
      // such a handler from sending this past the end.
#ifndef NDEBUG
      const size_t countBefore = children.size();
      const FocusNode* firstBefore = children[index].get();
      const FocusNode* lastBefore = children[index + removeCount - 1].get();
#endif
      clearFocus();
      assert(
          children.size() == countBefore &&
          children[index].get() == firstBefore &&
          children[index + removeCount - 1].get() == lastBefore);
      index = std::min(index, children.size());
      removeCount = std::min(removeCount, children.size() - index);
    }
    break;
  }

  markFocusableContentDirty();
  parent->invalidateFocusableContent();

  // A node that is already a child of parent only moves. Its parent pointer
  // is cleared until it is placed below, which is what tells the replaced
  // children that return from the ones that leave. Any other node joins
  // this manager with its subtree, as in addChild.
  for (size_t j = 0; j < nodeCount; j++)
  {
    const rcp<FocusNode>& node = nodes[j];
    if (node->m_parent == parent.get())
    {
      node->m_parent = nullptr;
      continue;
    }
    if (node->m_parent != nullptr)
    {
      node->removeFromParent();
    }
    else if (node->m_manager != nullptr)
    {
      // Only a node in a manager can be one of its roots.
      node->m_manager->eraseRoot(node);
    }
    // The whole subtree joins, even when the node itself is already in
    // this manager: a descendant may have been attached without one.
    assignManager(node);
  }
  for (size_t i = index; i < index + removeCount; i++)
  {
    const rcp<FocusNode>& child = children[i];
    if (child->m_parent == parent.get())
    {
      // Leaves for good, with its subtree, as in detachChild.
      removeManager(child);
      child->m_parent = nullptr;
#ifdef TESTING
      FocusNode::sm_childEdits++;
#endif
    }
  }

  // Size the gap once, then fill it.
  const auto gap = children.begin() + static_cast<ptrdiff_t>(index);
  if (nodeCount > removeCount)
  {
    children.insert(
        gap + static_cast<ptrdiff_t>(removeCount),
        nodeCount - removeCount,
        rcp<FocusNode>());
  }
  else if (nodeCount < removeCount)
  {
    children.erase(
        gap + static_cast<ptrdiff_t>(nodeCount),
        gap + static_cast<ptrdiff_t>(removeCount));
  }
  for (size_t j = 0; j < nodeCount; j++)
  {
    nodes[j]->m_parent = parent.get();
    children[index + j] = nodes[j];
  }
#ifdef TESTING
  FocusNode::sm_childEdits += nodeCount;
#endif
}

void FocusManager::detachChild(rcp<FocusNode> child)
{
  if (!child)
  {
    return;
  }
  // Usually redundant with the parent-side notification in
  // removeFromParent() / eraseRoot(), but load-bearing when the detached
  // node's parent isn't manager-attached (m_manager == nullptr), where
  // neither downstream mark can fire.
  markFocusableContentDirty();

  // Removing a node takes its whole subtree out of the manager, so clear
  // m_manager on every descendant too: a descendant held elsewhere (e.g. a
  // persistent NestedArtboard scope) must not retain a pointer to a manager
  // it no longer belongs to.
  removeManager(child);

  // NOTE: unlike removeChild, this intentionally does NOT clear focus. The
  // node stays alive (held by m_primaryFocus and the caller) and its
  // hasFocus flag survives, so reordering an existing node preserves focus
  // and fires no blur/focus callbacks. A genuinely re-parented focused node
  // keeps its hasFocus flag but its new ancestors won't carry it; that only
  // affects notifyFocusChange's blur-walk on a later focus change, not input
  // bubbling (which walks parent() directly).

  // Clear the manager reference
  child->m_manager = nullptr;

  if (child->parent())
  {
    child->removeFromParent();
  }
  else
  {
    eraseRoot(child);
  }
}

void FocusManager::eraseRoot(const rcp<FocusNode>& node)
{
  auto it = std::find(m_rootNodes.begin(), m_rootNodes.end(), node);
  if (it != m_rootNodes.end())
  {
    m_rootNodes.erase(it);
    // Covers root removal on THIS manager even when reached from another
    // manager's addChild (scope migration between managers).
    markFocusableContentDirty();
  }
}

namespace
{
// Raises the manager's traversing flag for the focus changes made inside one
// traversal call. Restores the previous value so a traversal started from a
// focus callback leaves the outer one flagged.
class TraversalScope
{
public:
  TraversalScope(bool& traversing)
      : m_traversing(traversing)
      , m_previous(traversing)
  {
    m_traversing = true;
  }
  ~TraversalScope() { m_traversing = m_previous; }

private:
  bool& m_traversing;
  bool m_previous;
};
} // namespace

FocusNode* FocusManager::traversalOrigin()
{
  // A traversal still waiting for its target stands where it is headed, so
  // Tab pressed again before the content arrives goes on from there rather
  // than from where focus was left.
  resolveReveal(nullptr, /*allRoots=*/true, /*endOfFrame=*/false);
  if (m_revealTarget != nullptr && m_revealTarget->manager() == this)
  {
    return m_revealTarget.get();
  }
  return m_primaryFocus.get();
}

bool FocusManager::focusNext()
{
  dropFocusIfFocusTargetHidden();
  TraversalScope traversal(m_traversing);
  return findNextFocusable(traversalOrigin(), true) != nullptr;
}

bool FocusManager::focusPrevious()
{
  dropFocusIfFocusTargetHidden();
  TraversalScope traversal(m_traversing);
  return findNextFocusable(traversalOrigin(), false) != nullptr;
}

void FocusManager::resolveReveal(
    const Artboard* rootArtboard,
    bool allRoots,
    bool endOfFrame)
{
  if (m_revealTarget == nullptr)
  {
    return;
  }
  FocusNode* target = m_revealTarget.get();
  if (target->manager() != this)
  {
    // Its item left the list.
    m_revealTarget = nullptr;
    return;
  }
  if (!allRoots && belongsToAnotherRoot(target, rootArtboard))
  {
    return;
  }
  const bool forward = m_revealForward;
  // The same walk a traversal makes, so what the target holds that isn't
  // built in its turn (a row of a virtualized list inside the row that just
  // arrived) is waited for rather than passed over for a later stop.
  FocusNode* latent = nullptr;
  FocusNode* stop = forward ? firstStopInSubtree(target, &latent)
                            : lastStopInSubtree(target, &latent);
  if (latent != nullptr && latent != target)
  {
    rcp<FocusNode> inner = ref_rcp(latent);
    if (latent->parent()->focusable()->revealFocusChild(latent, true))
    {
      m_revealTarget = std::move(inner);
      m_revealFrames = 0;
    }
    return;
  }
  if (stop != nullptr)
  {
    TraversalScope traversal(m_traversing);
    setFocus(ref_rcp(stop));
    return;
  }
  // What was just built may not have had the update that makes its stops
  // eligible, so only the end of a frame reads "no stop" as final.
  if (!endOfFrame)
  {
    return;
  }
  if (isLatent(target))
  {
    if (++m_revealFrames > maxRevealFrames)
    {
      m_revealTarget = nullptr;
    }
    return;
  }
  // Built, with nothing to land on: the walk goes on from it.
  rcp<FocusNode> from = std::move(m_revealTarget);
  TraversalScope traversal(m_traversing);
  findNextFocusable(from.get(), forward);
}

// Root-space world position from FocusNode. Live focusable geometry first;
// the node's cached bounds go stale when an ancestor host moves the
// containing artboard instance, and remain only for externally-managed nodes.
static bool getRootPosition(FocusNode* node, Vec2D& outPosition)
{
  if (!node)
  {
    return false;
  }
  if (node->focusable())
  {
    AABB bounds;
    if (node->focusable()->worldBounds(bounds))
    {
      outPosition = bounds.center();
      return true;
    }
    if (node->focusable()->worldPosition(outPosition))
    {
      return true;
    }
  }
  if (node->hasWorldBounds())
  {
    outPosition = node->worldBounds().center();
    return true;
  }
#ifdef WITH_RIVE_TOOLS
  if (node->hasWorldPosition())
  {
    outPosition = node->worldPosition();
    return true;
  }
#endif
  return false;
}

// Root-space world bounds from FocusNode; same live-first policy as
// getRootPosition.
static bool getRootBounds(FocusNode* node, AABB& outBounds)
{
  if (!node)
  {
    return false;
  }
  if (node->focusable() && node->focusable()->worldBounds(outBounds))
  {
    return true;
  }
  if (node->hasWorldBounds())
  {
    outBounds = node->worldBounds();
    return true;
  }
  return false;
}

// Every focus stop in the tree, in hierarchy order. Unlike Tab this ignores
// structure entirely — scoring is purely spatial — but it must agree with Tab
// on WHAT a stop is, so a container that is itself focusable and traversable
// is a candidate like any other.
static void collectFocusStops(
    const std::vector<rcp<FocusNode>>& nodes,
    std::vector<FocusNode*>& result)
{
  for (const auto& node : nodes)
  {
    if (isFocusStop(node.get()))
    {
      result.push_back(node.get());
    }
    collectFocusStops(node->children(), result);
  }
}

static bool subtreeHasFocusableContent(const std::vector<rcp<FocusNode>>& nodes)
{
  for (const auto& node : nodes)
  {
    // A node backed by focusable data counts even while it is currently
    // ineligible for traversal: eligibility (collapse, hidden ancestors)
    // and canFocus/canTraverse are runtime state that can change on any
    // frame, while this signal gates one-time setup in high-level
    // runtimes (e.g. attaching tab/shift+tab listeners in JS). Unbacked
    // nodes with canFocus=false are structural scopes and don't count on
    // their own.
    if ((node->focusable() != nullptr && node->focusable()->isFocusContent()) ||
        node->canFocus())
    {
      return true;
    }
    if (subtreeHasFocusableContent(node->children()))
    {
      return true;
    }
  }
  return false;
}

bool FocusManager::hasFocusableContent() const
{
  // A row being waited for is content: every built stop may have left the
  // window on the way to it, and a host that drops keyboard focus on a no
  // here (Flutter's canRequestFocus) doesn't get it back when focus lands.
  if (m_revealTarget != nullptr)
  {
    return true;
  }
  if (m_focusableContentDirty)
  {
    m_hasFocusableContent = subtreeHasFocusableContent(m_rootNodes);
    m_focusableContentDirty = false;
  }
  return m_hasFocusableContent;
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

static ScoreBreakdown scoreCandidateBoundsDetailed(
    const AABB& current,
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
      result.orthogonalDistance = std::max(
          0.0f,
          std::max(
              candidate.top() - current.bottom(),
              current.top() - candidate.bottom()));
      result.overlap = calculateOverlap(
          current.top(),
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
      result.orthogonalDistance = std::max(
          0.0f,
          std::max(
              candidate.top() - current.bottom(),
              current.top() - candidate.bottom()));
      result.overlap = calculateOverlap(
          current.top(),
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
      result.orthogonalDistance = std::max(
          0.0f,
          std::max(
              candidate.left() - current.right(),
              current.left() - candidate.right()));
      result.overlap = calculateOverlap(
          current.left(),
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
      result.orthogonalDistance = std::max(
          0.0f,
          std::max(
              candidate.left() - current.right(),
              current.left() - candidate.right()));
      result.overlap = calculateOverlap(
          current.left(),
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

static float scoreCandidateBounds(
    const AABB& current,
    const AABB& candidate,
    Direction direction)
{
  return scoreCandidateBoundsDetailed(current, candidate, direction).total;
}

// Point-based scoring fallback for nodes without bounds
static float scoreCandidatePoint(
    const Vec2D& currentPos,
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

FocusNode* FocusManager::findNodeInDirection(
    FocusNode* current,
    Direction direction) const
{
  if (!current)
  {
    return nullptr;
  }

  std::vector<FocusNode*> candidates;
  collectFocusStops(m_rootNodes, candidates);

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
      score = scoreCandidateBounds(currentBounds, candidateBounds, direction);
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
        score = scoreCandidatePoint(
            currentBounds.center(),
            candidatePos,
            direction);
      }
      else
      {
        score = scoreCandidatePoint(currentPos, candidatePos, direction);
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
  dropFocusIfFocusTargetHidden();
  TraversalScope traversal(m_traversing);
  FocusNode* next = findNodeInDirection(m_primaryFocus.get(), Direction::left);
  if (next)
  {
    setFocus(ref_rcp(next));
    return true;
  }
  return false;
}

bool FocusManager::focusRight()
{
  dropFocusIfFocusTargetHidden();
  TraversalScope traversal(m_traversing);
  FocusNode* next = findNodeInDirection(m_primaryFocus.get(), Direction::right);
  if (next)
  {
    setFocus(ref_rcp(next));
    return true;
  }
  return false;
}

bool FocusManager::focusUp()
{
  dropFocusIfFocusTargetHidden();
  TraversalScope traversal(m_traversing);
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
  dropFocusIfFocusTargetHidden();
  TraversalScope traversal(m_traversing);
  FocusNode* next = findNodeInDirection(m_primaryFocus.get(), Direction::down);
  if (next)
  {
    setFocus(ref_rcp(next));
    return true;
  }
  return false;
}

bool FocusManager::keyInput(
    Key key,
    KeyModifiers modifiers,
    bool isPressed,
    bool isRepeat)
{
  dropFocusIfFocusTargetHidden();
  // A press decides for its whole keystroke. A capture listener that names
  // a key claims every phase of it on its own, but one whose action hit()
  // the press may let the repeats and release through, and what's inside
  // it would get half a keystroke. So they run the capture phase only down
  // to the node that claimed the press, whose listeners and those outside
  // it still watch, and never bubble.
  auto pressed = std::find_if(
      m_capturedPresses.begin(),
      m_capturedPresses.end(),
      [key](const CapturedPress& press) { return press.key == key; });
  if (isPressed && !isRepeat)
  {
    // A new press decides afresh, even when a release never came.
    if (pressed != m_capturedPresses.end())
    {
      m_capturedPresses.erase(pressed);
    }
    auto claimedBy = captureKeyInputTo(
        nullptr,
        key,
        modifiers,
        isPressed,
        isRepeat);
    if (claimedBy != nullptr)
    {
      m_capturedPresses.push_back({key, std::move(claimedBy)});
      return true;
    }
  }
  else if (pressed != m_capturedPresses.end())
  {
    rcp<FocusNode> boundary = pressed->claimedBy;
    if (!isPressed)
    {
      m_capturedPresses.erase(pressed);
    }
    captureKeyInputTo(boundary.get(), key, modifiers, isPressed, isRepeat);
    return true;
  }
  else if (
      captureKeyInputTo(nullptr, key, modifiers, isPressed, isRepeat) !=
      nullptr)
  {
    return true;
  }
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

std::vector<rcp<FocusNode>> FocusManager::capturePath(
    const FocusNode* boundary) const
{
  FocusNode* node = m_primaryFocus.get();
  // Nothing below a boundary on the path takes part. One that left the
  // path has nothing on it inside it, so the whole path does.
  if (boundary != nullptr)
  {
    FocusNode* ancestor = node;
    while (ancestor != nullptr && ancestor != boundary)
    {
      ancestor = ancestor->parent();
    }
    if (ancestor != nullptr)
    {
      node = ancestor;
    }
  }
  // Most trees have nothing in the capture phase, and for them this walk
  // only reads one flag per ancestor.
  while (node != nullptr && !node->capturesInput())
  {
    node = node->parent();
  }
  std::vector<rcp<FocusNode>> path;
  if (node == nullptr)
  {
    return path;
  }
  // The capture phase runs outermost first, so collect the nodes that take
  // part and hand them back reversed. Held by reference: a listener's
  // actions can tear down part of the tree before the walk reaches it.
  for (; node != nullptr; node = node->parent())
  {
    if (node->capturesInput())
    {
      path.push_back(ref_rcp(node));
    }
  }
  std::reverse(path.begin(), path.end());
  return path;
}

bool FocusManager::captureKeyInput(
    Key key,
    KeyModifiers modifiers,
    bool isPressed,
    bool isRepeat)
{
  return captureKeyInputTo(nullptr, key, modifiers, isPressed, isRepeat) !=
         nullptr;
}

rcp<FocusNode> FocusManager::captureKeyInputTo(
    const FocusNode* boundary,
    Key key,
    KeyModifiers modifiers,
    bool isPressed,
    bool isRepeat)
{
  for (auto& node : capturePath(boundary))
  {
    if (node->captureKeyInput(key, modifiers, isPressed, isRepeat))
    {
      return std::move(node);
    }
  }
  return nullptr;
}

bool FocusManager::captureTextInput(const std::string& text)
{
  for (const auto& node : capturePath(nullptr))
  {
    if (node->captureTextInput(text))
    {
      return true;
    }
  }
  return false;
}

bool FocusManager::textInput(const std::string& text)
{
  dropFocusIfFocusTargetHidden();
  if (captureTextInput(text))
  {
    return true;
  }
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

std::string FocusManager::selectedText() const
{
  // Bubble up through the focus tree until someone handles the request,
  // mirroring how textInput routes.
  FocusNode* node = m_primaryFocus.get();
  while (node != nullptr)
  {
    Focusable* focusable = node->focusable();
    std::string text;
    if (focusable != nullptr && focusable->selectedText(text))
    {
      return text;
    }
    node = node->parent();
  }
  return std::string();
}

bool FocusManager::primaryFocusAcceptsText() const
{
  FocusNode* node = m_primaryFocus.get();
  while (node != nullptr)
  {
    Focusable* focusable = node->focusable();
    if (focusable != nullptr && focusable->acceptsTextInput())
    {
      return true;
    }
    node = node->parent();
  }
  return false;
}

bool FocusManager::gamepadDispatch(
    const ListenerInvocation& invocation,
    ScriptedDrawable** outDispatchedScriptedDrawable)
{
  dropFocusIfFocusTargetHidden();
  FocusNode* node = m_primaryFocus.get();
  while (node != nullptr)
  {
    if (node->gamepadDispatch(invocation, outDispatchedScriptedDrawable))
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
  while (current != nullptr && current != commonAncestor && current->hasFocus())
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
      Artboard* artboard = newFocus->focusable() != nullptr
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
  auto children = sortedByTabIndex(
      scope != nullptr ? scope->children() : m_rootNodes);
  children.erase(
      std::remove_if(
          children.begin(),
          children.end(),
          [](FocusNode* child) {
            return firstStopInSubtree(child) == nullptr;
          }),
      children.end());
  return children;
}

// The sibling list `node` sits in: its parent's children, or the manager's
// roots when it has no parent.
std::vector<FocusNode*> FocusManager::siblingsOf(FocusNode* node) const
{
  FocusNode* parent = node != nullptr ? node->parent() : nullptr;
  return sortedByTabIndex(parent != nullptr ? parent->children() : m_rootNodes);
}

// Pre-order walk. Forward: into `current`'s own subtree first, then out
// through each ancestor's following siblings. Backward is the exact mirror
// (reverse pre-order): the previous sibling's deepest-last stop, then the
// parent itself, then further out.
//
// edgeBehavior is asked wherever the walk would LEAVE a scope's subtree, which
// is the only place it can mean anything now that a scope can be a stop in its
// own right:
//   parentScope - keep climbing (the root list always behaves this way)
//   closedLoop  - wrap to the other end of that same subtree
//   stop        - stay put; the caller reads "no change" from next == current
//
// Only a scope is asked. A childless node's edgeBehavior has never applied and
// still doesn't (see FocusNode::edgeBehavior) — asking it here would let an
// authored closedLoop on a leaf wrap the leaf onto itself and trap focus.
FocusNode* FocusManager::nextFocusStop(
    FocusNode* current,
    bool forward,
    FocusNode** outLatent) const
{
  // Nothing focused, or focus is sitting on a node this manager does not
  // own: detachChild takes a subtree out of the manager while deliberately
  // leaving focus on it, and leaves its children hanging off it. Neither the
  // subtree nor the parent chain is ours to walk, so re-enter from the root
  // list instead. Checked before the descent below, not after it, or a
  // detached scope would hand focus to a child the manager no longer knows
  // about.
  if (current == nullptr || current->manager() != this)
  {
    auto roots = sortedByTabIndex(m_rootNodes);
    return forward ? firstStopAmong(roots, outLatent)
                   : lastStopAmong(roots, outLatent);
  }

  if (forward)
  {
    // `current`'s own subtree comes next in pre-order, whatever `current`'s
    // flags say: a parent that can't be traversed doesn't take its children
    // out of the order with it.
    for (FocusNode* child : sortedByTabIndex(current->children()))
    {
      FocusNode* stop = firstStopInSubtree(child, outLatent);
      if (stop != nullptr || *outLatent != nullptr)
      {
        return stop;
      }
    }
  }

  for (FocusNode* node = current; node != nullptr;)
  {
    // Everything inside `node` has been offered, so this step leaves its
    // subtree and its edge behavior decides whether the walk may.
    if (node->isScope())
    {
      switch (node->edgeBehavior())
      {
        case EdgeBehavior::closedLoop:
          // Wrap within this subtree. With one stop left in it (or
          // none) this lands back on `current` (or nowhere), which
          // the caller reads as no change rather than looping.
          return forward ? firstStopInSubtree(node, outLatent)
                         : lastStopInSubtree(node, outLatent);
        case EdgeBehavior::stop:
          return current;
        case EdgeBehavior::parentScope:
          break;
      }
    }

    // `node` belongs to this manager, so it is in the list it claims --
    // the check above sent everything else back to the root list. A miss
    // would leave index at siblings.size(), which reads as "past the end"
    // and lets the walk climb, so there is nothing to guard against here.
    auto siblings = siblingsOf(node);
    auto it = std::find(siblings.begin(), siblings.end(), node);
    size_t index = static_cast<size_t>(it - siblings.begin());
    if (forward)
    {
      for (size_t i = index + 1; i < siblings.size(); i++)
      {
        FocusNode* stop = firstStopInSubtree(siblings[i], outLatent);
        if (stop != nullptr || *outLatent != nullptr)
        {
          return stop;
        }
      }
    }
    else
    {
      for (size_t i = index; i-- > 0;)
      {
        FocusNode* stop = lastStopInSubtree(siblings[i], outLatent);
        if (stop != nullptr || *outLatent != nullptr)
        {
          return stop;
        }
      }
    }

    FocusNode* parent = node->parent();
    if (parent == nullptr)
    {
      // The root list has no owner to ask, so it always reads as
      // parentScope: the walk runs off the end and focus clears.
      return nullptr;
    }
    if (!forward && isFocusStop(parent))
    {
      // Reverse pre-order: a scope precedes its children, so the parent
      // is the predecessor of its first child's subtree. Still INSIDE
      // the parent's subtree, so the parent's edge has no say yet.
      return parent;
    }
    node = parent;
  }
  return nullptr;
}

FocusNode* FocusManager::findNextFocusable(FocusNode* current, bool forward)
{
  FocusNode* latent = nullptr;
  FocusNode* next = nextFocusStop(current, forward, &latent);
  if (latent != nullptr)
  {
    // The next stop in order is in content that isn't built: a row of a
    // virtualized list outside its window. Ask for it and wait, rather than
    // skip to whatever happens to be built. Focus stays where it is until
    // resolveReveal lands it.
    rcp<FocusNode> target = ref_rcp(latent);
    if (!latent->parent()->focusable()->revealFocusChild(latent, true))
    {
      return nullptr;
    }
    m_revealTarget = std::move(target);
    m_revealForward = forward;
    m_revealFrames = 0;
    return latent;
  }
  if (next == current)
  {
    // An edge that says stop, or a closed loop with nothing else left in
    // it. Reported as "didn't move" so a queued traversal request knows it
    // has nothing to retry for.
    return nullptr;
  }
  // A null `next` with focus set is the walk running off the end of the root
  // list, which clears focus. ref_rcp is null-safe and setFocus(nullptr)
  // clears, so this stays one call.
  setFocus(ref_rcp(next));
  return next;
}

void FocusManager::pressBegan() { m_pressFocus = m_primaryFocus; }

void FocusManager::pressEnded(const Vec2D& rootPosition)
{
  rcp<FocusNode> pressed = std::move(m_pressFocus);
  m_pressFocus = nullptr;
  // Focus moved during the press, or nothing was focused: nothing to blur.
  if (pressed == nullptr || pressed != m_primaryFocus)
  {
    return;
  }
  Focusable* focusable = pressed->focusable();
  if (focusable == nullptr || !focusable->blursOnOutsidePress() ||
      focusable->hitTestFocusable(rootPosition))
  {
    return;
  }
  // Direct, not queued: a focus request the press itself queued still lands
  // afterwards, as it would have blurred this target anyway.
  clearFocus();
}

} // namespace rive
