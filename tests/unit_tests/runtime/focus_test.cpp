#include <catch.hpp>
#include "rive/input/focus_node.hpp"
#include "rive/input/focus_manager.hpp"
#include "utils/serializing_factory.hpp"
#include "rive_file_reader.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/viewmodel/viewmodel_instance_boolean.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"

namespace rive
{

// Mock Focusable for testing
class MockFocusable : public Focusable
{
public:
    int keyInputCount = 0;
    int textInputCount = 0;
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

    manager.setFocus(node);

    // With focus, input is routed
    CHECK(manager.keyInput(Key::b, KeyModifiers::none, true, false) == true);
    CHECK(focusable.keyInputCount == 1);
    CHECK(focusable.lastKey == Key::b);

    CHECK(manager.textInput("world") == true);
    CHECK(focusable.textInputCount == 1);
    CHECK(focusable.lastText == "world");
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
    auto renderer = silver.makeRenderer();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    silver.addFrame();

    focusManager->focusNext();
    // Focus on an element
    REQUIRE(focusManager->primaryFocus() != nullptr);
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    silver.addFrame();

    // Hide the element, ensure the focus has been dropped
    opacityProp->propertyValue(0);
    // First advance sets the opacity to 0
    stateMachine->advanceAndApply(0.016f);
    // Next frame the focus is dropped
    stateMachine->advanceAndApply(0.016f);
    REQUIRE(focusManager->primaryFocus() == nullptr);
    artboard->draw(renderer.get());
    silver.addFrame();

    opacityProp->propertyValue(1);
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    silver.addFrame();
    focusManager->focusNext();
    stateMachine->advanceAndApply(0.016f);
    REQUIRE(focusManager->primaryFocus() != nullptr);
    artboard->draw(renderer.get());
    silver.addFrame();
    isMainLayout2VisibleProp->propertyValue(false);
    stateMachine->advanceAndApply(0.016f);
    focusManager->focusNext();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    silver.addFrame();

    // Toggles only between visible focused elements
    focusManager->focusNext();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    silver.addFrame();
    focusManager->focusNext();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    silver.addFrame();

    // Fully rotates over all nodes
    isMainLayout2VisibleProp->propertyValue(true);
    stateMachine->advanceAndApply(0.016f);
    focusManager->focusNext();
    artboard->draw(renderer.get());
    silver.addFrame();
    stateMachine->advanceAndApply(0.016f);
    focusManager->focusNext();
    artboard->draw(renderer.get());
    silver.addFrame();
    stateMachine->advanceAndApply(0.016f);
    focusManager->focusNext();
    artboard->draw(renderer.get());
    silver.addFrame();
    stateMachine->advanceAndApply(0.016f);
    focusManager->focusNext();
    artboard->draw(renderer.get());
    silver.addFrame();
    stateMachine->advanceAndApply(0.016f);
    focusManager->focusNext();
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