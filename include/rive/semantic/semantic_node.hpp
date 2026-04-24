#ifndef _RIVE_SEMANTIC_NODE_HPP_
#define _RIVE_SEMANTIC_NODE_HPP_

#include "rive/math/aabb.hpp"
#include "rive/refcnt.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace rive
{
class Artboard;
class Core;
class SemanticManager;

// Ref-counted tree node holding all data needed by platform accessibility.
// Created lazily by SemanticData::semanticNode() and registered with
// SemanticManager via addChild(). The manager flattens these into
// SemanticsDiffNode structs for platform consumption.
//
// Ids are scoped to the owning SemanticManager: a node is constructed
// with id == 0 (unassigned) and receives a manager-local id on first
// addChild(). This avoids collisions when multiple independent
// SemanticManagers (e.g. two unrelated artboards in the same process)
// outlive a single process-global counter.
//
// Callers that want a deterministic id (tests, serialization round-trips)
// may construct with an explicit id; SemanticManager will preserve it and
// bump its watermark to avoid future collisions.
//
// Access model:
//   - Authored-value mutators (role, label, value, hint, headingLevel,
//     stateFlags, traitFlags, bounds) are public: SemanticData writes
//     them from its property-change handlers, Artboard sets them on
//     boundary nodes, tests use them to hand-build trees.
//   - Construction-time back-refs (coreOwner, semanticData, isBoundaryNode)
//     are public: set once at node creation, never again.
//   - Structural mutators (parent, mutable children, manager,
//     id assignment) live behind `friend class SemanticManager`. The
//     manager is the only code allowed to wire the tree or rebind a
//     node to a different manager.
class SemanticNode : public RefCnt<SemanticNode>
{
public:
    explicit SemanticNode(uint32_t id = 0) : m_id(id) {}

    uint32_t id() const { return m_id; }

    SemanticNode* parent() const { return m_parent; }

    const std::vector<rcp<SemanticNode>>& children() const
    {
        return m_children;
    }

    void role(uint32_t value) { m_role = value; }
    uint32_t role() const { return m_role; }

    void stateFlags(uint32_t value) { m_stateFlags = value; }
    uint32_t stateFlags() const { return m_stateFlags; }

    void label(const std::string& value) { m_label = value; }
    const std::string& label() const { return m_label; }

    void value(const std::string& v) { m_value = v; }
    const std::string& value() const { return m_value; }

    void hint(const std::string& v) { m_hint = v; }
    const std::string& hint() const { return m_hint; }

    void headingLevel(uint32_t v) { m_headingLevel = v; }
    uint32_t headingLevel() const { return m_headingLevel; }

    AABB bounds() const { return m_bounds; }
    void bounds(const AABB& value) { m_bounds = value; }

    void traitFlags(uint32_t value) { m_traitFlags = value; }
    uint32_t traitFlags() const { return m_traitFlags; }

    Core* coreOwner() const { return m_coreOwner; }
    void coreOwner(Core* value) { m_coreOwner = value; }

    SemanticManager* manager() const { return m_manager; }

    /// Boundary nodes are structural-only nodes inserted at artboard
    /// boundaries. They are skipped during flattening (children reparented
    /// upward) but participate in tree management for O(1) subtree
    /// collapse/uncollapse and targeted bounds dirtying.
    bool isBoundaryNode() const { return m_isBoundaryNode; }
    void isBoundaryNode(bool value) { m_isBoundaryNode = value; }

    /// For boundary nodes, the nested Artboard whose region this boundary
    /// represents. The boundary's bounds are the artboard's rect mapped
    /// through its rootTransform into root space.
    Artboard* boundaryArtboard() const { return m_boundaryArtboard; }
    void boundaryArtboard(Artboard* value) { m_boundaryArtboard = value; }

    /// Back-pointer to the SemanticData that owns this node. Null for
    /// boundary nodes. Used for direct collapse without walking the
    /// component hierarchy.
    class SemanticData* semanticData() const { return m_semanticData; }
    void semanticData(class SemanticData* value) { m_semanticData = value; }

private:
    // SemanticManager is the only class allowed to rewire the tree,
    // assign/reassign ids, or rebind a node to a different manager.
    friend class SemanticManager;

    void id(uint32_t value) { m_id = value; }
    void parent(SemanticNode* value) { m_parent = value; }
    std::vector<rcp<SemanticNode>>& mutableChildren() { return m_children; }
    void manager(SemanticManager* m) { m_manager = m; }

    uint32_t m_id;
    SemanticNode* m_parent = nullptr;
    std::vector<rcp<SemanticNode>> m_children;
    uint32_t m_role = 0;
    uint32_t m_stateFlags = 0;
    std::string m_label = "";
    std::string m_value = "";
    std::string m_hint = "";
    uint32_t m_headingLevel = 0;
    // Default to "no bounds" rather than (0,0,0,0) which looks like a
    // valid position at the origin. forExpansion() has minX > maxX so
    // isEmptyOrNaN() returns true.
    AABB m_bounds = AABB::forExpansion();
    uint32_t m_traitFlags = 0;
    Core* m_coreOwner = nullptr;
    SemanticManager* m_manager = nullptr;
    bool m_isBoundaryNode = false;
    class SemanticData* m_semanticData = nullptr;
    Artboard* m_boundaryArtboard = nullptr;
};
} // namespace rive

#endif
