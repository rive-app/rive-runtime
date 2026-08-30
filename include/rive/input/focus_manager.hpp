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
class ListenerInvocation;
class ScriptedDrawable;

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
    FocusNode* primaryFocusPtr() const { return m_primaryFocus.get(); }
    void setFocus(rcp<FocusNode> node);
    void clearFocus();

    /// Re-homes primary focus when its target is no longer visible in the
    /// hierarchy (collapsed, hidden, opacity 0, nested host paused).
    ///
    /// Focus moves to the nearest ancestor that can still offer a focus stop,
    /// preferring another eligible leaf under that ancestor over the ancestor
    /// itself, and only clears when no ancestor has one. The walk stays inside
    /// the ancestor chain — it does not fall out into the manager's other root
    /// branches, which are unrelated trees. Kept under the original name
    /// because it is exported through the FFI/wasm bindings.
    void dropFocusIfFocusTargetHidden();

    void dropFocusIfFocusTargetHidden(const Artboard* rootArtboard);

    /// Re-applies the focus-rests-on-a-leaf rule to the current target.
    ///
    /// Call after an update pass, for the same reason as
    /// processPendingFocusRequests: renderOpacity and collapse are what
    /// eligibility reads, and they are only meaningful once that pass has run.
    ///
    /// Scoped to [rootArtboard] for that same reason. A manager can be shared
    /// across independent roots, and each root updates its own components:
    /// descending into a node that belongs to another root would measure its
    /// eligibility against components that root hasn't refreshed yet. The
    /// scope test is on where focus would LAND, not on where it sits — that is
    /// the eligibility being claimed. A destination that can't be attributed
    /// to any root — under a node a host created through the FocusNode API —
    /// is always descended, since no root's pass would ever claim it.
    void descendFocusToLeaf(const Artboard* rootArtboard);

    /// descendFocusToLeaf for every root on this manager at once, for a host
    /// that updates all of its roots together and so can descend whichever one
    /// the target belongs to.
    void descendFocusToLeafAllRoots();

    bool hasFocus(rcp<FocusNode> node) const; // node or descendant has focus
    bool hasPrimaryFocus(
        rcp<FocusNode> node) const; // node is the primary focus

    /// Get the world bounds of the primary focus node.
    /// Returns true if bounds are valid, false if no focus or no bounds.
    bool primaryFocusBounds(AABB& outBounds) const
    {
        if (m_primaryFocus == nullptr)
        {
            return false;
        }
        // Live focusable bounds first; the node's cached bounds go stale when
        // an ancestor host moves the containing artboard instance, and remain
        // only for nodes whose host pushes bounds in externally.
        if (m_primaryFocus->focusable() != nullptr &&
            m_primaryFocus->focusable()->worldBounds(outBounds))
        {
            return true;
        }
        if (!m_primaryFocus->hasWorldBounds())
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
    // Insert as index-th child of parent (0 = first). Same re-parenting as
    // addChild.
    void addChild(rcp<FocusNode> parent, rcp<FocusNode> child, size_t index);

    // Remove child from its current parent (clears focus if needed)
    void removeChild(rcp<FocusNode> child);

    // Detach child from its current parent WITHOUT clearing focus. Use this
    // when reordering an existing node (e.g. rebuilding the hierarchy) so the
    // primary focus and its blur/focus notifications are preserved. For genuine
    // removal use removeChild instead.
    void detachChild(rcp<FocusNode> child);

    const std::vector<rcp<FocusNode>>& rootNodes() const { return m_rootNodes; }

    // === Deferred Focus Requests ===

    /// A focus change requested by a FocusAction while the artboard's
    /// components were not yet up to date for the frame.
    ///
    /// FocusActions run while a state machine advances its layers, or from a
    /// listener — both before the frame's update pass has recomputed
    /// renderOpacity and propagated collapse. Applying a change there tests the
    /// target's eligibility against stale values (renderOpacity is still its 0
    /// default on the first advance; a target uncollapsed by the same
    /// interaction still reads as collapsed), so a perfectly focusable target
    /// is rejected and the request silently dropped. Requests are queued here
    /// and applied by processPendingFocusRequests once components are current.
    ///
    /// The queue lives on the manager rather than on each StateMachineInstance
    /// because nested artboards and artboard-component-list items share their
    /// root's manager: one drain reaches every state machine in the tree, with
    /// no traversal.
    struct PendingFocusRequest
    {
        enum class Kind : uint8_t
        {
            target,
            clear,
            traverse
        };
        Kind kind;
        /// Kind::target. Held as a FocusNode rather than a FocusData so the
        /// request can't outlive its target: ~FocusData clears the node's
        /// focusable, which the drain treats as "gone".
        rcp<FocusNode> node;
        /// Kind::traverse: 0=next, 1=previous, 2=up, 3=down, 4=left, 5=right
        /// (sync with FocusActionTraversal's traversalKind property).
        uint32_t traversalKind;
        /// Root artboard of the tree that raised this request. A manager can be
        /// shared across independent roots (the editor can wire that up through
        /// stateMachineSetExternalFocusManager), and each root updates its own
        /// components; draining another root's request would test eligibility
        /// against components that root hasn't updated yet.
        const Artboard* rootArtboard;
    };

    /// Safety valve for a host that queues requests but never drains them
    /// (draining is the caller's job — advanceAndApply does it internally,
    /// anyone driving advance() directly calls processPendingFocusRequests /
    /// finishPendingFocusRequests). Focus is latest-wins, so the oldest
    /// request is the one to drop.
    static constexpr size_t maxPendingFocusRequests = 64;

    /// Requests apply immediately when they can: only an attempt that doesn't
    /// take (the stale-components case above) is queued for retry, so focus
    /// asked for against an already-focusable target still lands on the same
    /// frame.
    void requestFocus(rcp<FocusNode> node, const Artboard* rootArtboard);
    void requestClearFocus(const Artboard* rootArtboard);
    void requestTraversal(uint32_t traversalKind, const Artboard* rootArtboard);

    /// Applies the requests raised by [rootArtboard]'s tree, keeping any that
    /// couldn't take yet for the next call. Requests from other roots are
    /// always left queued.
    ///
    /// Call after an update pass: a target's eligibility is read from
    /// renderOpacity and collapse, which that pass computes. Called any
    /// earlier it reads values that aren't meaningful yet and rejects a
    /// perfectly focusable target.
    void processPendingFocusRequests(const Artboard* rootArtboard);

    /// processPendingFocusRequests for every root on this manager at once, for
    /// a host that updates all of its roots together and so can drain them
    /// together.
    void processAllPendingFocusRequests();

    /// Last call of the frame for [rootArtboard]'s tree: applies what it can
    /// and discards the rest, so a request for a target that never becomes
    /// focusable can't linger and steal focus in some later frame.
    void finishPendingFocusRequests(const Artboard* rootArtboard);

    /// finishPendingFocusRequests for every root on this manager at once, for
    /// a host that knows the frame is over for all of them — a manager shared
    /// across roots has nothing left to wait for at that point.
    void finishAllPendingFocusRequests();

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

    /// The selected text of the focused element (bubbling up like textInput
    /// until a focusable reports a non-empty selection). Empty when nothing
    /// with a selection is focused. Lets hosts implement clipboard copy/cut.
    std::string selectedText() const;

    /// Bubble gamepad invocations from primary focus up through ancestors.
    /// `outDispatchedScriptedDrawable` (when non-null) is filled with the
    /// `ScriptedDrawable` that the focus tree forwarded the event to so
    /// callers can avoid double-dispatching during a separate broadcast pass.
    bool gamepadDispatch(
        const ListenerInvocation& invocation,
        ScriptedDrawable** outDispatchedScriptedDrawable = nullptr);

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

    /// True if any node in the tree is backed by focusable data or can take
    /// focus — even while currently ineligible for traversal. Structural
    /// scopes (unbacked, canFocus=false) don't count on their own. Gates
    /// one-time keyboard setup in high-level runtimes via
    /// StateMachineInstance::hasFocusNodes().
    ///
    /// Cached: high-level runtimes poll this every frame, so the tree walk
    /// only reruns after markFocusableContentDirty(); otherwise O(1).
    bool hasFocusableContent() const;

    /// Invalidate the cached hasFocusableContent() answer. Called whenever
    /// an input to its predicate changes: tree structure (add/remove/erase)
    /// or a node's focusable backing / canFocus flag.
    void markFocusableContentDirty() { m_focusableContentDirty = true; }

private:
    void enqueueFocusRequest(PendingFocusRequest request);
    void drainPendingFocusRequests(const Artboard* rootArtboard,
                                   bool keepUnapplied,
                                   bool allRoots);
    bool hasPendingFocusRequests(const Artboard* rootArtboard) const;
    bool applyFocusTraversal(uint32_t traversalKind);
    /// @returns true when the request is done with — it took effect, or it
    /// never can (its target is gone). False means "try again later".
    bool applyPendingFocusRequest(const PendingFocusRequest& request);
    /// Shared body of descendFocusToLeaf and descendFocusToLeafAllRoots.
    void applyDescendFocusToLeaf(const Artboard* rootArtboard, bool allRoots);

    rcp<FocusNode> m_primaryFocus;
    std::vector<rcp<FocusNode>> m_rootNodes;
    std::vector<PendingFocusRequest> m_pendingFocusRequests;
    // Backing for hasFocusableContent(); mutable so the const getter can
    // recompute lazily. Starts dirty so the first call computes.
    mutable bool m_hasFocusableContent = false;
    mutable bool m_focusableContentDirty = true;
    void removeManager(rcp<FocusNode>);
    /// Point `node` and every descendant at this manager. Counterpart to
    /// removeManager; a subtree joining the manager has to be claimed whole,
    /// or descendants left over from a detach/re-add cycle can never
    /// unregister themselves.
    void assignManager(rcp<FocusNode>);
    // Erase a node from m_rootNodes if present (no-op otherwise).
    void eraseRoot(const rcp<FocusNode>& node);
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
