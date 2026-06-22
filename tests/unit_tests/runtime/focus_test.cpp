#include <catch.hpp>
#include "rive/animation/focus_action_traversal.hpp"
#include "rive/animation/listener_invocation.hpp"
#include "rive/animation/state_machine.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/artboard.hpp"
#include "rive/artboard_component_list.hpp"
#include "rive/focus_data.hpp"
#include "rive/node.hpp"
#include "rive/input/focus_node.hpp"
#include "rive/input/focus_manager.hpp"
#include "utils/no_op_factory.hpp"
#include "utils/serializing_factory.hpp"
#include "rive_file_reader.hpp"
#include "rive/viewmodel/viewmodel_instance_boolean.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "rive/animation/listener_invocation.hpp"
#include "rive/input/gamepad_snapshot.hpp"

namespace rive
{

// Mock Focusable for testing
class MockFocusable : public Focusable
{
public:
    int keyInputCount = 0;
    int textInputCount = 0;
    int gamepadDispatchCount = 0;
    int focusedCount = 0;
    int blurredCount = 0;
    std::string lastText;
    Key lastKey = Key::a;
    bool returnValue = false;

    bool keyInput(Key key,
                  KeyModifiers modifiers,
                  bool isPressed,
                  bool isRepeat) override
    {
        keyInputCount++;
        lastKey = key;
        return returnValue;
    }

    bool textInput(const std::string& text) override
    {
        textInputCount++;
        lastText = text;
        return returnValue;
    }

    bool gamepadDispatch(const ListenerInvocation&,
                         ScriptedDrawable** = nullptr) override
    {
        gamepadDispatchCount++;
        return returnValue;
    }

    void focused() override { focusedCount++; }

