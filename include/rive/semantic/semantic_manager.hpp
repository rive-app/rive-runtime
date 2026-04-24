#ifndef _RIVE_SEMANTIC_MANAGER_HPP_
#define _RIVE_SEMANTIC_MANAGER_HPP_

#include "rive/refcnt.hpp"
#include "rive/semantic/semantic_dirt.hpp"
#include "rive/semantic/semantic_node.hpp"
#include "rive/semantic/semantic_snapshot.hpp"

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace rive
{
class Artboard;

// Owns the semantic tree, tracks dirty state, and produces incremental
// SemanticsDiff payloads. Created by StateMachineInstance, populated by
// Artboard::buildSemanticTree(), consumed via SemanticManager::drainDiff().
class SemanticManager
{
public:
    explicit SemanticManager() {}

    void addChild(rcp<SemanticNode> parentNode, rcp<SemanticNode> child);
    void removeChild(rcp<SemanticNode> node);

    void markDirty(SemanticDirt dirt = SemanticDirt::All)
    {
        m_dirt = m_dirt | dirt;
    }
    void markNodeDirty(uint32_t id, SemanticDirt dirt = SemanticDirt::All);

    /// Mark a boundary node as having a dirty host transform. During the
    /// next drainDiff(), bounds will be re-read for all semantic nodes under
    /// this boundary instead of polling all nodes globally.
    void markBoundaryDirty(uint32_t boundaryNodeId);
    bool isDirty() const { return m_dirt != SemanticDirt::None; }
    uint64_t version() const { return m_version; }

    // Look up a SemanticNode by its unique ID. Returns nullptr if not found.
    SemanticNode* nodeById(uint32_t id) const
    {
        auto it = m_nodesById.find(id);
        return it != m_nodesById.end() ? it->second.get() : nullptr;
    }

    // Request focus on the FocusData sibling of the SemanticData that owns
    // the given semantic node ID. Returns true if focus was set.
    bool requestFocus(uint32_t semanticNodeId);

    // Rebuild the tree if dirty and return the diff since the last call.
    // Returns an empty diff when nothing changed. Clears internal state so
    // the next call starts fresh.
    SemanticsDiff drainDiff();

private:
    void refresh();
    SemanticsDiff consumeDiff();
    void patchBoundsNode(SemanticsDiffNode& entry, SemanticsDiff& diff);
    void patchContentNode(SemanticsDiffNode& entry, SemanticsDiff& diff);
    void reconcileBoundsForSubtree(SemanticNode* node);

    // Sorts children of every node in the given forest by visual position
    // (top-to-bottom, left-to-right), recursively. Defined here (as a
    // private static member rather than a free helper) so it can use
    // SemanticNode::mutableChildren() through the friend relationship.
    static void sortChildrenByVisualPosition(
        std::vector<rcp<SemanticNode>>& nodes);

    // Assign a manager-local id to a freshly registered node (id == 0),
    // or reconcile the watermark for an explicitly-provided id. Called from
    // addChild() before the node is inserted in m_nodesById.
    void ensureNodeId(SemanticNode& node);

    SemanticDirt m_dirt = SemanticDirt::All;
    SemanticsDiff m_lastDiff;
    std::vector<SemanticsDiffNode> m_lastFlatSnapshot;
    uint64_t m_version = 0;
    // Next id for auto-assignment; starts at 1 so 0 can remain an
    // "unassigned" sentinel. Scoped per-manager so two unrelated managers
    // in the same process can both start fresh without colliding.
    uint32_t m_nextLocalId = 1;
    std::unordered_map<uint32_t, rcp<SemanticNode>> m_nodesById;
    std::vector<rcp<SemanticNode>> m_roots;
    std::unordered_set<uint32_t> m_dirtyContentNodes;
    std::unordered_set<uint32_t> m_dirtyBoundsNodes;
    std::unordered_set<uint32_t> m_dirtyBoundaryIds;

    // Label derivation state from the last full flatten. Interactive nodes
    // that derived their label from children have entries in m_derivedLabels.
    // Children that were absorbed (flattened) are tracked in m_excludedIds
    // so the incremental path can detect when re-derivation is needed.
    std::unordered_map<uint32_t, std::string> m_derivedLabels;
    std::unordered_set<uint32_t> m_excludedIds;
};
} // namespace rive

#endif
