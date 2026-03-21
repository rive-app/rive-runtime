/*
 * Copyright 2024 Rive
 */

#ifndef _RIVE_FOCUS_NODE_HPP_
#define _RIVE_FOCUS_NODE_HPP_

#include "rive/input/focusable.hpp"
#include "rive/math/aabb.hpp"
#include "rive/refcnt.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace rive
{

class FocusManager; // Forward declaration

enum class EdgeBehavior : uint8_t
{
    parentScope = 0, // Focus exits to parent scope's next focusable
    closedLoop = 1,  // Focus wraps from last to first within this scope
    stop = 2,        // Focus stays on boundary element
};

class FocusNode : public RefCnt<FocusNode>
{
public:
    FocusNode(Focusable* focusable = nullptr) : m_focusable(focusable) {}

    // === Focusable ===

    Focusable* focusable() const { return m_focusable; }
    void setFocusable(Focusable* focusable) { m_focusable = focusable; }
    void clearFocusable() { m_focusable = nullptr; }

    // === Properties (bitfield-backed) ===

    // Master switch: if false, node cannot receive focus at all
    bool canFocus() const { return m_flags & Flag::kCanFocus; }
    void canFocus(bool value) { setFlag(Flag::kCanFocus, value); }

    // Can receive focus via pointer/touch click
    bool canTouch() const { return m_flags & Flag::kCanTouch; }
    void canTouch(bool value) { setFlag(Flag::kCanTouch, value); }

    // Included in tab traversal
    bool canTraverse() const { return m_flags & Flag::kCanTraverse; }
    void canTraverse(bool value) { setFlag(Flag::kCanTraverse, value); }

#ifdef WITH_RIVE_TOOLS
    // Only at edit time, we push down the collapse status of the nodes because
    // we can't get the FocusData node from the FocusNode
    bool isCollapsed() const { return m_isCollapsed; }
    void isCollapsed(bool value) { m_isCollapsed = value; }
#endif
    // True if this node or any descendant currently has focus
    // (set by FocusManager during focus transitions)
    bool hasFocus() const { return m_flags & Flag::kHasFocus; }

    // Note: A node with canFocus=true but canTouch=false and canTraverse=false
    // can still be focused programmatically (via script) and receives text
    // input.

    // Scope status is implicit: a node with children is a scope.
    // EdgeBehavior only applies to scopes (nodes with children).
    EdgeBehavior edgeBehavior() const
    {
        return static_cast<EdgeBehavior>((m_flags >> edgeBehaviorShift) &
                                         edgeBehaviorMask);
    }
    void edgeBehavior(EdgeBehavior edge)
    {
        m_flags = (m_flags & ~(edgeBehaviorMask << edgeBehaviorShift)) |
                  (static_cast<uint8_t>(edge) << edgeBehaviorShift);
    }

    int tabIndex() const { return m_tabIndex; }
    void tabIndex(int value) { m_tabIndex = static_cast<int16_t>(value); }

    // Debug name (not required to be unique)
    const std::string& name() const { return m_name; }
    void name(const std::string& value) { m_name = value; }

    // === World Bounds (for directional navigation) ===

    // World bounds in root artboard space, used for spatial navigation.
    // Set during update cycle by owner (TextInput, Dart, etc).
    const AABB& worldBounds() const { return m_worldBounds; }
    void worldBounds(const AABB& bounds) { m_worldBounds = bounds; }

    // Check if bounds are available (not empty/invalid)
    bool hasWorldBounds() const { return !m_worldBounds.isEmptyOrNaN(); }

    // Clear bounds (mark as unavailable)
    void clearWorldBounds() { m_worldBounds = AABB(); }

    // === Hierarchy ===

    FocusNode* parent() const { return m_parent; }
    const std::vector<rcp<FocusNode>>& children() const { return m_children; }
    FocusManager* manager() const { return m_manager; }
    bool isScope() const { return !m_children.empty(); }

    void addChild(rcp<FocusNode> child);
    void removeChild(rcp<FocusNode> child);

    // Remove this node from its current parent (used internally)
    void removeFromParent();

    // === Input Handling (delegates to Focusable) ===

    bool keyInput(Key key,
                  KeyModifiers modifiers,
                  bool isPressed,
                  bool isRepeat)
    {
        return m_focusable
                   ? m_focusable->keyInput(key, modifiers, isPressed, isRepeat)
                   : false;
    }

    bool textInput(const std::string& text)
    {
        return m_focusable ? m_focusable->textInput(text) : false;
    }

    void focused()
    {
        if (m_focusable)
        {
            m_focusable->focused();
        }
    }

    void blurred()
    {
        if (m_focusable)
        {
            m_focusable->blurred();
        }
    }

private:
    friend class FocusManager;

    // Used by FocusManager to update focus state
    void setHasFocus(bool value) { setFlag(Flag::kHasFocus, value); }

    enum Flag : uint8_t
    {
        kCanFocus = 1 << 0,    // default: true
        kCanTouch = 1 << 1,    // default: true
        kCanTraverse = 1 << 2, // default: true
        // bits 3-4: EdgeBehavior (2 bits)
        kHasFocus = 1 << 5, // true if this node or descendant has focus
    };
    static constexpr uint8_t edgeBehaviorShift = 3;
    static constexpr uint8_t edgeBehaviorMask = 0x3;
    static constexpr uint8_t defaultFlags =
        Flag::kCanFocus | Flag::kCanTouch | Flag::kCanTraverse;

    void setFlag(Flag flag, bool value)
    {
        if (value)
        {
            m_flags |= flag;
        }
        else
        {
            m_flags &= ~flag;
        }
    }

    Focusable* m_focusable = nullptr;
    FocusNode* m_parent = nullptr;
    FocusManager* m_manager = nullptr;
    std::vector<rcp<FocusNode>> m_children;
    std::string m_name;
    AABB m_worldBounds; // Default empty (invalid) - hasWorldBounds() returns
                        // false
    uint8_t m_flags = defaultFlags;
    int16_t m_tabIndex = 0;
#ifdef WITH_RIVE_TOOLS
    bool m_isCollapsed = false;
#endif
};

} // namespace rive

#endif