    void blurred() override { blurredCount++; }
};

// =============================================================================
// FocusNode Tests
// =============================================================================

TEST_CASE("FocusNode default properties", "[FocusNode]")
{
    auto node = make_rcp<FocusNode>();

    CHECK(node->canFocus() == true);
    CHECK(node->canTouch() == true);
    CHECK(node->canTraverse() == true);
    CHECK(node->tabIndex() == 0);
    CHECK(node->edgeBehavior() == EdgeBehavior::parentScope);
    CHECK(node->focusable() == nullptr);
    CHECK(node->parent() == nullptr);
    CHECK(node->children().empty());
    CHECK(node->isScope() == false);
    CHECK(node->hasFocus() == false);
    CHECK(node->manager() == nullptr);
}

TEST_CASE("FocusNode property setters", "[FocusNode]")
{
    auto node = make_rcp<FocusNode>();

    node->canFocus(false);
    CHECK(node->canFocus() == false);

    node->canTouch(false);
    CHECK(node->canTouch() == false);

    node->canTraverse(false);
    CHECK(node->canTraverse() == false);

    node->tabIndex(42);
    CHECK(node->tabIndex() == 42);

    node->edgeBehavior(EdgeBehavior::closedLoop);
    CHECK(node->edgeBehavior() == EdgeBehavior::closedLoop);

    node->edgeBehavior(EdgeBehavior::stop);
    CHECK(node->edgeBehavior() == EdgeBehavior::stop);
}

TEST_CASE("FocusNode with Focusable", "[FocusNode]")
{
    MockFocusable focusable;
    auto node = make_rcp<FocusNode>(&focusable);

    CHECK(node->focusable() == &focusable);

    // Test input delegation
    node->keyInput(Key::a, KeyModifiers::none, true, false);
    CHECK(focusable.keyInputCount == 1);
    CHECK(focusable.lastKey == Key::a);

    node->textInput("hello");
    CHECK(focusable.textInputCount == 1);
    CHECK(focusable.lastText == "hello");

    // Test lifecycle delegation
    node->focused();
    CHECK(focusable.focusedCount == 1);

    node->blurred();
    CHECK(focusable.blurredCount == 1);
}

TEST_CASE("FocusNode without Focusable doesn't crash", "[FocusNode]")
{
    auto node = make_rcp<FocusNode>();

    // These should not crash
    CHECK(node->keyInput(Key::a, KeyModifiers::none, true, false) == false);
    CHECK(node->textInput("hello") == false);
    node->focused();
    node->blurred();
}

TEST_CASE("FocusNode setFocusable/clearFocusable", "[FocusNode]")
{
    MockFocusable focusable;
    auto node = make_rcp<FocusNode>();

    CHECK(node->focusable() == nullptr);

    node->setFocusable(&focusable);
    CHECK(node->focusable() == &focusable);

    node->clearFocusable();
    CHECK(node->focusable() == nullptr);
}

TEST_CASE("FocusNode hierarchy", "[FocusNode]")
{
    auto parent = make_rcp<FocusNode>();
    auto child1 = make_rcp<FocusNode>();
    auto child2 = make_rcp<FocusNode>();

    parent->addChild(child1);
    parent->addChild(child2);

    CHECK(child1->parent() == parent.get());
    CHECK(child2->parent() == parent.get());
    CHECK(parent->children().size() == 2);
    CHECK(parent->isScope() == true);

    parent->removeChild(child1);
    CHECK(child1->parent() == nullptr);
    CHECK(parent->children().size() == 1);
}

// =============================================================================
// FocusManager Tests
// =============================================================================

TEST_CASE("FocusManager basic focus operations", "[FocusManager]")
{
    FocusManager manager;
    MockFocusable focusable;
    auto node = make_rcp<FocusNode>(&focusable);

    CHECK(manager.primaryFocus() == nullptr);

    manager.addChild(nullptr, node);
    manager.setFocus(node);

    CHECK(manager.primaryFocus() == node);
    CHECK(manager.hasFocus(node) == true);
    CHECK(manager.hasPrimaryFocus(node) == true);
    CHECK(focusable.focusedCount == 1);

    manager.clearFocus();
    CHECK(manager.primaryFocus() == nullptr);
    CHECK(focusable.blurredCount == 1);
}

TEST_CASE("FocusManager focus change notifications", "[FocusManager]")
{
    FocusManager manager;
    MockFocusable focusable1, focusable2;
    auto node1 = make_rcp<FocusNode>(&focusable1);
    auto node2 = make_rcp<FocusNode>(&focusable2);

    manager.addChild(nullptr, node1);
    manager.addChild(nullptr, node2);

    manager.setFocus(node1);
    CHECK(focusable1.focusedCount == 1);
    CHECK(focusable1.blurredCount == 0);

    manager.setFocus(node2);
    CHECK(focusable1.blurredCount == 1);
    CHECK(focusable2.focusedCount == 1);
}

TEST_CASE("FocusManager respects canFocus", "[FocusManager]")
{
    FocusManager manager;
    auto node = make_rcp<FocusNode>();
    node->canFocus(false);

    manager.addChild(nullptr, node);
    manager.setFocus(node);

    CHECK(manager.primaryFocus() == nullptr);
}

TEST_CASE("FocusManager hierarchy", "[FocusManager]")
{
    FocusManager manager;
    auto parent = make_rcp<FocusNode>();
    auto child1 = make_rcp<FocusNode>();
    auto child2 = make_rcp<FocusNode>();

    manager.addChild(nullptr, parent);
    manager.addChild(parent, child1);
    manager.addChild(parent, child2);

    CHECK(parent->parent() == nullptr);
    CHECK(child1->parent() == parent.get());
    CHECK(child2->parent() == parent.get());

    CHECK(parent->isScope() == true);
    CHECK(child1->isScope() == false);

    const auto& children = parent->children();
    CHECK(children.size() == 2);

    // Manager reference is set on all nodes
    CHECK(parent->manager() == &manager);
    CHECK(child1->manager() == &manager);
    CHECK(child2->manager() == &manager);
}

TEST_CASE("FocusManager hasFocus with descendants", "[FocusManager]")
{
    FocusManager manager;
    auto parent = make_rcp<FocusNode>();
    auto child = make_rcp<FocusNode>();

    manager.addChild(nullptr, parent);
    manager.addChild(parent, child);

    manager.setFocus(child);

    // Manager queries should work
    CHECK(manager.hasFocus(parent) == true);
    CHECK(manager.hasPrimaryFocus(parent) == false);
    CHECK(manager.hasFocus(child) == true);
    CHECK(manager.hasPrimaryFocus(child) == true);

    // Node's hasFocus flag should be set for focused node and ancestors
    CHECK(parent->hasFocus() == true);
    CHECK(child->hasFocus() == true);
}

TEST_CASE("FocusManager removeChild clears focus", "[FocusManager]")
{
    FocusManager manager;
    MockFocusable focusable;
    auto node = make_rcp<FocusNode>(&focusable);

    manager.addChild(nullptr, node);
    manager.setFocus(node);
    CHECK(manager.primaryFocus() == node);

    manager.removeChild(node);
    CHECK(manager.primaryFocus() == nullptr);
    CHECK(focusable.blurredCount == 1);
}

TEST_CASE(
    "List row reparent: FocusNode removeFromParent preserves primary focus",
    "[FocusManager][list]")
{
    FocusManager manager;
    MockFocusable fLeaf;
    auto scope = make_rcp<FocusNode>(nullptr);
    scope->canFocus(true);
    scope->canTraverse(true);
    auto row = make_rcp<FocusNode>(nullptr);
    row->canFocus(true);
    row->canTraverse(true);
    auto leaf = make_rcp<FocusNode>(&fLeaf);

    manager.addChild(nullptr, scope);
    manager.addChild(scope, row);
    manager.addChild(row, leaf);
    manager.setFocus(leaf);
    CHECK(manager.primaryFocus() == leaf);

    row->removeFromParent();
    CHECK(manager.primaryFocus() == leaf);

    manager.addChild(scope, row, 0);
    CHECK(manager.primaryFocus() == leaf);
    CHECK(fLeaf.blurredCount == 0);
}

TEST_CASE("FocusManager input routing", "[FocusManager]")
{
    FocusManager manager;
    MockFocusable focusable;
    focusable.returnValue = true;
    auto node = make_rcp<FocusNode>(&focusable);

    manager.addChild(nullptr, node);

    // No focus, input not handled
    CHECK(manager.keyInput(Key::a, KeyModifiers::none, true, false) == false);
    CHECK(manager.textInput("hello") == false);
    GamepadSnapshot snap{};
    snap.deviceId = 1;
    snap.buttonMask = 1;
    CHECK(manager.gamepadDispatch(ListenerInvocation::gamepadConnected(snap)) ==
          false);

    manager.setFocus(node);

    // With focus, input is routed
    CHECK(manager.keyInput(Key::b, KeyModifiers::none, true, false) == true);
    CHECK(focusable.keyInputCount == 1);
    CHECK(focusable.lastKey == Key::b);

    CHECK(manager.textInput("world") == true);
    CHECK(focusable.textInputCount == 1);
    CHECK(focusable.lastText == "world");

    CHECK(manager.gamepadDispatch(ListenerInvocation::gamepadConnected(snap)) ==
          true);
    CHECK(focusable.gamepadDispatchCount == 1);
}

TEST_CASE("FocusManager traversal basic", "[FocusManager]")
{
    FocusManager manager;
    MockFocusable f1, f2, f3;
    auto node1 = make_rcp<FocusNode>(&f1);
    auto node2 = make_rcp<FocusNode>(&f2);
    auto node3 = make_rcp<FocusNode>(&f3);

    manager.addChild(nullptr, node1);
    manager.addChild(nullptr, node2);
    manager.addChild(nullptr, node3);

    // Focus first node
    manager.setFocus(node1);
    CHECK(manager.primaryFocus() == node1);

    // Navigate forward
    manager.focusNext();
    CHECK(manager.primaryFocus() == node2);

    manager.focusNext();
    CHECK(manager.primaryFocus() == node3);

    // Navigate backward
    manager.focusPrevious();
    CHECK(manager.primaryFocus() == node2);
}

TEST_CASE("FocusManager traversal with tabIndex", "[FocusManager]")
{
    FocusManager manager;
    auto node1 = make_rcp<FocusNode>();
    auto node2 = make_rcp<FocusNode>();
    auto node3 = make_rcp<FocusNode>();

    node1->tabIndex(3);
    node2->tabIndex(1);
    node3->tabIndex(2);

    manager.addChild(nullptr, node1);
    manager.addChild(nullptr, node2);
    manager.addChild(nullptr, node3);

    // Start with no focus, focusNext should pick first by tabIndex
    manager.focusNext();
    CHECK(manager.primaryFocus() == node2); // tabIndex 1

    manager.focusNext();
    CHECK(manager.primaryFocus() == node3); // tabIndex 2

    manager.focusNext();
    CHECK(manager.primaryFocus() == node1); // tabIndex 3
}

TEST_CASE("FocusManager traversal skips non-traversable", "[FocusManager]")
{
    FocusManager manager;
    auto node1 = make_rcp<FocusNode>();
    auto node2 = make_rcp<FocusNode>();
    auto node3 = make_rcp<FocusNode>();

    node2->canTraverse(false);

    manager.addChild(nullptr, node1);
    manager.addChild(nullptr, node2);
    manager.addChild(nullptr, node3);

    manager.setFocus(node1);
    manager.focusNext();

    // Should skip node2 and go to node3
    CHECK(manager.primaryFocus() == node3);
}

TEST_CASE("FocusManager edge behavior closedLoop", "[FocusManager]")
{
    FocusManager manager;
    auto scope = make_rcp<FocusNode>();
    auto node1 = make_rcp<FocusNode>();
    auto node2 = make_rcp<FocusNode>();

    scope->edgeBehavior(EdgeBehavior::closedLoop);

    manager.addChild(nullptr, scope);
    manager.addChild(scope, node1);
    manager.addChild(scope, node2);

    manager.setFocus(node2);
    manager.focusNext();

    // Should wrap to first
    CHECK(manager.primaryFocus() == node1);
}

TEST_CASE("FocusManager edge behavior stop", "[FocusManager]")
{
    FocusManager manager;
    auto scope = make_rcp<FocusNode>();
    auto node1 = make_rcp<FocusNode>();
    auto node2 = make_rcp<FocusNode>();

    scope->edgeBehavior(EdgeBehavior::stop);

    manager.addChild(nullptr, scope);
    manager.addChild(scope, node1);
    manager.addChild(scope, node2);

    manager.setFocus(node2);
    manager.focusNext();

    // Should stay on node2
    CHECK(manager.primaryFocus() == node2);
}

TEST_CASE("FocusManager ancestor notification on focus", "[FocusManager]")
{
    FocusManager manager;
    MockFocusable grandparentFocusable, parentFocusable, childFocusable;
    auto grandparent = make_rcp<FocusNode>(&grandparentFocusable);
    auto parent = make_rcp<FocusNode>(&parentFocusable);
    auto child = make_rcp<FocusNode>(&childFocusable);

    manager.addChild(nullptr, grandparent);
    manager.addChild(grandparent, parent);
    manager.addChild(parent, child);

    // Focus the leaf node
    manager.setFocus(child);

    // All ancestors should have received focused() callback
    CHECK(childFocusable.focusedCount == 1);
    CHECK(parentFocusable.focusedCount == 1);
    CHECK(grandparentFocusable.focusedCount == 1);

    // All nodes in the chain should have hasFocus flag
    CHECK(child->hasFocus() == true);
    CHECK(parent->hasFocus() == true);
    CHECK(grandparent->hasFocus() == true);
}

TEST_CASE("FocusManager common ancestor optimization", "[FocusManager]")
{
    FocusManager manager;
    MockFocusable parentFocusable, child1Focusable, child2Focusable;
    auto parent = make_rcp<FocusNode>(&parentFocusable);
    auto child1 = make_rcp<FocusNode>(&child1Focusable);
    auto child2 = make_rcp<FocusNode>(&child2Focusable);

    manager.addChild(nullptr, parent);
    manager.addChild(parent, child1);
    manager.addChild(parent, child2);

    // Focus first child
    manager.setFocus(child1);
    CHECK(parentFocusable.focusedCount == 1);
    CHECK(child1Focusable.focusedCount == 1);

    // Move focus to sibling - parent should NOT get re-notified
    manager.setFocus(child2);
    CHECK(child1Focusable.blurredCount == 1);
    CHECK(child2Focusable.focusedCount == 1);
    // Parent should not be blurred or re-focused
    CHECK(parentFocusable.focusedCount == 1); // Still 1, not 2
    CHECK(parentFocusable.blurredCount == 0);

    // Parent still has focus (descendant focused)
    CHECK(parent->hasFocus() == true);
}

TEST_CASE("FocusManager traversal focuses leaves only", "[FocusManager]")
{
    FocusManager manager;
    MockFocusable scopeFocusable, leaf1Focusable, leaf2Focusable;
    auto scope = make_rcp<FocusNode>(&scopeFocusable);
    auto leaf1 = make_rcp<FocusNode>(&leaf1Focusable);
    auto leaf2 = make_rcp<FocusNode>(&leaf2Focusable);

    manager.addChild(nullptr, scope);
    manager.addChild(scope, leaf1);
    manager.addChild(scope, leaf2);

    // Start with no focus, focusNext should focus first leaf, not scope
    manager.focusNext();
    CHECK(manager.primaryFocus() == leaf1);
    CHECK(manager.hasPrimaryFocus(scope) == false);
    CHECK(scope->hasFocus() == true); // But scope has descendant focus

    manager.focusNext();
    CHECK(manager.primaryFocus() == leaf2);
}

TEST_CASE("FocusManager nested scopes focus deepest leaf", "[FocusManager]")
{
    FocusManager manager;
    auto scope1 = make_rcp<FocusNode>();
    auto scope2 = make_rcp<FocusNode>();
    auto leaf = make_rcp<FocusNode>();

    manager.addChild(nullptr, scope1);
    manager.addChild(scope1, scope2);
    manager.addChild(scope2, leaf);

    // Navigate should go directly to the deepest leaf
    manager.focusNext();
    CHECK(manager.primaryFocus() == leaf);
    CHECK(scope1->hasFocus() == true);
    CHECK(scope2->hasFocus() == true);
}

TEST_CASE("FocusManager edge behavior parentScope exits to parent",
          "[FocusManager]")
{
    FocusManager manager;
    auto root = make_rcp<FocusNode>();
    auto scope = make_rcp<FocusNode>();
    auto inner1 = make_rcp<FocusNode>();
    auto inner2 = make_rcp<FocusNode>();
    auto outer = make_rcp<FocusNode>();

    scope->edgeBehavior(EdgeBehavior::parentScope);

    manager.addChild(nullptr, root);
    manager.addChild(root, scope);
    manager.addChild(scope, inner1);
    manager.addChild(scope, inner2);
    manager.addChild(root, outer);

    // Focus last node in scope
    manager.setFocus(inner2);
    CHECK(manager.primaryFocus() == inner2);

    // Navigate forward should exit scope and go to outer
    manager.focusNext();
    CHECK(manager.primaryFocus() == outer);
}

TEST_CASE("FocusManager clearFocus clears hasFocus flag chain",
          "[FocusManager]")
{
    FocusManager manager;
    MockFocusable parentFocusable, childFocusable;
    auto parent = make_rcp<FocusNode>(&parentFocusable);
    auto child = make_rcp<FocusNode>(&childFocusable);

    manager.addChild(nullptr, parent);
    manager.addChild(parent, child);

    manager.setFocus(child);
    CHECK(parent->hasFocus() == true);
    CHECK(child->hasFocus() == true);

    manager.clearFocus();

    // Both should be cleared
    CHECK(parent->hasFocus() == false);
    CHECK(child->hasFocus() == false);

    // Both should have received blurred callback
    CHECK(parentFocusable.blurredCount == 1);
    CHECK(childFocusable.blurredCount == 1);
}

TEST_CASE("FocusManager removeChild clears manager reference", "[FocusManager]")
{
    FocusManager manager;
    auto node = make_rcp<FocusNode>();

    manager.addChild(nullptr, node);
    CHECK(node->manager() == &manager);

    manager.removeChild(node);
    CHECK(node->manager() == nullptr);
}

TEST_CASE("FocusManager traversal backward from first leaf exits scope",
          "[FocusManager]")
{
    FocusManager manager;
    auto root = make_rcp<FocusNode>();
    auto before = make_rcp<FocusNode>();
    auto scope = make_rcp<FocusNode>();
    auto inner = make_rcp<FocusNode>();

    scope->edgeBehavior(EdgeBehavior::parentScope);

    manager.addChild(nullptr, root);
    manager.addChild(root, before);
    manager.addChild(root, scope);
    manager.addChild(scope, inner);

    // Focus the inner node
    manager.setFocus(inner);

    // Navigate backward should exit scope and go to before
    manager.focusPrevious();
    CHECK(manager.primaryFocus() == before);
}

TEST_CASE("FocusManager closedLoop wraps backward", "[FocusManager]")
{
    FocusManager manager;
    auto scope = make_rcp<FocusNode>();
    auto node1 = make_rcp<FocusNode>();
    auto node2 = make_rcp<FocusNode>();

    scope->edgeBehavior(EdgeBehavior::closedLoop);

    manager.addChild(nullptr, scope);
    manager.addChild(scope, node1);
    manager.addChild(scope, node2);

    manager.setFocus(node1);
    manager.focusPrevious();

    // Should wrap to last
    CHECK(manager.primaryFocus() == node2);
}

TEST_CASE("FocusManager stop prevents backward traversal", "[FocusManager]")
{
    FocusManager manager;
    auto scope = make_rcp<FocusNode>();
    auto node1 = make_rcp<FocusNode>();
    auto node2 = make_rcp<FocusNode>();

    scope->edgeBehavior(EdgeBehavior::stop);

    manager.addChild(nullptr, scope);
    manager.addChild(scope, node1);
    manager.addChild(scope, node2);

    manager.setFocus(node1);
    manager.focusPrevious();

    // Should stay on node1
    CHECK(manager.primaryFocus() == node1);
}

TEST_CASE("FocusActionTraversal perform advances focus with traversalKind next",
          "[FocusActionTraversal]")
{
    NoOpFactory factory;
    Artboard artboard(&factory);
    auto instance = artboard.instance();
    StateMachine machine;
    StateMachineInstance smi(&machine, instance.get());

    FocusManager* fm = smi.focusManager();
    MockFocusable f1, f2;
    auto node1 = make_rcp<FocusNode>(&f1);
    auto node2 = make_rcp<FocusNode>(&f2);
    fm->addChild(nullptr, node1);
    fm->addChild(nullptr, node2);
    fm->setFocus(node1);

    FocusActionTraversal action;
    action.traversalKind(0);
    action.perform(&smi, ListenerInvocation::none());

    CHECK(fm->primaryFocus() == node2);
}

TEST_CASE("FocusActionTraversal perform moves focus back with traversalKind "
          "previous",
          "[FocusActionTraversal]")
{
    NoOpFactory factory;
    Artboard artboard(&factory);
    auto instance = artboard.instance();
    StateMachine machine;
    StateMachineInstance smi(&machine, instance.get());

    FocusManager* fm = smi.focusManager();
    MockFocusable f1, f2;
    auto node1 = make_rcp<FocusNode>(&f1);
    auto node2 = make_rcp<FocusNode>(&f2);
    fm->addChild(nullptr, node1);
    fm->addChild(nullptr, node2);
    fm->setFocus(node2);

    FocusActionTraversal action;
    action.traversalKind(1);
    action.perform(&smi, ListenerInvocation::none());

    CHECK(fm->primaryFocus() == node1);
}

TEST_CASE("FocusActionTraversal perform unknown traversalKind defaults to next",
          "[FocusActionTraversal]")
{
    NoOpFactory factory;
    Artboard artboard(&factory);
    auto instance = artboard.instance();
    StateMachine machine;
    StateMachineInstance smi(&machine, instance.get());

    FocusManager* fm = smi.focusManager();
    MockFocusable f1, f2;
    auto node1 = make_rcp<FocusNode>(&f1);
    auto node2 = make_rcp<FocusNode>(&f2);
    fm->addChild(nullptr, node1);
    fm->addChild(nullptr, node2);
    fm->setFocus(node1);

    FocusActionTraversal action;
    action.traversalKind(999);
    action.perform(&smi, ListenerInvocation::none());

    CHECK(fm->primaryFocus() == node2);
}

TEST_CASE(
    "StateMachineInstance exposes hasFocusNodes, focusNext, focusPrevious from focusManager",
    "[FocusManager]")
{
    NoOpFactory factory;
    Artboard artboard(&factory);
    auto instance = artboard.instance();
    StateMachine machine;
    StateMachineInstance smi(&machine, instance.get());

    MockFocusable f1, f2;
    auto node1 = make_rcp<FocusNode>(&f1);
    auto node2 = make_rcp<FocusNode>(&f2);

    CHECK(smi.hasFocusNodes() == false);

    smi.focusManager()->addChild(nullptr, node1);
    smi.focusManager()->addChild(nullptr, node2);
    smi.focusManager()->setFocus(node1);

    CHECK(smi.hasFocusNodes() == true);
    CHECK(smi.focusNext() == true);
    CHECK(smi.focusPrevious() == true);
}

TEST_CASE("FocusActionTraversal perform ignores null StateMachineInstance",
          "[FocusActionTraversal]")
{
    FocusActionTraversal action;
    action.traversalKind(0);
    action.perform(nullptr, ListenerInvocation::none());
}

// Mock Focusable that reports it accepts keyboard input.
class KeyboardAcceptingFocusable : public MockFocusable
{
public:
    bool acceptsKeyboardInput() const override { return true; }
};

TEST_CASE("Focusable::acceptsKeyboardInput defaults to false", "[Focusable]")
{
    MockFocusable f;
    CHECK(f.acceptsKeyboardInput() == false);

    KeyboardAcceptingFocusable kf;
    CHECK(kf.acceptsKeyboardInput() == true);
}

TEST_CASE("StateMachineInstance::focusState reports no focus when nothing is "
          "focused",
          "[FocusState]")
{
    NoOpFactory factory;
    Artboard artboard(&factory);
    auto instance = artboard.instance();
    StateMachine machine;
    StateMachineInstance smi(&machine, instance.get());

    auto state = smi.focusState();
    CHECK(state.hasFocus == false);
    CHECK(state.expectsKeyboardInput == false);
}

TEST_CASE("StateMachineInstance::focusState reports focused non-keyboard "
          "focusable",
          "[FocusState]")
{
    NoOpFactory factory;
    Artboard artboard(&factory);
    auto instance = artboard.instance();
    StateMachine machine;
    StateMachineInstance smi(&machine, instance.get());

    MockFocusable f;
    auto node = make_rcp<FocusNode>(&f);
    smi.focusManager()->addChild(nullptr, node);
    smi.focusManager()->setFocus(node);

    auto state = smi.focusState();
    CHECK(state.hasFocus == true);
    CHECK(state.expectsKeyboardInput == false);
}

TEST_CASE("StateMachineInstance::focusState reports keyboard expectation when "
          "focused focusable accepts keys",
          "[FocusState]")
{
    NoOpFactory factory;
    Artboard artboard(&factory);
    auto instance = artboard.instance();
    StateMachine machine;
    StateMachineInstance smi(&machine, instance.get());

    KeyboardAcceptingFocusable kf;
    auto node = make_rcp<FocusNode>(&kf);
    smi.focusManager()->addChild(nullptr, node);
    smi.focusManager()->setFocus(node);

    auto state = smi.focusState();
    CHECK(state.hasFocus == true);
    CHECK(state.expectsKeyboardInput == true);
}

TEST_CASE("StateMachineInstance::focusState clears when focus is cleared",
          "[FocusState]")
{
    NoOpFactory factory;
    Artboard artboard(&factory);
    auto instance = artboard.instance();
    StateMachine machine;
    StateMachineInstance smi(&machine, instance.get());

    KeyboardAcceptingFocusable kf;
    auto node = make_rcp<FocusNode>(&kf);
    smi.focusManager()->addChild(nullptr, node);
    smi.focusManager()->setFocus(node);

    REQUIRE(smi.focusState().hasFocus == true);

    smi.focusManager()->clearFocus();

    auto state = smi.focusState();
    CHECK(state.hasFocus == false);
    CHECK(state.expectsKeyboardInput == false);
}

TEST_CASE("StateMachineInstance::focusState tracks switches between focusables",
          "[FocusState]")
{
    NoOpFactory factory;
    Artboard artboard(&factory);
    auto instance = artboard.instance();
    StateMachine machine;
    StateMachineInstance smi(&machine, instance.get());

    MockFocusable plain;
    KeyboardAcceptingFocusable kf;
    auto plainNode = make_rcp<FocusNode>(&plain);
    auto kfNode = make_rcp<FocusNode>(&kf);
    smi.focusManager()->addChild(nullptr, plainNode);
    smi.focusManager()->addChild(nullptr, kfNode);

    smi.focusManager()->setFocus(plainNode);
    {
        auto state = smi.focusState();
        CHECK(state.hasFocus == true);
        CHECK(state.expectsKeyboardInput == false);
    }

    smi.focusManager()->setFocus(kfNode);
    {
        auto state = smi.focusState();
        CHECK(state.hasFocus == true);
        CHECK(state.expectsKeyboardInput == true);
    }

    smi.focusManager()->setFocus(plainNode);
    {
        auto state = smi.focusState();
        CHECK(state.hasFocus == true);
        CHECK(state.expectsKeyboardInput == false);
    }
}

TEST_CASE("StateMachineInstance::focusState uses external focus manager when "
          "set",
          "[FocusState]")
{
    NoOpFactory factory;
    Artboard artboard(&factory);
    auto instance = artboard.instance();
    StateMachine machine;
    StateMachineInstance smi(&machine, instance.get());

    FocusManager external;
    KeyboardAcceptingFocusable kf;
    auto node = make_rcp<FocusNode>(&kf);
    external.addChild(nullptr, node);
    external.setFocus(node);

    // Before swapping, internal manager has nothing focused.
    CHECK(smi.focusState().hasFocus == false);

    smi.setExternalFocusManager(&external);

    auto state = smi.focusState();
    CHECK(state.hasFocus == true);
    CHECK(state.expectsKeyboardInput == true);
}

TEST_CASE("StateMachineInstance::clearFocus clears internal focus manager",
          "[FocusState]")
{
    NoOpFactory factory;
    Artboard artboard(&factory);
    auto instance = artboard.instance();
    StateMachine machine;
    StateMachineInstance smi(&machine, instance.get());

    KeyboardAcceptingFocusable kf;
    auto node = make_rcp<FocusNode>(&kf);
    smi.focusManager()->addChild(nullptr, node);
    smi.focusManager()->setFocus(node);

    REQUIRE(smi.focusState().hasFocus == true);

    smi.clearFocus();

    auto state = smi.focusState();
    CHECK(state.hasFocus == false);
    CHECK(state.expectsKeyboardInput == false);
}

} // namespace rive

