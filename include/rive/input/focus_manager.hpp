/*
 * Copyright 2024 Rive
 */

#ifndef _RIVE_FOCUS_MANAGER_HPP_
#define _RIVE_FOCUS_MANAGER_HPP_

#include "rive/input/focus_node.hpp"
#include "rive/refcnt.hpp"
#include <vector>

namespace rive
{

class Artboard;

/// Direction for directional focus navigation
enum class Direction : uint8_t
{
    left,
    right,
    up,
    down
};

/// FocusManager tracks focus state and provides traversal.
/// Hierarchy is stored on FocusNode itself (parent/children).
class FocusManager
{
public:
    FocusManager() = default;
    ~FocusManager();

    // === Focus State ===

    rcp<FocusNode> primaryFocus() const { return m_primaryFocus; }
    void setFocus(rcp<FocusNode> node);
    void clearFocus();

    /// Clears primary focus if it targets FocusData that is no longer visible
    /// in the hierarchy (collapsed, hidden, opacity 0, nested host hidden).
    void dropFocusIfFocusTargetHidden();
    bool hasFocus(rcp<FocusNode> node) const; // node or descendant has focus
    bool hasPrimaryFocus(
        rcp<FocusNode> node) const; // node is the primary focus

    /// Get the world bounds of the primary focus node.
    /// Returns true if bounds are valid, false if no focus or no bounds.
    bool primaryFocusBounds(AABB& outBounds) const
    {
        if (m_primaryFocus == nullptr || !m_primaryFocus->hasWorldBounds())
        {
            return false;
        }
        outBounds = m_primaryFocus->worldBounds();
        return true;
    }

    /// Get the root artboard that contains the primary focus node.
    /// This walks up the nested artboard chain to find the topmost artboard
    /// (the one mounted by Dart/the host).
    /// Returns nullptr if there is no focus or the focusable has no artboard.
    Artboard* primaryFocusArtboard() const;

    /// Get the immediate artboard that contains the primary focus node.
    /// Unlike primaryFocusArtboard(), this returns the direct parent artboard
    /// without walking up nested artboard chains.
    Artboard* primaryFocusImmediateArtboard() const
    {
        if (m_primaryFocus == nullptr || m_primaryFocus->focusable() == nullptr)
        {
            return nullptr;
        }
        return m_primaryFocus->focusable()->focusableArtboard();
    }

    // === Hierarchy ===

    // Add child to parent (or to root nodes if parent is null)
    void addChild(rcp<FocusNode> parent, rcp<FocusNode> child);

    // Remove child from its current parent (clears focus if needed)
    void removeChild(rcp<FocusNode> child);

    const std::vector<rcp<FocusNode>>& rootNodes() const { return m_rootNodes; }

    // === Traversal ===

    bool focusNext();
    bool focusPrevious();

    // Directional navigation (gamepad/arrow keys)
    bool focusLeft();
    bool focusRight();
    bool focusUp();
    bool focusDown();

    // Get traversable children of a scope (or root nodes if scope is null)
    // Sorted by tabIndex, filtered by canFocus && canTraverse
    std::vector<FocusNode*> getTraversableNodes(FocusNode* scope) const;

    // === Input Routing ===

    bool keyInput(Key key,
                  KeyModifiers modifiers,
                  bool isPressed,
                  bool isRepeat);
    bool textInput(const std::string& text);

#ifdef WITH_RIVE_TOOLS
    // === Callbacks (editor/tools only) ===
    using FocusChangedCallback = void (*)();
    void setFocusChangedCallback(FocusChangedCallback callback)
    {
        m_focusChangedCallback = callback;
    }

    /// Callback for scroll-into-view requests from Dart-mounted artboards.
    /// Called when focus changes to an element in an artboard whose root
    /// has no host (i.e., is mounted by Dart).
    /// Parameters:
    /// - bounds: world bounds of the focused element to scroll into view
    /// - artboard: the artboard directly hosted by the Dart root artboard
    using ScrollIntoViewCallback = void (*)(const AABB& bounds,
                                            Artboard* artboard);
    void setScrollIntoViewCallback(ScrollIntoViewCallback callback)
    {
        m_scrollIntoViewCallback = callback;
    }
#endif

private:
    rcp<FocusNode> m_primaryFocus;
    std::vector<rcp<FocusNode>> m_rootNodes;
    void removeManager(rcp<FocusNode>);
#ifdef WITH_RIVE_TOOLS
    FocusChangedCallback m_focusChangedCallback = nullptr;
    ScrollIntoViewCallback m_scrollIntoViewCallback = nullptr;
#endif

    void notifyFocusChange(FocusNode* oldFocus, FocusNode* newFocus);
    FocusNode* findNextFocusable(FocusNode* current, bool forward) const;
    FocusNode* findNodeInDirection(FocusNode* current,
                                   Direction direction) const;
};

} // namespace rive

#endif
