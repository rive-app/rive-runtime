#ifndef _RIVE_SEMANTIC_SNAPSHOT_HPP_
#define _RIVE_SEMANTIC_SNAPSHOT_HPP_

#include <cstdint>
#include <string>
#include <vector>

namespace rive
{
// Flattened node payload used by incremental deltas. Produced by
// SemanticManager::refresh() and consumed by platform runtimes via
// SemanticManager::drainDiff().
struct SemanticsDiffNode
{
    uint32_t id = 0;
    uint32_t role = 0;
    std::string label = "";
    std::string value = "";
    std::string hint = "";
    uint32_t stateFlags = 0;
    uint32_t traitFlags = 0;
    uint32_t headingLevel = 0;
    float minX = 0.0f;
    float minY = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;
    // -1 means root.
    int32_t parentId = -1;
    // Sibling position under parent (or root index if parent is -1).
    uint32_t siblingIndex = 0;
};

// Parent children list update for robust reorder/reparent sync.
// parentId == -1 updates root ordering.
struct SemanticsChildrenUpdate
{
    // -1 means roots list order.
    int32_t parentId = -1;
    // Full ordered child list for the parent.
    std::vector<uint32_t> childIds;
};

// Minimal bounds-only update for the hot path — avoids string allocations
// when only geometry changes frame-to-frame.
struct SemanticsBoundsUpdate
{
    uint32_t id = 0;
    float minX = 0.0f;
    float minY = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;
};

// Incremental delta payload emitted by SemanticManager::refresh().
// Platform runtimes consume this to incrementally update their
// accessibility trees.
//
// Ordering contract (stable; platform adapters may rely on it):
//   - added, moved, updatedSemantic, updatedGeometry are emitted in
//     current-tree pre-order. A parent always precedes its children within
//     the same array.
//   - removed is emitted in previous-tree pre-order.
//   - childrenUpdated lists parent ids in the order their subtrees are
//     first entered during a pre-order walk of the current tree; parents
//     that existed only in the previous tree are appended in previous-tree
//     order. Each entry's childIds is the full authoritative child list.
//
// A minimal adapter may process the arrays in the order (removed, added,
// moved, childrenUpdated, updatedSemantic, updatedGeometry) and, within
// each array, iterate linearly without a deferred-attachment queue.
struct SemanticsDiff
{
    uint64_t frameNumber = 0;
    uint64_t treeVersion = 0;
    uint32_t rootId = 0;
    std::vector<uint32_t> removed;
    std::vector<SemanticsDiffNode> added;
    std::vector<SemanticsDiffNode> moved;
    std::vector<SemanticsChildrenUpdate> childrenUpdated;
    std::vector<SemanticsDiffNode> updatedSemantic;
    std::vector<SemanticsBoundsUpdate> updatedGeometry;

    bool empty() const
    {
        return removed.empty() && added.empty() && moved.empty() &&
               childrenUpdated.empty() && updatedSemantic.empty() &&
               updatedGeometry.empty();
    }
};
} // namespace rive

#endif