TEST_CASE("FocusManager skips collapsed nodes and fully transparent nodes",
          "[FocusManager]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/focus_collapsing.riv", &silver);

    auto artboard = file->artboardDefault();
    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    auto vmi = file->createDefaultViewModelInstance(artboard.get());
    auto focusManager = artboard->focusManager();
    auto opacityProp =
        vmi->propertyValue("opacity")->as<rive::ViewModelInstanceNumber>();
    auto isMainLayout2VisibleProp = vmi->propertyValue("isMainLayout2Visible")
                                        ->as<rive::ViewModelInstanceBoolean>();

    stateMachine->bindViewModelInstance(vmi);
    // ===> Frame 0
    auto renderer = silver.makeRenderer();
    stateMachine->advanceAndApply(0.016f);
    // ===> Frame 1
    artboard->draw(renderer.get());
    silver.addFrame();

    focusManager->focusNext();
    focusManager->focusNext();
    // Focus on an element
    REQUIRE(focusManager->primaryFocus() != nullptr);
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    // ===> Frame 2
    silver.addFrame();
    // Hide the element, ensure the focus has been dropped
    opacityProp->propertyValue(0);
    // First advance sets the opacity to 0
    stateMachine->advanceAndApply(0.016f);
    // Next frame the focus is dropped
    stateMachine->advanceAndApply(0.016f);
    REQUIRE(focusManager->primaryFocus() == nullptr);
    artboard->draw(renderer.get());
    // ===> Frame 3
    silver.addFrame();

    opacityProp->propertyValue(1);
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    // ===> Frame 4
    silver.addFrame();
    focusManager->focusNext();
    focusManager->focusNext();
    stateMachine->advanceAndApply(0.016f);
    REQUIRE(focusManager->primaryFocus() != nullptr);
    artboard->draw(renderer.get());
    // ===> Frame 5
    silver.addFrame();
    isMainLayout2VisibleProp->propertyValue(false);
    stateMachine->advanceAndApply(0.016f);
    focusManager->focusNext();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    // ===> Frame 6
    silver.addFrame();

    // Toggles only between visible focused elements
    focusManager->focusNext();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    // ===> Frame 7
    silver.addFrame();
    focusManager->focusNext();
    focusManager->focusNext();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    // ===> Frame 8
    silver.addFrame();

    // Fully rotates over all nodes
    isMainLayout2VisibleProp->propertyValue(true);
    stateMachine->advanceAndApply(0.016f);
    focusManager->focusNext();
    artboard->draw(renderer.get());
    // ===> Frame 9
    silver.addFrame();
    stateMachine->advanceAndApply(0.016f);
    focusManager->focusNext();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    // ===> Frame 10
    silver.addFrame();
    focusManager->focusNext();
    stateMachine->advanceAndApply(0.016f);
    focusManager->focusNext();
    artboard->draw(renderer.get());
    // ===> Frame 11
    silver.addFrame();
    focusManager->focusNext();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    // ===> Frame 12
    silver.addFrame();
    focusManager->focusNext();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("focus_collapsing"));
}

