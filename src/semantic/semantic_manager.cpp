#include "rive/semantic/semantic_manager.hpp"

#include "rive/artboard.hpp"
#include "rive/component.hpp"
#include "rive/node.hpp"
#include "rive/semantic/semantic_data.hpp"
#include "rive/semantic/semantic_node.hpp"
#include "rive/semantic/semantic_provider.hpp"
#include "rive/semantic/semantic_role.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace rive;

namespace
{
// ---------------------------------------------------------------------------
// Label derivation helpers.
//
// Interactive elements (button, toggle, etc.) with no explicit label derive
// their accessible label from child semantic nodes. Children that contribute
// to the derived label are "absorbed" — excluded from the flat accessibility
// output so screen readers don't announce them twice.
// ---------------------------------------------------------------------------

// Trim leading/trailing whitespace and collapse internal runs of whitespace
// to a single space. Preserves logical reading order.
static std::string normalizeLabel(const std::string& input)
{
    std::string result;
    result.reserve(input.size());
    bool lastWasSpace = true; // treats start as whitespace → trims leading
    for (char c : input)
    {
        if (static_cast<unsigned char>(c) <= ' ')
        {
            if (!lastWasSpace && !result.empty())
            {
                result += ' ';
                lastWasSpace = true;
            }
        }
        else
        {
            result += c;
            lastWasSpace = false;
        }
    }
    // Trim trailing space.
    if (!result.empty() && result.back() == ' ')
    {
        result.pop_back();
    }
    return result;
}

// Walks the subtree rooted at `node` in tree order, collecting text and
// image labels. All visited (non-interactive) nodes are added to
// `absorbedIds`. Nested interactive elements are left untouched — they
// are not absorbed and their subtrees are not descended into.
static void collectLabelsFromSubtree(const rcp<SemanticNode>& node,
                                     std::string& textOut,
                                     std::string& imageTextOut,
                                     std::unordered_set<uint32_t>& absorbedIds)
{
    auto role = static_cast<SemanticRole>(node->role());

    // Nested interactive element: don't absorb, don't descend.
    if (isInteractiveRole(role))
    {
        return;
    }

    absorbedIds.insert(node->id());

    if (role == SemanticRole::text && !node->label().empty())
    {
        if (!textOut.empty())
        {
            textOut += ' ';
        }
        textOut += node->label();
    }
    else if (role == SemanticRole::image && !node->label().empty())
    {
        // Only keep the first image label as fallback.
        if (imageTextOut.empty())
        {
            imageTextOut = node->label();
        }
    }

    for (const auto& child : node->children())
    {
        collectLabelsFromSubtree(child, textOut, imageTextOut, absorbedIds);
    }
}

// Reusable scratch state for a single deriveLabelForInteractiveNodes pass.
// Allocation buffers are owned here and clear()-ed between interactive
// nodes so capacity is kept across the walk.
struct LabelDerivationScratch
{
    std::string textLabel;
    std::string imageLabel;
    std::unordered_set<uint32_t> absorbed;
};

static void deriveLabelVisit(
    const rcp<SemanticNode>& node,
    std::unordered_map<uint32_t, std::string>& derivedLabels,
    std::unordered_set<uint32_t>& excludedIds,
    LabelDerivationScratch& scratch)
{
    if (isInteractiveRole(node->role()) && node->label().empty())
    {
        scratch.textLabel.clear();
        scratch.imageLabel.clear();
        scratch.absorbed.clear();

        for (const auto& child : node->children())
        {
            collectLabelsFromSubtree(child,
                                     scratch.textLabel,
                                     scratch.imageLabel,
                                     scratch.absorbed);
        }

        // Text takes priority; images are fallback only (spec §3.3).
        std::string derived = normalizeLabel(scratch.textLabel);
        if (derived.empty())
        {
            derived = normalizeLabel(scratch.imageLabel);
        }

        if (!derived.empty())
        {
            derivedLabels[node->id()] = std::move(derived);
            excludedIds.insert(scratch.absorbed.begin(),
                               scratch.absorbed.end());
        }
    }

    for (const auto& child : node->children())
    {
        if (excludedIds.count(child->id()) == 0)
        {
            deriveLabelVisit(child, derivedLabels, excludedIds, scratch);
        }
    }
}

// Pre-pass that computes derived labels for interactive nodes and builds
// the set of absorbed (excluded) node IDs.
//
// Rules (from spec):
//   1. Explicit label wins — skip derivation.
//   2. Text children contribute in tree order; images only if no text.
//   3. Hidden / non-semantic nodes were already pruned before this point.
//   4. textField never auto-derives (not in isInteractiveRole).
//   5. Nested interactive children are not absorbed.
static void deriveLabelForInteractiveNodes(
    const std::vector<rcp<SemanticNode>>& roots,
    std::unordered_map<uint32_t, std::string>& derivedLabels,
    std::unordered_set<uint32_t>& excludedIds)
{
    LabelDerivationScratch scratch;
    for (const auto& root : roots)
    {
        deriveLabelVisit(root, derivedLabels, excludedIds, scratch);
    }
}

// ---------------------------------------------------------------------------
// Tree flattening.
// ---------------------------------------------------------------------------

// Depth-first pre-order flattening of the SemanticNode tree into a flat
// array of SemanticsDiffNode structs. Nodes in `excludedIds` are skipped
// but their non-excluded children are reparented to the excluded node's
// parent (handles the nested-interactive-inside-absorbed-group case).
// Interactive nodes with entries in `derivedLabels` use the derived label
// instead of their (empty) canonical label.
static void flattenSemanticNode(
    const rcp<SemanticNode>& node,
    int32_t parentId,
    uint32_t& siblingCounter,
    std::vector<SemanticsDiffNode>& out,
    const std::unordered_set<uint32_t>& excludedIds,
    const std::unordered_map<uint32_t, std::string>& derivedLabels)
{
    if (excludedIds.count(node->id()) || node->isBoundaryNode())
    {
        // Node is absorbed or is a structural boundary node — skip it
        // but reparent any non-excluded children to the absorbing parent.
        for (const auto& child : node->children())
        {
            flattenSemanticNode(child,
                                parentId,
                                siblingCounter,
                                out,
                                excludedIds,
                                derivedLabels);
        }
        return;
    }

    SemanticsDiffNode flat;
    flat.id = node->id();
    flat.role = node->role();

    auto derivedIt = derivedLabels.find(node->id());
    flat.label =
        (derivedIt != derivedLabels.end()) ? derivedIt->second : node->label();

    flat.value = node->value();
    flat.hint = node->hint();
    flat.stateFlags = node->stateFlags();
    flat.traitFlags = node->traitFlags();
    flat.headingLevel = node->headingLevel();
    const auto bounds = node->bounds();
    flat.minX = bounds.minX;
    flat.minY = bounds.minY;
    flat.maxX = bounds.maxX;
    flat.maxY = bounds.maxY;
    flat.parentId = parentId;
    flat.siblingIndex = siblingCounter++;
    out.push_back(flat);

    uint32_t childSiblingCounter = 0;
    for (const auto& child : node->children())
    {
        flattenSemanticNode(child,
                            static_cast<int32_t>(flat.id),
                            childSiblingCounter,
                            out,
                            excludedIds,
                            derivedLabels);
    }
}

// Entry point for full-tree flattening. Iterates all root nodes (nodes with
// no semantic parent, i.e. top-level accessible elements). Root nodes have
// parentId = -1. The reserveHint pre-allocates to avoid repeated
// vector reallocation during the walk.
static std::vector<SemanticsDiffNode> flattenFromSemanticNodes(
    const std::vector<rcp<SemanticNode>>& roots,
    size_t reserveHint,
    const std::unordered_set<uint32_t>& excludedIds,
    const std::unordered_map<uint32_t, std::string>& derivedLabels)
{
    std::vector<SemanticsDiffNode> out;
    if (reserveHint != 0)
    {
        out.reserve(reserveHint);
    }
    uint32_t rootSiblingCounter = 0;
    for (const auto& root : roots)
    {
        flattenSemanticNode(root,
                            -1,
                            rootSiblingCounter,
                            out,
                            excludedIds,
                            derivedLabels);
    }
    return out;
}

// Maps parentId → ordered list of child ids.
// Used by buildDiffFromFlats to detect reordering and reparenting.
using ChildrenByParent = std::unordered_map<int32_t, std::vector<uint32_t>>;

// Builds the ChildrenByParent map from a flat snapshot. Groups nodes by
// parentId, then sorts each group by siblingIndex to produce the
// canonical child ordering. This ordering is compared frame-to-frame to
// detect reorder/reparent changes, which are emitted as
// SemanticsChildrenUpdate entries in the diff.
static ChildrenByParent buildChildrenByParent(
    const std::vector<SemanticsDiffNode>& nodes)
{
    // First pass: group (siblingIndex, id) pairs by parent.
    std::unordered_map<int32_t, std::vector<std::pair<uint32_t, uint32_t>>>
        grouped;
    grouped.reserve(nodes.size());
    for (const auto& node : nodes)
    {
        grouped[node.parentId].emplace_back(node.siblingIndex, node.id);
    }

    // Second pass: sort each group by siblingIndex and extract ids
    // in order. The result is a map from parentId to an ordered child list
    // that can be directly compared via == for change detection.
    ChildrenByParent out;
    out.reserve(grouped.size());
    for (auto& entry : grouped)
    {
        auto& nodeList = entry.second;
        std::sort(nodeList.begin(),
                  nodeList.end(),
                  [](const std::pair<uint32_t, uint32_t>& a,
                     const std::pair<uint32_t, uint32_t>& b) {
                      return a.first < b.first;
                  });
        auto& children = out[entry.first];
        children.reserve(nodeList.size());
        for (const auto& n : nodeList)
        {
            children.push_back(n.second);
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Visual position ordering.
//
// Screen-reader traversal order is determined by the order of children in
// each SemanticNode's children vector. These helpers sort children by their
// visual position (top-to-bottom, left-to-right) and detect when bounds
// changes have caused siblings to move past each other.
// ---------------------------------------------------------------------------

// Returns true if the children with valid bounds are already in visual
// order (top-to-bottom, left-to-right). Nodes with empty/NaN bounds are
// skipped — they have no visual position to compare.
static bool childrenInVisualOrder(
    const std::vector<rcp<SemanticNode>>& children)
{
    AABB prevBounds;
    bool hasPrev = false;
    for (const auto& child : children)
    {
        const auto b = child->bounds();
        if (b.isEmptyOrNaN())
        {
            continue;
        }
        if (hasPrev)
        {
            if (b.minY < prevBounds.minY ||
                (b.minY == prevBounds.minY && b.minX < prevBounds.minX))
            {
                return false;
            }
        }
        prevBounds = b;
        hasPrev = true;
    }
    return true;
}

// Single post-order walk that:
//   1. Computes container bounds for nodes that have no drawable geometry
//      of their own (boundary nodes always; leaf nodes with empty/NaN
//      self-bounds).
//   2. Records (on the incremental path) which of those container bounds
//      actually changed vs. the last snapshot.
//   3. Optionally checks whether each parent's children are still in
//      visual order — relevant only when we might take the incremental
//      diff path and need to detect re-sorts from bounds-only changes.
//
// Merging the three jobs into one walk replaces two full-tree passes
// plus a pre-built postOrder vector. Bounds are still resolved
// bottom-up because children recurse before the parent does its merge.
static void reconcileBoundsAndCheckOrder(
    const rcp<SemanticNode>& node,
    bool reorderCheckEnabled,
    bool& needsReorder,
    std::vector<uint32_t>& containerBoundsChangedIds,
    const std::unordered_map<uint32_t, size_t>& snapshotIndex,
    const std::vector<SemanticsDiffNode>& lastFlatSnapshot)
{
    // Depth-first: process children first so their bounds are settled
    // before a parent tries to merge them.
    for (const auto& child : node->children())
    {
        reconcileBoundsAndCheckOrder(child,
                                     reorderCheckEnabled,
                                     needsReorder,
                                     containerBoundsChangedIds,
                                     snapshotIndex,
                                     lastFlatSnapshot);
    }

    // Container bounds computation. Non-boundary nodes that already have
    // valid explicit bounds keep them; boundary nodes are container-derived
    // and must always recompute.
    const auto selfBounds = node->bounds();
    if (node->isBoundaryNode() || selfBounds.isEmptyOrNaN())
    {
        AABB merged = AABB::forExpansion();
        bool hasChildBounds = false;
        for (const auto& child : node->children())
        {
            const auto childBounds = child->bounds();
            if (childBounds.isEmptyOrNaN())
            {
                continue;
            }
            merged.expand(childBounds);
            hasChildBounds = true;
        }

        if (hasChildBounds)
        {
            // On the incremental path, record this node only if its computed
            // bounds actually differ from the snapshot. Avoids no-op
            // updatedGeometry entries.
            if (!snapshotIndex.empty())
            {
                auto idxIt = snapshotIndex.find(node->id());
                if (idxIt != snapshotIndex.end())
                {
                    const auto& snap = lastFlatSnapshot[idxIt->second];
                    if (snap.minX != merged.minX || snap.minY != merged.minY ||
                        snap.maxX != merged.maxX || snap.maxY != merged.maxY)
                    {
                        containerBoundsChangedIds.push_back(node->id());
                    }
                }
            }
            node->bounds(merged);
        }
    }

    // Visual-order check. Runs once the node's children's bounds are final
    // (true by construction — post-order). Short-circuits once we know a
    // reorder is needed; we still keep walking the tree so the bounds work
    // completes, but skip the compare.
    if (reorderCheckEnabled && !needsReorder && node->children().size() > 1 &&
        !childrenInVisualOrder(node->children()))
    {
        needsReorder = true;
    }
}

// Compares two flat snapshots (current vs. previous) and produces a
// SemanticsDiff with added, removed, moved, updatedSemantic,
// updatedGeometry, and childrenUpdated arrays.
static SemanticsDiff buildDiffFromFlats(
    const std::vector<SemanticsDiffNode>& currentFlat,
    const std::vector<SemanticsDiffNode>& previousFlat,
    uint64_t treeVersion)
{
    SemanticsDiff diff;
    diff.frameNumber = Artboard::frameId();
    diff.treeVersion = treeVersion;

    uint32_t rootCount = 0;
    uint32_t rootId = 0;
    for (const auto& node : currentFlat)
    {
        if (node.parentId != -1)
        {
            continue;
        }
        rootCount++;
        rootId = node.id;
    }
    if (rootCount == 1)
    {
        diff.rootId = rootId;
    }

    // First-frame fast path: no previous snapshot to diff against, so
    // everything is an addition. Also emit the full children ordering map
    // so the platform can set up its tree structure in one pass.
    //
    // Emit childrenUpdated entries in the order parents first appear as
    // parentId in currentFlat (tree pre-order). Consumers applying the diff
    // sequentially see parents attached before their children's orderings.
    if (previousFlat.empty())
    {
        const auto currentChildrenByParent = buildChildrenByParent(currentFlat);
        std::unordered_set<int32_t> seenParents;
        seenParents.reserve(currentChildrenByParent.size());
        for (const auto& node : currentFlat)
        {
            if (!seenParents.insert(node.parentId).second)
            {
                continue;
            }
            auto it = currentChildrenByParent.find(node.parentId);
            if (it == currentChildrenByParent.end())
            {
                continue;
            }
            SemanticsChildrenUpdate update;
            update.parentId = it->first;
            update.childIds = it->second;
            diff.childrenUpdated.push_back(std::move(update));
        }

        diff.added.reserve(currentFlat.size());
        for (const auto& node : currentFlat)
        {
            diff.added.push_back(node);
        }
        return diff;
    }

    // Build O(1) lookup maps for both snapshots. These allow per-node
    // comparisons without scanning the full arrays.
    std::unordered_map<uint32_t, const SemanticsDiffNode*> currentById;
    currentById.reserve(currentFlat.size());
    for (const auto& node : currentFlat)
    {
        currentById[node.id] = &node;
    }

    std::unordered_map<uint32_t, const SemanticsDiffNode*> previousById;
    previousById.reserve(previousFlat.size());
    for (const auto& node : previousFlat)
    {
        previousById[node.id] = &node;
    }

    // Build children-by-parent maps for both snapshots. Used later
    // to detect reordering and reparenting of child lists.
    const auto currentChildrenByParent = buildChildrenByParent(currentFlat);
    const auto previousChildrenByParent = buildChildrenByParent(previousFlat);

    // Removal detection: any node in the previous snapshot that is absent
    // from the current snapshot has been removed. The platform layer uses
    // this to tear down the corresponding accessibility node. Iterate
    // previousFlat (tree pre-order) so removed ids are emitted
    // deterministically.
    for (const auto& previousNode : previousFlat)
    {
        if (currentById.find(previousNode.id) == currentById.end())
        {
            diff.removed.push_back(previousNode.id);
        }
    }

    // Added / moved / content / geometry detection for nodes in current.
    // For each current node:
    //   - If absent from previous → added (new node appeared).
    //   - If present in previous → compare fields:
    //     - parentId or siblingIndex changed → moved (reparent or
    //       reorder within same parent).
    //     - role, label, or stateFlags changed → updatedSemantic. The
    //       platform layer re-reads these to update screen reader properties.
    //     - bounds changed → updatedGeometry. The platform layer updates
    //       the hit-test region and visual position.
    //   A single node can appear in multiple diff arrays (e.g. both moved
    //   and updatedSemantic) if multiple aspects changed simultaneously.
    //
    // Iterate currentFlat (tree pre-order) so parents land in the added/
    // moved arrays before their children, and iteration is deterministic
    // frame-to-frame. currentById/previousById are used for O(1) lookup.
    for (const auto& currentNode : currentFlat)
    {
        auto previousIt = previousById.find(currentNode.id);
        if (previousIt == previousById.end())
        {
            diff.added.push_back(currentNode);
            continue;
        }

        const auto& previousNode = *previousIt->second;

        // Moved detection: parent changed (reparent) or sibling position
        // changed (reorder within the same parent).
        const bool moved =
            previousNode.parentId != currentNode.parentId ||
            previousNode.siblingIndex != currentNode.siblingIndex;
        if (moved)
        {
            diff.moved.push_back(currentNode);
        }

        // Semantic content detection: any of role, label, stateFlags, or
        // traitFlags changed. These affect how a screen reader
        // announces the node (e.g. "button, selected" vs "button").
        if (previousNode.role != currentNode.role ||
            previousNode.label != currentNode.label ||
            previousNode.stateFlags != currentNode.stateFlags ||
            previousNode.traitFlags != currentNode.traitFlags)
        {
            diff.updatedSemantic.push_back(currentNode);
        }

        // Geometry detection: bounds changed. Affects hit-test region and
        // visual overlay positioning.
        if (previousNode.minX != currentNode.minX ||
            previousNode.minY != currentNode.minY ||
            previousNode.maxX != currentNode.maxX ||
            previousNode.maxY != currentNode.maxY)
        {
            diff.updatedGeometry.push_back({currentNode.id,
                                            currentNode.minX,
                                            currentNode.minY,
                                            currentNode.maxX,
                                            currentNode.maxY});
        }
    }

    // Children ordering updates. Parent ids are collected in currentFlat
    // tree order, then appended with any parents that only exist in
    // previousFlat. This gives a deterministic emission order matching how
    // a consumer would walk the new tree.
    std::vector<int32_t> orderedParents;
    std::unordered_set<int32_t> seenParents;
    orderedParents.reserve(currentChildrenByParent.size() +
                           previousChildrenByParent.size());
    seenParents.reserve(orderedParents.capacity());
    for (const auto& node : currentFlat)
    {
        if (seenParents.insert(node.parentId).second)
        {
            orderedParents.push_back(node.parentId);
        }
    }
    for (const auto& node : previousFlat)
    {
        if (seenParents.insert(node.parentId).second)
        {
            orderedParents.push_back(node.parentId);
        }
    }

    for (const auto parentId : orderedParents)
    {
        auto currentIt = currentChildrenByParent.find(parentId);
        auto previousIt = previousChildrenByParent.find(parentId);
        std::vector<uint32_t> empty;
        const auto* currentChildren = currentIt == currentChildrenByParent.end()
                                          ? &empty
                                          : &currentIt->second;
        const auto* previousChildren =
            previousIt == previousChildrenByParent.end() ? &empty
                                                         : &previousIt->second;
        if (*currentChildren != *previousChildren)
        {
            SemanticsChildrenUpdate update;
            update.parentId = parentId;
            update.childIds = *currentChildren;
            diff.childrenUpdated.push_back(std::move(update));
        }
    }

    return diff;
}

} // namespace

// Recursively sorts each node's children by visual position so that the
// flatten traversal produces siblingIndex values matching screen-reader
// order (top-to-bottom, left-to-right). Nodes with empty/NaN bounds are
// sorted to the end, preserving their relative insertion order via
// stable_sort.
void SemanticManager::sortChildrenByVisualPosition(
    std::vector<rcp<SemanticNode>>& nodes)
{
    if (nodes.size() > 1)
    {
        std::stable_sort(
            nodes.begin(),
            nodes.end(),
            [](const rcp<SemanticNode>& a, const rcp<SemanticNode>& b) {
                const auto ab = a->bounds();
                const auto bb = b->bounds();
                const bool aEmpty = ab.isEmptyOrNaN();
                const bool bEmpty = bb.isEmptyOrNaN();
                // Empty-bounds nodes sort after all valid-bounds nodes.
                if (aEmpty != bEmpty)
                {
                    return bEmpty; // a < b when b is empty and a is not
                }
                if (aEmpty)
                {
                    return false; // both empty — preserve order
                }
                if (ab.minY != bb.minY)
                {
                    return ab.minY < bb.minY;
                }
                return ab.minX < bb.minX;
            });
    }

    for (auto& node : nodes)
    {
        sortChildrenByVisualPosition(node->mutableChildren());
    }
}

// Marks a specific node dirty. The global m_dirt flag triggers refresh();
// the per-node sets enable O(K) incremental patching. id == 0 is the
// uninitialized sentinel — we set global dirt but skip per-node.
void SemanticManager::markNodeDirty(uint32_t id, SemanticDirt dirt)
{
    markDirty(dirt);
    if (id == 0)
    {
        return;
    }

    if (enums::is_flag_set(dirt, SemanticDirt::Content))
    {
        m_dirtyContentNodes.insert(id);
    }
    if (enums::is_flag_set(dirt, SemanticDirt::Bounds))
    {
        m_dirtyBoundsNodes.insert(id);
    }
}

void SemanticManager::markBoundaryDirty(uint32_t boundaryNodeId)
{
    m_dirtyBoundaryIds.insert(boundaryNodeId);
    markDirty(SemanticDirt::Bounds);
}

// Re-read bounds from the scene graph for all non-boundary nodes in the
// subtree rooted at `node`. Updates the SemanticNode bounds and marks
// them bounds-dirty if changed.
void SemanticManager::reconcileBoundsForSubtree(SemanticNode* node)
{
    if (node == nullptr)
    {
        return;
    }

    // Skip boundary nodes themselves (they have no drawable bounds), but
    // recurse into their children.
    if (!node->isBoundaryNode())
    {
        auto* coreOwner = node->coreOwner();
        if (coreOwner != nullptr && coreOwner->is<Component>())
        {
            auto* owner = coreOwner->as<Component>();
            const AABB liveBounds = SemanticProvider::semanticBounds(owner);
            if (liveBounds != node->bounds())
            {
                node->bounds(liveBounds);
                m_dirtyBoundsNodes.insert(node->id());
            }
        }
    }

    for (const auto& child : node->children())
    {
        reconcileBoundsForSubtree(child.get());
    }
}

// Assign a manager-local id to this node, or reconcile the watermark
// for an explicitly-provided id so subsequent auto-assignments don't
// collide. Called from addChild() before the lookup map is updated.
void SemanticManager::ensureNodeId(SemanticNode& node)
{
    if (node.id() == 0)
    {
        // Fresh node: pick the next local id. Skip any id already in
        // the map (can happen when an explicit id was preserved earlier
        // with a value greater than our watermark).
        while (m_nodesById.count(m_nextLocalId) != 0)
        {
            ++m_nextLocalId;
        }
        node.id(m_nextLocalId++);
        return;
    }

    // Explicit id: bump the watermark so future auto-assignments skip it.
    if (node.id() >= m_nextLocalId)
    {
        m_nextLocalId = node.id() + 1;
    }
}

// Registers a SemanticNode in the tree. Appends the child to the
// parent's children (or roots). Assigns a manager-local id if the node
// was constructed without one. If the node's id collides with a
// *different* node already tracked by this manager (nodes migrating from
// another manager's id space), reassign it.
void SemanticManager::addChild(rcp<SemanticNode> parentNode,
                               rcp<SemanticNode> child)
{
    if (child == nullptr)
    {
        return;
    }

    // Collision with a different node already tracked under this id —
    // reassign the new node to a fresh manager-local id.
    auto existing = m_nodesById.find(child->id());
    if (child->id() != 0 && existing != m_nodesById.end() &&
        existing->second != child)
    {
        child->id(0);
    }

    ensureNodeId(*child);

    if (m_nodesById.find(child->id()) == m_nodesById.end())
    {
        m_nodesById[child->id()] = child;
    }

    child->manager(this);

    // Append to the end — refresh() sorts children by visual position
    // (bounds) before every flatten.
    if (parentNode != nullptr)
    {
        // Parent may have been created by a different artboard's
        // buildSemanticTree and never inserted here directly.
        if (m_nodesById.find(parentNode->id()) == m_nodesById.end())
        {
            m_nodesById[parentNode->id()] = parentNode;
        }
        child->parent(parentNode.get());
        parentNode->mutableChildren().push_back(child);
    }
    else
    {
        child->parent(nullptr);
        m_roots.push_back(child);
    }

    markDirty(SemanticDirt::Structure);
}

// Removes a SemanticNode from the tree. Unlinks from parent (or roots),
// clears the manager back-reference, removes from lookup map, and marks
// Structure dirty so the next refresh() emits a removal diff entry.
void SemanticManager::removeChild(rcp<SemanticNode> node)
{
    if (node == nullptr)
    {
        return;
    }

    auto* parentNode = node->parent();
    if (parentNode != nullptr)
    {
        auto& siblings = parentNode->mutableChildren();
        siblings.erase(std::remove_if(siblings.begin(),
                                      siblings.end(),
                                      [&node](const rcp<SemanticNode>& child) {
                                          return child.get() == node.get();
                                      }),
                       siblings.end());
        node->parent(nullptr);
    }
    else
    {
        m_roots.erase(std::remove_if(m_roots.begin(),
                                     m_roots.end(),
                                     [&node](const rcp<SemanticNode>& root) {
                                         return root.get() == node.get();
                                     }),
                      m_roots.end());
    }

    node->manager(nullptr);
    m_nodesById.erase(node->id());
    markDirty(SemanticDirt::Structure);
}

// Rebuilds the m_snapshotIndexById lookup map from the current flat
// snapshot so the incremental patch path can find entries in O(1).
void SemanticManager::rebuildSnapshotIndex()
{
    m_snapshotIndexById.clear();
    m_snapshotIndexById.reserve(m_lastFlatSnapshot.size());
    for (size_t i = 0; i < m_lastFlatSnapshot.size(); i++)
    {
        m_snapshotIndexById[m_lastFlatSnapshot[i].id] = i;
    }
}

// Incremental bounds patch for a single dirty node. Compares live bounds
// to the snapshot; if different, updates in-place and emits updatedGeometry.
void SemanticManager::patchBoundsNode(uint32_t id, SemanticsDiff& diff)
{
    auto idxIt = m_snapshotIndexById.find(id);
    if (idxIt == m_snapshotIndexById.end())
    {
        return;
    }
    auto nodeIt = m_nodesById.find(id);
    if (nodeIt == m_nodesById.end())
    {
        return;
    }
    const auto b = nodeIt->second->bounds();
    auto& entry = m_lastFlatSnapshot[idxIt->second];

    // Only emit an update if bounds actually changed. This avoids no-op
    // diffs when a node is marked dirty but its bounds haven't moved
    // (e.g. a text content change that doesn't affect layout).
    if (entry.minX != b.minX || entry.minY != b.minY || entry.maxX != b.maxX ||
        entry.maxY != b.maxY)
    {
        entry.minX = b.minX;
        entry.minY = b.minY;
        entry.maxX = b.maxX;
        entry.maxY = b.maxY;
        diff.updatedGeometry.push_back(
            {entry.id, entry.minX, entry.minY, entry.maxX, entry.maxY});
    }
}

// Incremental content patch for a single dirty node. Compares role, label,
// stateFlags, and traitFlags to the snapshot; if different, updates in-place
// and emits updatedSemantic. This is how animation/data-binding changes to
// stateFlags (e.g. "Selected" toggling) propagate to the platform layer.
void SemanticManager::patchContentNode(uint32_t id, SemanticsDiff& diff)
{
    auto idxIt = m_snapshotIndexById.find(id);
    if (idxIt == m_snapshotIndexById.end())
    {
        return;
    }
    auto nodeIt = m_nodesById.find(id);
    if (nodeIt == m_nodesById.end())
    {
        return;
    }
    const auto& node = nodeIt->second;
    auto& entry = m_lastFlatSnapshot[idxIt->second];

    // Use derived label if this node has one, otherwise the canonical label.
    auto derivedIt = m_derivedLabels.find(id);
    const std::string& effectiveLabel = (derivedIt != m_derivedLabels.end())
                                            ? derivedIt->second
                                            : node->label();

    // Compare all semantic content fields. Even if only one changed,
    // we update all fields to keep the snapshot consistent.
    if (entry.role != node->role() || entry.label != effectiveLabel ||
        entry.value != node->value() || entry.hint != node->hint() ||
        entry.stateFlags != node->stateFlags() ||
        entry.traitFlags != node->traitFlags() ||
        entry.headingLevel != node->headingLevel())
    {
        entry.role = node->role();
        entry.label = effectiveLabel;
        entry.value = node->value();
        entry.hint = node->hint();
        entry.stateFlags = node->stateFlags();
        entry.traitFlags = node->traitFlags();
        entry.headingLevel = node->headingLevel();
        diff.updatedSemantic.push_back(entry);
    }
}

// Core refresh algorithm. Two paths:
// 1. FULL RE-FLATTEN (Structure dirty or first frame): O(N) in tree size.
// 2. INCREMENTAL PATCH (only Bounds/Content dirty): O(K) dirty nodes.
// Before diffing, reconciles live bounds and computes container bounds.
void SemanticManager::refresh()
{
    // Read the current dirt flags.
    const bool structureDirty =
        enums::is_flag_set(m_dirt, SemanticDirt::Structure);
    bool boundsDirty = enums::is_flag_set(m_dirt, SemanticDirt::Bounds);
    const bool contentDirty = enums::is_flag_set(m_dirt, SemanticDirt::Content);

    // -------------------------------------------------------------------------
    // Targeted bounds reconciliation via dirty boundary nodes.
    //
    // When a host artboard's transform changes, its boundary node is marked
    // dirty via markBoundaryDirty(). We re-read bounds only for nodes under
    // dirty boundaries — O(K_subtree) instead of O(N_all_nodes).
    // -------------------------------------------------------------------------
    if (!m_dirtyBoundaryIds.empty())
    {
        for (uint32_t boundaryId : m_dirtyBoundaryIds)
        {
            auto it = m_nodesById.find(boundaryId);
            if (it != m_nodesById.end())
            {
                reconcileBoundsForSubtree(it->second.get());
            }
        }
        m_dirtyBoundaryIds.clear();
        if (!m_dirtyBoundsNodes.empty())
        {
            boundsDirty = true;
        }
    }

    // Early exit if nothing is dirty after reconciliation.
    if (!structureDirty && !boundsDirty && !contentDirty)
    {
        return;
    }

    // -------------------------------------------------------------------------
    // Post-order container bounds fallback.
    //
    // Container nodes (e.g. groups, tabLists) often have no drawable geometry
    // of their own — their bounds are empty/NaN from SemanticProvider. For
    // accessibility, they need bounds that enclose their children so the
    // platform can render focus indicators and hit-test regions.
    //
    // We compute these bottom-up (post-order) so that leaf bounds are resolved
    // before parents try to merge them. Only nodes with empty/NaN self-bounds
    // get container bounds — drawable nodes keep their own bounds.
    //
    // On the incremental path, we also track which container nodes had their
    // computed bounds change (vs. the snapshot) so patchBoundsNode can emit
    // updatedGeometry entries for them without a full re-flatten.
    // -------------------------------------------------------------------------
    bool needsReorder = false;
    std::vector<uint32_t> containerBoundsChangedIds;
    // Run when bounds changed OR when structure changed. Structure changes
    // add new nodes whose leaf bounds were set during creation (before the
    // manager was assigned), so boundsDirty may not be set even though
    // boundary nodes need their container bounds computed for sorting.
    if (boundsDirty || structureDirty)
    {
        // The visual-order check is only meaningful when we'd otherwise
        // take the incremental path. Under structureDirty (or on first
        // frame) the full re-flatten re-sorts children anyway, so the
        // check is redundant and we skip it.
        const bool reorderCheckEnabled =
            !structureDirty && !m_lastFlatSnapshot.empty();

        for (const auto& root : m_roots)
        {
            reconcileBoundsAndCheckOrder(root,
                                         reorderCheckEnabled,
                                         needsReorder,
                                         containerBoundsChangedIds,
                                         m_snapshotIndexById,
                                         m_lastFlatSnapshot);
        }

        // Root-level order check is not covered by the recursion (roots
        // have no parent to iterate from), so handle it here.
        if (reorderCheckEnabled && !needsReorder && m_roots.size() > 1)
        {
            needsReorder = !childrenInVisualOrder(m_roots);
        }
    }

    // -------------------------------------------------------------------------
    // Diff production — two paths depending on what kind of dirt is present.
    // -------------------------------------------------------------------------

    // -------------------------------------------------------------------------
    // Check if any content-dirty node is an absorbed child from the last
    // derivation pass. If so, the parent interactive node's derived label
    // may be stale — escalate to a full re-flatten so derivation re-runs.
    // -------------------------------------------------------------------------
    bool needsRederivation = false;
    if (contentDirty && !m_excludedIds.empty())
    {
        for (const auto& id : m_dirtyContentNodes)
        {
            if (m_excludedIds.count(id))
            {
                needsRederivation = true;
                break;
            }
        }
    }

    if (structureDirty || needsRederivation || needsReorder ||
        m_lastFlatSnapshot.empty())
    {
        // FULL RE-FLATTEN PATH.
        // The tree topology changed (add/remove/reparent), an absorbed
        // child's content changed (needs re-derivation), children moved
        // past each other (needs re-sort), or this is the first frame.
        // Walk the entire SemanticNode tree to produce a new flat snapshot,
        // then diff it against the previous snapshot.
        //
        // Step 1: Sort children by visual position (bounds) — only needed
        //         when topology or ordering changed, not for rederivation.
        // Step 2: Derive labels for interactive nodes and build exclude set.
        // Step 3: Flatten with derivation applied.
        // Step 4: Diff against previous snapshot.
        if (structureDirty || needsReorder || m_lastFlatSnapshot.empty())
        {
            sortChildrenByVisualPosition(m_roots);
        }

        m_derivedLabels.clear();
        m_excludedIds.clear();
        deriveLabelForInteractiveNodes(m_roots, m_derivedLabels, m_excludedIds);

        auto currentFlat = flattenFromSemanticNodes(m_roots,
                                                    m_nodesById.size(),
                                                    m_excludedIds,
                                                    m_derivedLabels);
        auto nextDiff =
            buildDiffFromFlats(currentFlat, m_lastFlatSnapshot, m_version + 1);
        if (!nextDiff.empty())
        {
            m_version++;
            m_lastDiff = std::move(nextDiff);
            m_lastFlatSnapshot = std::move(currentFlat);
            rebuildSnapshotIndex();
        }
    }
    else
    {
        // INCREMENTAL PATCH PATH.
        // Only bounds and/or content changed — no structural changes. Patch
        // m_lastFlatSnapshot in-place for each dirty node and emit a minimal
        // diff. This avoids the O(N) tree walk + comparison, instead doing
        // O(K) work where K = number of dirty nodes.
        //
        // The snapshot index provides O(1) lookup from id to the
        // snapshot array slot, enabling in-place updates.
        SemanticsDiff nextDiff;
        nextDiff.frameNumber = Artboard::frameId();

        // Patch bounds-dirty and content-dirty nodes in tree pre-order by
        // walking the last flat snapshot. The dirty sets are used for O(1)
        // membership checks only — iterating them directly would produce
        // hash-order output that varies frame-to-frame.
        //
        // containerBoundsChangedIds is produced by the post-order walk above
        // and is merged into the bounds-dirty set so container updates emit
        // alongside the regular bounds updates in tree order.
        if (boundsDirty || contentDirty)
        {
            // Only build the merged bounds-dirty set when container bounds
            // actually changed. In the common case (no boundary/container
            // bounds recomputed) we can fall back to the single
            // m_dirtyBoundsNodes set and skip the merge allocation.
            const bool needsBoundsUnion =
                boundsDirty && !containerBoundsChangedIds.empty();
            std::unordered_set<uint32_t> boundsDirtyUnion;
            if (needsBoundsUnion)
            {
                boundsDirtyUnion.reserve(m_dirtyBoundsNodes.size() +
                                         containerBoundsChangedIds.size());
                boundsDirtyUnion.insert(m_dirtyBoundsNodes.begin(),
                                        m_dirtyBoundsNodes.end());
                boundsDirtyUnion.insert(containerBoundsChangedIds.begin(),
                                        containerBoundsChangedIds.end());
                nextDiff.updatedGeometry.reserve(boundsDirtyUnion.size());
            }
            else if (boundsDirty)
            {
                nextDiff.updatedGeometry.reserve(m_dirtyBoundsNodes.size());
            }
            if (contentDirty)
            {
                nextDiff.updatedSemantic.reserve(m_dirtyContentNodes.size());
            }

            for (const auto& entry : m_lastFlatSnapshot)
            {
                if (boundsDirty &&
                    (needsBoundsUnion ? boundsDirtyUnion.count(entry.id)
                                      : m_dirtyBoundsNodes.count(entry.id)))
                {
                    patchBoundsNode(entry.id, nextDiff);
                }
                if (contentDirty && m_dirtyContentNodes.count(entry.id))
                {
                    patchContentNode(entry.id, nextDiff);
                }
            }
        }

        // Single-root optimization for the incremental path.
        if (m_roots.size() == 1)
        {
            nextDiff.rootId = m_roots[0]->id();
        }

        // Only bump version and store the diff if something actually changed.
        // The patchBounds/patchContent methods guard against no-op updates,
        // so nextDiff can be empty even when dirty flags were set.
        if (!nextDiff.empty())
        {
            m_version++;
            nextDiff.treeVersion = m_version;
            m_lastDiff = std::move(nextDiff);
        }
    }

    // Clear all dirt flags and per-node dirty sets for the next frame.
    m_dirt = SemanticDirt::None;
    m_dirtyContentNodes.clear();
    m_dirtyBoundsNodes.clear();
}

SemanticsDiff SemanticManager::drainDiff()
{
    refresh();
    return consumeDiff();
}

// Returns the diff produced by the most recent refresh() and clears it.
// Subsequent calls return an empty diff until the next dirty refresh.
//
// The move + replace pattern ensures the caller gets ownership of the diff
// data and the manager's internal storage is reset to empty.
SemanticsDiff SemanticManager::consumeDiff()
{
    SemanticsDiff result = std::move(m_lastDiff);
    m_lastDiff = SemanticsDiff();
    return result;
}

bool SemanticManager::requestFocus(uint32_t semanticNodeId)
{
    auto* semanticNode = nodeById(semanticNodeId);
    if (semanticNode == nullptr)
    {
        return false;
    }
    auto* owner = semanticNode->coreOwner();
    if (owner == nullptr || !owner->is<Node>())
    {
        return false;
    }
    auto* data = owner->as<Node>()->firstChild<SemanticData>();
    return data != nullptr && data->requestFocus();
}