TEST_CASE("Focused elements receive keyboard inputs", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/keyboard_listener.riv", &silver);

    auto artboard = file->artboardDefault();
    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    auto renderer = silver.makeRenderer();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    silver.addFrame();

    auto focusManager = artboard->focusManager();
    // Child index 5
    focusManager->focusPrevious();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    silver.addFrame();
    focusManager->keyInput(rive::Key::space,
                           rive::KeyModifiers::none,
                           false,
                           false);
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    silver.addFrame();

    // Child index 4
    focusManager->focusPrevious();
    // Child index 3
    focusManager->focusPrevious();
    // Child index 2
    focusManager->focusPrevious();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    silver.addFrame();
    focusManager->keyInput(rive::Key::space,
                           rive::KeyModifiers::none,
                           false,
                           false);
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    silver.addFrame();

    // Child index 1
    focusManager->focusPrevious();
    // Child index 0
    focusManager->focusPrevious();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    silver.addFrame();
    focusManager->keyInput(rive::Key::space,
                           rive::KeyModifiers::none,
                           false,
                           false);
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    silver.addFrame();
    focusManager->focusPrevious();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    silver.addFrame();
    focusManager->keyInput(rive::Key::space,
                           rive::KeyModifiers::none,
                           false,
                           false);
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("keyboard_listener"));
}

TEST_CASE("Keyboard inputs with different key combinations", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/keyboard_listener.riv", &silver);

    auto artboard = file->artboardNamed("KeyboardInput");
    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);
    auto keyCountProp =
        vmi->propertyValue("keyCount")->as<rive::ViewModelInstanceNumber>();

    stateMachine->bindViewModelInstance(vmi);
    auto renderer = silver.makeRenderer();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    silver.addFrame();

    auto focusManager = artboard->focusManager();
    focusManager->focusNext();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    silver.addFrame();
    // Key "a" on phase down with no modifiers is captured
    focusManager->keyInput(rive::Key::a, rive::KeyModifiers::none, true, false);
    stateMachine->advanceAndApply(0.016f);
    CHECK(keyCountProp->propertyValue() == 1);
    artboard->draw(renderer.get());
    silver.addFrame();
    // Key "a" on phase repeat with no modifiers is not captured
    focusManager->keyInput(rive::Key::a, rive::KeyModifiers::none, true, true);
    stateMachine->advanceAndApply(0.016f);
    CHECK(keyCountProp->propertyValue() == 1);
    // Key "a" on phase up with no modifiers is captured
    focusManager->keyInput(rive::Key::a,
                           rive::KeyModifiers::none,
                           false,
                           false);
    stateMachine->advanceAndApply(0.016f);
    CHECK(keyCountProp->propertyValue() == 2);

    // Key "a" on phase down with modifiers is not captured
    focusManager->keyInput(rive::Key::a,
                           rive::KeyModifiers::shift,
                           true,
                           false);
    stateMachine->advanceAndApply(0.016f);
    CHECK(keyCountProp->propertyValue() == 2);

    // Key "e" on any phase is not captured
    focusManager->keyInput(rive::Key::e,
                           rive::KeyModifiers::none,
                           false,
                           false);
    focusManager->keyInput(rive::Key::e, rive::KeyModifiers::none, true, true);
    focusManager->keyInput(rive::Key::e, rive::KeyModifiers::none, true, false);
    CHECK(keyCountProp->propertyValue() == 2);
    stateMachine->advanceAndApply(0.016f);
    // Key "b" on phase down with no modifiers is NOT captured
    focusManager->keyInput(rive::Key::b, rive::KeyModifiers::none, true, false);
    // Key "b" on phase up with no modifiers is NOT captured
    CHECK(keyCountProp->propertyValue() == 2);
    stateMachine->advanceAndApply(0.016f);
    focusManager->keyInput(rive::Key::b,
                           rive::KeyModifiers::none,
                           false,
                           false);
    stateMachine->advanceAndApply(0.016f);
    CHECK(keyCountProp->propertyValue() == 3);
    // Key "b" on phase repeat with no modifiers is captured
    focusManager->keyInput(rive::Key::b, rive::KeyModifiers::none, true, true);
    stateMachine->advanceAndApply(0.016f);
    CHECK(keyCountProp->propertyValue() == 4);
    // Key "d" on phase down with no modifiers is not captured
    focusManager->keyInput(rive::Key::d, rive::KeyModifiers::none, true, false);
    stateMachine->advanceAndApply(0.016f);
    CHECK(keyCountProp->propertyValue() == 4);
    // Key "d" on phase down with shift + command modifiers is captured
    focusManager->keyInput(rive::Key::d,
                           rive::KeyModifiers::shift | rive::KeyModifiers::meta,
                           true,
                           false);
    stateMachine->advanceAndApply(0.016f);
    CHECK(keyCountProp->propertyValue() == 5);
    // Key "c" on phase down with shift + command modifiers is NOT captured
    focusManager->keyInput(rive::Key::c,
                           rive::KeyModifiers::shift | rive::KeyModifiers::meta,
                           true,
                           false);
    stateMachine->advanceAndApply(0.016f);
    CHECK(keyCountProp->propertyValue() == 5);
    // Key "c" on phase down with shift modifiers is captured
    focusManager->keyInput(rive::Key::c,
                           rive::KeyModifiers::shift,
                           true,
                           false);
    stateMachine->advanceAndApply(0.016f);
    CHECK(keyCountProp->propertyValue() == 6);
    // Key "x" on phase down with shift modifiers is NOT captured
    focusManager->keyInput(rive::Key::x,
                           rive::KeyModifiers::shift,
                           true,
                           false);
    stateMachine->advanceAndApply(0.016f);
    CHECK(keyCountProp->propertyValue() == 6);

    artboard->draw(renderer.get());

    CHECK(silver.matches("keyboard_listener-KeyboardInput"));
}

TEST_CASE("Text input events are handled on focused nodes", "[silver]")
{
    auto file = ReadRiveFile("assets/text_input_event.riv");

    auto artboard = file->artboardDefault();

    auto stateMachine = artboard->stateMachineAt(0);

    auto vmi = file->createViewModelInstance(artboard.get());
    auto isFocusedProp =
        vmi->propertyValue("isFocused")->as<rive::ViewModelInstanceBoolean>();
    auto hasKeyedProp =
        vmi->propertyValue("hasKeyed")->as<rive::ViewModelInstanceBoolean>();
    auto hasTextedProp =
        vmi->propertyValue("hasTexted")->as<rive::ViewModelInstanceBoolean>();

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.016f);

    auto focusManager = artboard->focusManager();
    focusManager->focusNext();
    stateMachine->advanceAndApply(0.016f);
    CHECK(isFocusedProp->propertyValue() == true);
    CHECK(hasKeyedProp->propertyValue() == false);
    CHECK(hasTextedProp->propertyValue() == false);

    // Key "b" on phase down with no modifiers is NOT captured
    focusManager->keyInput(rive::Key::b, rive::KeyModifiers::none, true, false);
    stateMachine->advanceAndApply(0.016f);
    CHECK(isFocusedProp->propertyValue() == true);
    CHECK(hasKeyedProp->propertyValue() == false);
    CHECK(hasTextedProp->propertyValue() == false);
    // Text "b" on captured by text but not by key
    focusManager->textInput("b");
    stateMachine->advanceAndApply(0.016f);
    CHECK(isFocusedProp->propertyValue() == true);
    CHECK(hasKeyedProp->propertyValue() == false);
    CHECK(hasTextedProp->propertyValue() == true);

    // Key "a" on phase down with no modifiers is captured by key
    focusManager->keyInput(rive::Key::a, rive::KeyModifiers::none, true, false);
    stateMachine->advanceAndApply(0.016f);
    CHECK(isFocusedProp->propertyValue() == true);
    CHECK(hasKeyedProp->propertyValue() == true);
    CHECK(hasTextedProp->propertyValue() == true);
}

TEST_CASE("Focus traversal listener actions", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/focus_traversal.riv", &silver);

    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);

    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);

    auto vmi = file->createDefaultViewModelInstance(artboard.get());

    auto renderer = silver.makeRenderer();
    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    silver.addFrame();

    // There are 2 rows of buttons
    // Top row: Top / Right / Down / Left
    // Bottom row: Prev / Next

    // Click on Next
    stateMachine->pointerDown(rive::Vec2D(180, 450));
    stateMachine->pointerUp(rive::Vec2D(180, 450));
    stateMachine->advanceAndApply(0.016f);
    // Second advance to apply focus changes
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    silver.addFrame();

    // Click on Prev twice to reenter focus tree
    stateMachine->pointerDown(rive::Vec2D(60, 450));
    stateMachine->pointerUp(rive::Vec2D(60, 450));
    stateMachine->advanceAndApply(0.016f);
    stateMachine->pointerDown(rive::Vec2D(60, 450));
    stateMachine->pointerUp(rive::Vec2D(60, 450));
    stateMachine->advanceAndApply(0.016f);
    // Second advance to apply focus changes
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    silver.addFrame();

    // Click on Up
    stateMachine->pointerDown(rive::Vec2D(60, 350));
    stateMachine->pointerUp(rive::Vec2D(60, 350));
    stateMachine->advanceAndApply(0.016f);
    // Second advance to apply focus changes
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    silver.addFrame();

    // Click on Left
    stateMachine->pointerDown(rive::Vec2D(420, 350));
    stateMachine->pointerUp(rive::Vec2D(420, 350));
    stateMachine->advanceAndApply(0.016f);
    // Second advance to apply focus changes
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    silver.addFrame();

    // Click on Down
    stateMachine->pointerDown(rive::Vec2D(300, 350));
    stateMachine->pointerUp(rive::Vec2D(300, 350));
    stateMachine->advanceAndApply(0.016f);
    // Second advance to apply focus changes
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    silver.addFrame();

    // Click on Right
    stateMachine->pointerDown(rive::Vec2D(180, 350));
    stateMachine->pointerUp(rive::Vec2D(180, 350));
    stateMachine->advanceAndApply(0.016f);
    // Second advance to apply focus changes
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("focus_traversal"));
}

TEST_CASE("Focus traversal clears focus when it reaches edge of root scope",
          "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/focusable_element.riv", &silver);

    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);

    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);

    auto vmi = file->createViewModelInstance(artboard.get());

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);
    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());
    silver.addFrame();
    stateMachine->focusManager()->focusNext();
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());
    silver.addFrame();
    stateMachine->focusManager()->focusNext();
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());
    silver.addFrame();
    stateMachine->focusManager()->focusNext();
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());
    silver.addFrame();
    stateMachine->focusManager()->focusNext();
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());
    silver.addFrame();
    stateMachine->focusManager()->focusNext();
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());
    silver.addFrame();
    stateMachine->focusManager()->focusNext();
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());
    silver.addFrame();
    stateMachine->focusManager()->focusNext();
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("focusable_element"));
}

TEST_CASE("ArtboardComponentList list scope is registered on shared "
          "FocusManager",
          "[FocusManager][list]")
{
    auto file = ReadRiveFile("assets/component_list_1.riv");
    auto artboard = file->artboard("Main")->instance();
    REQUIRE(artboard != nullptr);
    auto vmi = file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(vmi != nullptr);
    artboard->bindViewModelInstance(vmi);
    auto sm = artboard->stateMachineAt(0);
    REQUIRE(sm != nullptr);
    artboard->advance(0.0f);

    auto* list = artboard->find<rive::ArtboardComponentList>("List");
    REQUIRE(list != nullptr);
    auto* fm = artboard->focusManager();
    REQUIRE(fm != nullptr);

    artboard->buildFocusTree(artboard->focusManager(), nullptr);
    auto scope = list->listScopeFocusNode();
    REQUIRE(scope != nullptr);
    CHECK(scope->manager() == fm);
    CHECK(scope->name() == "ArtboardComponentListScope");
    CHECK(scope->canFocus() == true);
    CHECK(scope->canTraverse() == true);
    CHECK(scope->focusable() == nullptr);
}

TEST_CASE("List under Node: when parent has a direct FocusData, "
          "findClosestFocusNode from list matches that node",
          "[FocusManager][list]")
{
    // buildFocusTreeVisit pass-1: at most one direct child FocusData per
    // container; if present, its focusNode is the scope for siblings (e.g. the
    // list host). The walk-based fallback from the old findClosest for the
    // no-direct-FocusData case is not used by the focus build anymore.
    auto file = ReadRiveFile("assets/component_list_1.riv");
    auto artboard = file->artboard("Main")->instance();
    REQUIRE(artboard != nullptr);
    auto vmi = file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(vmi != nullptr);
    artboard->bindViewModelInstance(vmi);
    auto sm = artboard->stateMachineAt(0);
    REQUIRE(sm != nullptr);
    artboard->advance(0.0f);

    auto* list = artboard->find<rive::ArtboardComponentList>("List");
    REQUIRE(list != nullptr);
    auto* p = list->parent();
    REQUIRE(p != nullptr);
    REQUIRE(p->is<rive::Node>());

    rive::rcp<rive::FocusNode> fromFirstDirectFd;
    for (auto* ch : p->as<rive::Node>()->children())
    {
        if (ch != nullptr && ch->is<rive::FocusData>())
        {
            fromFirstDirectFd = ch->as<rive::FocusData>()->focusNode();
            break;
        }
    }
    if (fromFirstDirectFd != nullptr)
    {
        CHECK(rive::FocusData::findClosestFocusNode(list) == fromFirstDirectFd);
    }
}

TEST_CASE("Focus is correctly built and updated for lists", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/list_focus_order.riv", &silver);

    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);

    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    auto focusManager = stateMachine->focusManager();

    auto vmi = file->createDefaultViewModelInstance(artboard.get());
    auto stageProcessedProp = vmi->propertyValue("stageProcessed")
                                  ->as<rive::ViewModelInstanceBoolean>();
    auto stageCountProp =
        vmi->propertyValue("stageCount")->as<rive::ViewModelInstanceNumber>();

    auto renderer = silver.makeRenderer();
    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    silver.addFrame();

    // Focuses on first element of tree
    focusManager->focusNext();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    silver.addFrame();

    // Focuses on last element of list
    focusManager->focusNext();
    focusManager->focusNext();
    focusManager->focusNext();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    silver.addFrame();

    // Inserts one element at end of list
    stageProcessedProp->propertyValue(false);
    stageCountProp->propertyValue(1);
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    silver.addFrame();

    // Focus is on that new element
    focusManager->focusNext();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    silver.addFrame();

    // Focused elements is moved in the list and keeps focus
    stageProcessedProp->propertyValue(false);
    stageCountProp->propertyValue(2);
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    silver.addFrame();

    // Focusing on the next element correctly focuses on the next element on the
    // list
    focusManager->focusNext();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    silver.addFrame();

    // Removing the focused element from the list, clears the focus
    stageProcessedProp->propertyValue(false);
    stageCountProp->propertyValue(3);
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    silver.addFrame();

    // Focuses back on first element of tree
    focusManager->focusNext();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("list_focus_order"));
}