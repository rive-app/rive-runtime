#include <catch.hpp>
#include "rive/animation/focus_action_clear.hpp"
#include "rive/animation/focus_action_traversal.hpp"
#include "rive/animation/transition_condition_op.hpp"
#include "rive/animation/transition_focus_condition.hpp"
#include "rive/animation/transition_property_component_comparator.hpp"
#include "rive/animation/listener_invocation.hpp"
#include "rive/animation/nested_state_machine.hpp"
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
#include "rive/nested_artboard.hpp"
#include "rive/viewmodel/viewmodel_instance_artboard.hpp"
#include "rive/viewmodel/viewmodel_instance_boolean.hpp"
#include "rive/viewmodel/viewmodel_instance_viewmodel.hpp"
#include "rive/viewmodel/runtime/viewmodel_runtime.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_runtime.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_list_runtime.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_boolean_runtime.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "rive/animation/listener_invocation.hpp"
#include "rive/input/gamepad_batch.hpp"
#include "rive/input/gamepad_snapshot.hpp"
#include "rive/input/standard_gamepad.hpp"
#include <cstring>

namespace rive
{

namespace
{
// A root artboard normally gets its FocusManager from File::instanceArtboard.
// Tests that build an Artboard directly and instance it skip that path, so
// give the instance a manager explicitly — otherwise focusManager() is null.
std::unique_ptr<ArtboardInstance> instanceWithFocus(Artboard& artboard)
{
    auto instance = artboard.instance();
    instance->ensureFocusManager();
    return instance;
}

// Minimal builder for the little-endian gamepad batch wire format documented
// in `gamepad_batch.cpp`, enough to connect a pad and press one button.
// gamepad_test.cpp has a fuller version for the parsing tests.
struct GamepadWire
{
    std::vector<uint8_t> buf;

    void u8(uint8_t v) { buf.push_back(v); }
    void u32(uint32_t v)
    {
        buf.push_back(static_cast<uint8_t>(v));
        buf.push_back(static_cast<uint8_t>(v >> 8));
        buf.push_back(static_cast<uint8_t>(v >> 16));
        buf.push_back(static_cast<uint8_t>(v >> 24));
    }
    void f32(float v)
    {
        uint32_t bits;
        std::memcpy(&bits, &v, sizeof(bits));
        u32(bits);
    }

    GamepadWire() { u32(kGamepadBatchWireVersion); }

    // A standard-mapped pad: 17 buttons, 4 axes, all at rest.
    void connected(int32_t deviceId)
    {
        u8(static_cast<uint8_t>(GamepadRecordType::connected));
        u32(static_cast<uint32_t>(deviceId));
        u8(0);  // mapping: standard
        u8(17); // buttons
        u8(4);  // axes
        u8(0);  // padding to align the float arrays
        for (int i = 0; i < 17 + 4; i++)
        {
            f32(0.f);
        }
    }

    void button(int32_t deviceId, StandardGamepadButton index, float value)
    {
        u8(static_cast<uint8_t>(GamepadRecordType::update));
        u32(static_cast<uint32_t>(deviceId));
        u8(1); // nChanges
        u8(static_cast<uint8_t>(GamepadInputChangeKind::button));
        u8(static_cast<uint8_t>(index));
        f32(value);
    }
};
} // namespace

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

    bool eligible = true;
    bool isEligibleForFocusTraversal() const override { return eligible; }

    // Lets a test say which artboard tree a node belongs to, for the
    // root-scoped focus calls. Null (the default) means "not attributable to
    // any root", which is how a host-created FocusNode reads.
    Artboard* artboard = nullptr;
    Artboard* focusableArtboard() const override { return artboard; }
};

// A focusable that consumes typed text, the way TextInput (and a FocusData
// whose parent is one) does.
class MockTextFocusable : public MockFocusable
{
public:
    bool acceptsTextInput() const override { return true; }
};

// =============================================================================
// FocusNode Tests
// =============================================================================

TEST_CASE("primaryFocusAcceptsText reports text-consuming focus",
          "[FocusManager]")
{
    FocusManager manager;
    MockFocusable plain;
    MockTextFocusable text;

    auto plainNode = make_rcp<FocusNode>(&plain);
    auto textNode = make_rcp<FocusNode>(&text);
    // A non-text child under a text-accepting parent: the answer bubbles up
    // through ancestors, like text input routing does.
    auto childOfText = make_rcp<FocusNode>(&plain);
    manager.addChild(nullptr, plainNode);
    manager.addChild(nullptr, textNode);
    manager.addChild(textNode, childOfText);

    // Nothing focused.
    CHECK(manager.primaryFocusAcceptsText() == false);

    // A focusable that doesn't take text.
    manager.setFocus(plainNode);
    CHECK(manager.primaryFocusAcceptsText() == false);

    // One that does.
    manager.setFocus(textNode);
    CHECK(manager.primaryFocusAcceptsText() == true);

    // A descendant of one that does.
    manager.setFocus(childOfText);
    CHECK(manager.primaryFocusAcceptsText() == true);

    manager.clearFocus();
    CHECK(manager.primaryFocusAcceptsText() == false);
}

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

TEST_CASE("a node that loses its Focusable stops being a focus stop",
          "[FocusNode]")
{
    // ~FocusData clears its node's Focusable. If that node is still in the
    // tree (a detach that was never paired with a re-add, a manager torn down
    // around it), nothing is left that could report it collapsed or hidden —
    // so it must not stay eligible, or it becomes a focus stop that no
    // visibility change can ever remove.
    FocusManager manager;
    MockFocusable focusable;

    auto live = make_rcp<FocusNode>(&focusable);
    auto defunct = make_rcp<FocusNode>(&focusable);
    manager.addChild(nullptr, live);
    manager.addChild(nullptr, defunct);

    // Both are reachable while backed.
    CHECK(manager.getTraversableNodes(nullptr).size() == 2);

    defunct->clearFocusable();
    CHECK(defunct->hadFocusable());

    auto traversable = manager.getTraversableNodes(nullptr);
    REQUIRE(traversable.size() == 1);
    CHECK(traversable[0] == live.get());

    // It can't be focused programmatically either.
    manager.setFocus(defunct);
    CHECK(manager.primaryFocusPtr() != defunct.get());
}

TEST_CASE("a node that never had a Focusable stays focusable", "[FocusNode]")
{
    // Hosts create bare FocusNodes as focus targets through this API; they
    // have no Focusable to consult and must keep working. Only a node that
    // *lost* its backing is treated as defunct.
    FocusManager manager;

    auto external = make_rcp<FocusNode>();
    CHECK_FALSE(external->hadFocusable());
    manager.addChild(nullptr, external);

    auto traversable = manager.getTraversableNodes(nullptr);
    REQUIRE(traversable.size() == 1);
    CHECK(traversable[0] == external.get());

    manager.setFocus(external);
    CHECK(manager.primaryFocusPtr() == external.get());
}

TEST_CASE("traversal still descends through a defunct node to live children",
          "[FocusNode]")
{
    // A defunct node stops being a stop, but its children may still be live —
    // traversal must keep reaching them rather than skipping the subtree.
    FocusManager manager;
    MockFocusable focusable;

    auto defunctScope = make_rcp<FocusNode>(&focusable);
    auto liveChild = make_rcp<FocusNode>(&focusable);
    manager.addChild(nullptr, defunctScope);
    manager.addChild(defunctScope, liveChild);

    defunctScope->clearFocusable();

    // Tab traversal walks through the defunct scope and lands on the child.
    CHECK(manager.focusNext());
    CHECK(manager.primaryFocusPtr() == liveChild.get());

    // Note: setFocus() aimed directly at the defunct node stays a no-op. Its
    // descend-to-first-leaf step is gated on the requested target being
    // eligible, so an ineligible target is rejected outright — the same as any
    // other collapsed or hidden target.
    manager.clearFocus();
    manager.setFocus(defunctScope);
    CHECK(manager.primaryFocusPtr() == nullptr);
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

TEST_CASE("adding a node claims its whole subtree for the manager",
          "[FocusManager]")
{
    // detachChild clears the manager pointer across the entire subtree, so
    // addChild has to restore it across the entire subtree too. A host that
    // rebuilds by detaching and re-adding a set of nodes (the Dart editor does
    // exactly this) never touches the descendants underneath them; if those
    // kept a null manager, their FocusData could no longer remove them from
    // the tree when it died, stranding them here forever.
    FocusManager manager;
    MockFocusable focusable;

    auto parent = make_rcp<FocusNode>(&focusable);
    auto child = make_rcp<FocusNode>(&focusable);
    auto grandchild = make_rcp<FocusNode>(&focusable);

    manager.addChild(nullptr, parent);
    manager.addChild(parent, child);
    manager.addChild(child, grandchild);

    CHECK(parent->manager() == &manager);
    CHECK(child->manager() == &manager);
    CHECK(grandchild->manager() == &manager);

    // Detach only the top of the subtree — descendants come along untouched.
    manager.detachChild(parent);
    CHECK(parent->manager() == nullptr);
    CHECK(child->manager() == nullptr);
    CHECK(grandchild->manager() == nullptr);
    CHECK(parent->children().size() == 1);

    // Re-adding just the top has to re-claim everything beneath it.
    manager.addChild(nullptr, parent);
    CHECK(parent->manager() == &manager);
    CHECK(child->manager() == &manager);
    CHECK(grandchild->manager() == &manager);
}

TEST_CASE("a rebuild that misses a node still lets its FocusData clean up",
          "[FocusManager]")
{
    // End-to-end shape of the editor's rebuildFocusHierarchy: collect a subset
    // of the tree, detach each collected node, re-add it. A node sitting under
    // a collected node that the collection itself missed used to come out of
    // that cycle with no manager pointer — and then its FocusData could never
    // remove it, so it survived as an unbacked node that
    // focusNodeEligibleForFocus treated as eligible: a focus stop nothing
    // could take out.
    FocusManager manager;
    MockFocusable focusable;

    auto root = make_rcp<FocusNode>(&focusable);
    manager.addChild(nullptr, root);

    {
        FocusData missedByTheRebuild;
        auto missedNode = missedByTheRebuild.focusNode();
        manager.addChild(root, missedNode);

        // The rebuild touches only `root`; `missedNode` is never collected.
        manager.detachChild(root);
        manager.addChild(nullptr, root);

        CHECK(missedNode->manager() == &manager);
        CHECK(root->children().size() == 1);
    }

    // The FocusData is gone, and so is its node — no unbacked leftover.
    CHECK(root->children().empty());
    // And even if one did survive, it must not be a focus stop.
    CHECK(manager.getTraversableNodes(root.get()).empty());
}

TEST_CASE("a dying FocusData removes its node via an ancestor's manager",
          "[FocusData]")
{
    // A node can sit in a live tree while holding no manager pointer of its
    // own. ~FocusData still has to take it out: left behind, it would keep a
    // cleared Focusable and nothing could ever report it hidden.
    FocusManager manager;
    MockFocusable focusable;

    auto scope = make_rcp<FocusNode>(&focusable);
    manager.addChild(nullptr, scope);
    REQUIRE(scope->manager() == &manager);

    {
        FocusData data;
        auto node = data.focusNode();
        // Parent it through FocusNode directly, which doesn't hand out a
        // manager pointer — the same state a detach/re-add cycle used to
        // leave descendants in.
        scope->addChild(node);
        REQUIRE(node->manager() == nullptr);
        REQUIRE(scope->children().size() == 1);

        manager.setFocus(node);
        REQUIRE(manager.primaryFocusPtr() == node.get());
    }

    // Removed through the nearest registered ancestor, so focus is cleared
    // too — not merely detached.
    CHECK(scope->children().empty());
    CHECK(manager.primaryFocusPtr() == nullptr);
}

TEST_CASE("a dying FocusData detaches its node when no manager is reachable",
          "[FocusData]")
{
    // Nothing in the chain is registered (a manager torn down around the
    // subtree). There is no focus to clear, but the node still must not stay
    // parented.
    auto scope = make_rcp<FocusNode>();

    {
        FocusData data;
        scope->addChild(data.focusNode());
        REQUIRE(scope->children().size() == 1);
    }

    CHECK(scope->children().empty());
}

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

// Focusable that can report live world bounds, like FocusData/TextInput do.
class MockBoundedFocusable : public MockFocusable
{
public:
    bool hasBounds = true;
    AABB liveBounds = AABB(10, 20, 110, 220);

    bool worldBounds(AABB& outBounds) override
    {
        if (!hasBounds)
        {
            return false;
        }
        outBounds = liveBounds;
        return true;
    }
};

TEST_CASE("primaryFocusBounds prefers live focusable bounds over cached",
          "[FocusManager]")
{
    FocusManager manager;
    MockBoundedFocusable focusable;
    auto node = make_rcp<FocusNode>(&focusable);
    manager.addChild(nullptr, node);
    manager.setFocus(node);

    // Stale bounds cached on the node by a previous update pass.
    node->worldBounds(AABB(1, 2, 3, 4));

    AABB bounds;
    REQUIRE(manager.primaryFocusBounds(bounds) == true);
    CHECK(bounds.minX == 10);
    CHECK(bounds.minY == 20);
    CHECK(bounds.maxX == 110);
    CHECK(bounds.maxY == 220);

    // When the focusable cannot compute, the cached bounds remain the
    // fallback.
    focusable.hasBounds = false;
    REQUIRE(manager.primaryFocusBounds(bounds) == true);
    CHECK(bounds.minX == 1);
    CHECK(bounds.maxY == 4);

    manager.clearFocus();
    CHECK(manager.primaryFocusBounds(bounds) == false);
}

TEST_CASE("primaryFocusBounds uses cached bounds without a focusable",
          "[FocusManager]")
{
    // Externally-managed nodes (e.g. created over FFI by a host) have no
    // focusable; their host pushes bounds into the node directly.
    FocusManager manager;
    auto node = make_rcp<FocusNode>();
    manager.addChild(nullptr, node);
    manager.setFocus(node);

    AABB bounds;
    CHECK(manager.primaryFocusBounds(bounds) == false);

    node->worldBounds(AABB(5, 6, 7, 8));
    REQUIRE(manager.primaryFocusBounds(bounds) == true);
    CHECK(bounds.minX == 5);
    CHECK(bounds.maxY == 8);
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

TEST_CASE("hasFocusableContent invalidates when canFocus toggles after caching",
          "[FocusManager]")
{
    FocusManager manager;
    // Both structural: no focusable backing, canFocus=false.
    auto scope = FocusNode::makeStructuralScope();
    auto child = FocusNode::makeStructuralScope();
    manager.addChild(nullptr, scope);
    manager.addChild(scope, child);

    // Compute + cache the "no focusable content" answer.
    CHECK(manager.hasFocusableContent() == false);

    // A canFocus flip on a cached tree must be reflected.
    child->canFocus(true);
    CHECK(manager.hasFocusableContent() == true);

    child->canFocus(false);
    CHECK(manager.hasFocusableContent() == false);
}

TEST_CASE(
    "hasFocusableContent invalidates when focusable backing toggles after "
    "caching",
    "[FocusManager]")
{
    FocusManager manager;
    MockFocusable focusable;
    auto scope = FocusNode::makeStructuralScope();
    auto child = FocusNode::makeStructuralScope();
    manager.addChild(nullptr, scope);
    manager.addChild(scope, child);

    CHECK(manager.hasFocusableContent() == false);

    // Gaining a focusable backing counts even while canFocus stays false.
    child->setFocusable(&focusable);
    CHECK(manager.hasFocusableContent() == true);

    child->clearFocusable();
    CHECK(manager.hasFocusableContent() == false);
}

TEST_CASE("hasFocusableContent invalidates when a backed node is added then "
          "removed",
          "[FocusManager]")
{
    // Mirrors a data-bound nested-artboard swap: a structural scope gains a
    // focusable node on swap-in, then loses it on swap-out.
    FocusManager manager;
    MockFocusable focusable;
    auto scope = FocusNode::makeStructuralScope();
    manager.addChild(nullptr, scope);

    CHECK(manager.hasFocusableContent() == false);

    auto backed = make_rcp<FocusNode>(&focusable);
    manager.addChild(scope, backed);
    CHECK(manager.hasFocusableContent() == true);

    manager.removeChild(backed);
    CHECK(manager.hasFocusableContent() == false);
}

TEST_CASE("hasFocusableContent invalidates when the last root is erased",
          "[FocusManager]")
{
    // eraseRoot is the only invalidation for a root removed while migrating to
    // another manager; exercise it directly via a re-parent to a second
    // manager, which erases the node from the first manager's root list.
    FocusManager first;
    FocusManager second;
    auto node = make_rcp<FocusNode>();
    node->canFocus(true);
    first.addChild(nullptr, node);

    CHECK(first.hasFocusableContent() == true);

    // Migrating the root out of `first` empties its tree.
    second.addChild(nullptr, node);
    CHECK(first.hasFocusableContent() == false);
    CHECK(second.hasFocusableContent() == true);
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

    // The loop is the scope's whole subtree in pre-order, and a plain
    // FocusNode is focusable and traversable, so the scope is the first stop
    // in it: scope -> node1 -> node2 -> scope.
    CHECK(manager.primaryFocus() == scope);
    manager.focusNext();
    CHECK(manager.primaryFocus() == node1);
}

TEST_CASE("FocusManager closedLoop skips a scope that can't be focused",
          "[FocusManager]")
{
    FocusManager manager;
    auto scope = make_rcp<FocusNode>();
    auto node1 = make_rcp<FocusNode>();
    auto node2 = make_rcp<FocusNode>();

    scope->edgeBehavior(EdgeBehavior::closedLoop);
    // A pure container now has to say so; holding focusable children is no
    // longer enough to keep a node out of the order.
    scope->canFocus(false);

    manager.addChild(nullptr, scope);
    manager.addChild(scope, node1);
    manager.addChild(scope, node2);

    manager.setFocus(node2);
    manager.focusNext();

    // Wraps to the first stop in the subtree, which is now node1.
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

    // node2 is the last stop in the scope's subtree, so the next step would
    // leave it — which `stop` refuses.
    CHECK(manager.primaryFocus() == node2);
}

TEST_CASE("FocusManager traversal is pre-order in both directions",
          "[FocusManager]")
{
    FocusManager manager;
    auto root = make_rcp<FocusNode>();
    auto a = make_rcp<FocusNode>();
    auto a1 = make_rcp<FocusNode>();
    auto a2 = make_rcp<FocusNode>();
    auto b = make_rcp<FocusNode>();

    manager.addChild(nullptr, root);
    manager.addChild(root, a);
    manager.addChild(a, a1);
    manager.addChild(a, a2);
    manager.addChild(root, b);

    const std::vector<FocusNode*> order{root.get(),
                                        a.get(),
                                        a1.get(),
                                        a2.get(),
                                        b.get()};

    for (FocusNode* expected : order)
    {
        manager.focusNext();
        CHECK(manager.primaryFocusPtr() == expected);
    }

    // And Shift+Tab retraces it exactly.
    for (auto it = order.rbegin() + 1; it != order.rend(); ++it)
    {
        manager.focusPrevious();
        CHECK(manager.primaryFocusPtr() == *it);
    }
}

TEST_CASE("FocusManager a parent's flags do not decide for its children",
          "[FocusManager]")
{
    // The regression this rule change is about. A backed canFocus=false
    // container used to prune its entire subtree from traversal, which is how
    // an artboard-level <FocusData canFocus="false"/> — the documented way to
    // host keyboard listeners — took every focusable in the file out of Tab.
    MockFocusable containerFocusable, childFocusable, siblingFocusable;

    SECTION("canFocus=false parent")
    {
        FocusManager manager;
        auto container = make_rcp<FocusNode>(&containerFocusable);
        auto child = make_rcp<FocusNode>(&childFocusable);
        container->canFocus(false);
        manager.addChild(nullptr, container);
        manager.addChild(container, child);

        CHECK(manager.getTraversableNodes(nullptr).size() == 1);
        manager.focusNext();
        CHECK(manager.primaryFocus() == child);
    }

    SECTION("canTraverse=false parent")
    {
        FocusManager manager;
        auto container = make_rcp<FocusNode>(&containerFocusable);
        auto child = make_rcp<FocusNode>(&childFocusable);
        container->canTraverse(false);
        manager.addChild(nullptr, container);
        manager.addChild(container, child);

        manager.focusNext();
        CHECK(manager.primaryFocus() == child);
    }

    SECTION("directional navigation agrees")
    {
        FocusManager manager;
        auto container = make_rcp<FocusNode>(&containerFocusable);
        auto child = make_rcp<FocusNode>(&childFocusable);
        auto sibling = make_rcp<FocusNode>(&siblingFocusable);
        container->canFocus(false);
        // The container's bounds enclose the child's, so it sits closer to
        // the focused sibling and would win the scoring outright if it were
        // ever collected as a candidate.
        container->worldBounds(AABB(0, 0, 50, 50));
        child->worldBounds(AABB(0, 0, 10, 10));
        sibling->worldBounds(AABB(100, 0, 110, 10));
        manager.addChild(nullptr, container);
        manager.addChild(container, child);
        manager.addChild(nullptr, sibling);

        manager.setFocus(sibling);
        CHECK(manager.focusLeft());
        CHECK(manager.primaryFocus() == child);
    }

    SECTION("backward traversal agrees")
    {
        FocusManager manager;
        auto container = make_rcp<FocusNode>(&containerFocusable);
        auto child = make_rcp<FocusNode>(&childFocusable);
        auto before = make_rcp<FocusNode>(&siblingFocusable);
        container->canFocus(false);
        manager.addChild(nullptr, before);
        manager.addChild(nullptr, container);
        manager.addChild(container, child);

        manager.setFocus(child);
        manager.focusPrevious();
        // Straight past the container to the node before it, rather than
        // stopping on it or refusing to leave its subtree.
        CHECK(manager.primaryFocus() == before);
    }

    SECTION("two non-stop levels deep")
    {
        FocusManager manager;
        MockFocusable innerFocusable;
        auto outer = make_rcp<FocusNode>(&containerFocusable);
        auto inner = make_rcp<FocusNode>(&innerFocusable);
        auto child = make_rcp<FocusNode>(&childFocusable);
        outer->canFocus(false);
        inner->canTraverse(false);
        manager.addChild(nullptr, outer);
        manager.addChild(outer, inner);
        manager.addChild(inner, child);

        manager.focusNext();
        CHECK(manager.primaryFocus() == child);
    }
}

TEST_CASE("FocusManager flag matrix decides focus and navigation separately",
          "[FocusManager]")
{
    // canFocus and canTraverse answer two different questions: "may this node
    // hold focus at all" and "does navigation visit it". Every combination,
    // against every route in.
    MockFocusable beforeFocusable, subjectFocusable, afterFocusable;

    bool canFocus = false;
    bool canTraverse = false;
    SECTION("canFocus, canTraverse")
    {
        canFocus = true;
        canTraverse = true;
    }
    SECTION("canFocus, !canTraverse")
    {
        canFocus = true;
        canTraverse = false;
    }
    SECTION("!canFocus, canTraverse")
    {
        canFocus = false;
        canTraverse = true;
    }
    SECTION("!canFocus, !canTraverse")
    {
        canFocus = false;
        canTraverse = false;
    }

    FocusManager manager;
    auto before = make_rcp<FocusNode>(&beforeFocusable);
    auto subject = make_rcp<FocusNode>(&subjectFocusable);
    auto after = make_rcp<FocusNode>(&afterFocusable);
    subject->canFocus(canFocus);
    subject->canTraverse(canTraverse);
    // The subject sits between the other two on the x axis, so it is what
    // every arrow-key step from `after` would reach first.
    before->worldBounds(AABB(0, 0, 10, 10));
    subject->worldBounds(AABB(50, 0, 60, 10));
    after->worldBounds(AABB(100, 0, 110, 10));
    manager.addChild(nullptr, before);
    manager.addChild(nullptr, subject);
    manager.addChild(nullptr, after);

    // Only canFocus decides whether focus may land when something names it.
    manager.setFocus(subject);
    CHECK(manager.hasPrimaryFocus(subject) == canFocus);
    manager.clearFocus();

    // Navigation needs both, in both directions and on the arrows.
    const bool navigable = canFocus && canTraverse;

    manager.setFocus(before);
    manager.focusNext();
    CHECK(manager.hasPrimaryFocus(subject) == navigable);

    manager.setFocus(after);
    manager.focusPrevious();
    CHECK(manager.hasPrimaryFocus(subject) == navigable);

    manager.setFocus(after);
    manager.focusLeft();
    CHECK(manager.hasPrimaryFocus(subject) == navigable);

    // And a node Tab skips still keeps focus it was handed directly — being
    // out of navigation is not the same as being unable to hold focus.
    if (canFocus)
    {
        manager.setFocus(subject);
        REQUIRE(manager.hasPrimaryFocus(subject));
        manager.dropFocusIfFocusTargetHidden();
        CHECK(manager.hasPrimaryFocus(subject));
    }
}

TEST_CASE("FocusManager follows the flags when they change at runtime",
          "[FocusManager]")
{
    // canFocus and canTraverse are animatable and data-bindable, so both can
    // flip under a node that already holds focus.
    MockFocusable subjectFocusable, otherFocusable;

    SECTION("canFocus going false re-homes focus away")
    {
        FocusManager manager;
        auto subject = make_rcp<FocusNode>(&subjectFocusable);
        auto other = make_rcp<FocusNode>(&otherFocusable);
        auto parent = make_rcp<FocusNode>();
        manager.addChild(nullptr, parent);
        manager.addChild(parent, subject);
        manager.addChild(parent, other);

        manager.setFocus(subject);
        REQUIRE(manager.primaryFocus() == subject);

        subject->canFocus(false);
        manager.dropFocusIfFocusTargetHidden();
        CHECK(manager.primaryFocus() == other);
        CHECK(subjectFocusable.blurredCount == 1);
    }

    SECTION("canTraverse going false leaves focus where it is")
    {
        FocusManager manager;
        auto subject = make_rcp<FocusNode>(&subjectFocusable);
        auto other = make_rcp<FocusNode>(&otherFocusable);
        manager.addChild(nullptr, subject);
        manager.addChild(nullptr, other);

        manager.setFocus(subject);
        REQUIRE(manager.primaryFocus() == subject);

        subject->canTraverse(false);
        manager.dropFocusIfFocusTargetHidden();
        CHECK(manager.primaryFocus() == subject);
        CHECK(subjectFocusable.blurredCount == 0);

        // But Tab no longer comes back to it.
        manager.setFocus(other);
        manager.focusPrevious();
        CHECK(manager.primaryFocus() != subject);
    }

    SECTION("a flag flip changes the order without a rebuild")
    {
        FocusManager manager;
        auto first = make_rcp<FocusNode>(&subjectFocusable);
        auto second = make_rcp<FocusNode>(&otherFocusable);
        manager.addChild(nullptr, first);
        manager.addChild(nullptr, second);

        manager.focusNext();
        CHECK(manager.primaryFocus() == first);

        manager.clearFocus();
        first->canTraverse(false);
        manager.focusNext();
        CHECK(manager.primaryFocus() == second);

        manager.clearFocus();
        first->canTraverse(true);
        first->canFocus(false);
        manager.focusNext();
        CHECK(manager.primaryFocus() == second);
    }
}

TEST_CASE("FocusManager authored focus flags behave like the node flags",
          "[FocusManager]")
{
    // The bare-FocusNode tests above set the flags directly; authored files
    // set them as bits of FocusData::focusFlags. Same rules, and in
    // particular an authored canFocus="false" container — the documented way
    // to host keyboard listeners — keeps its children in the order.
    FocusManager manager;
    FocusData container;
    FocusData child;
    container.focusFlags(container.focusFlags() & ~FocusData::canFocusBitmask);
    manager.addChild(nullptr, container.focusNode());
    manager.addChild(container.focusNode(), child.focusNode());

    REQUIRE(container.focusNode()->canFocus() == false);
    CHECK(manager.getTraversableNodes(nullptr).size() == 1);

    manager.setFocus(container.focusNode());
    CHECK(manager.primaryFocus() == nullptr);

    manager.focusNext();
    CHECK(manager.primaryFocus() == child.focusNode());
}

TEST_CASE("FocusManager a queued focus request never lands on a "
          "non-focusable target",
          "[FocusManager]")
{
    // The FocusActionTarget route: requests that can't take are retried on
    // every drain rather than applied once, so a canFocus=false target has to
    // be refused at each one.
    FocusManager manager;
    MockFocusable targetFocusable;
    auto target = make_rcp<FocusNode>(&targetFocusable);
    target->canFocus(false);
    manager.addChild(nullptr, target);

    manager.requestFocus(target, nullptr);
    CHECK(manager.primaryFocus() == nullptr);

    manager.processPendingFocusRequests(nullptr);
    CHECK(manager.primaryFocus() == nullptr);

    manager.finishPendingFocusRequests(nullptr);
    CHECK(manager.primaryFocus() == nullptr);
    CHECK(targetFocusable.focusedCount == 0);
}

TEST_CASE("FocusManager edgeBehavior on a childless node is inert",
          "[FocusManager]")
{
    FocusManager manager;
    auto node1 = make_rcp<FocusNode>();
    auto node2 = make_rcp<FocusNode>();
    // Authored edge behavior on something that is not a scope has never
    // applied. If it did, closedLoop here would wrap node1 onto itself and
    // focus could never leave it.
    node1->edgeBehavior(EdgeBehavior::closedLoop);
    node2->edgeBehavior(EdgeBehavior::stop);

    manager.addChild(nullptr, node1);
    manager.addChild(nullptr, node2);

    manager.setFocus(node1);
    manager.focusNext();
    CHECK(manager.primaryFocus() == node2);
}

TEST_CASE("FocusManager closedLoop with a single stop does not move focus",
          "[FocusManager]")
{
    FocusManager manager;
    auto scope = make_rcp<FocusNode>();
    auto only = make_rcp<FocusNode>();
    scope->edgeBehavior(EdgeBehavior::closedLoop);
    scope->canFocus(false);

    manager.addChild(nullptr, scope);
    manager.addChild(scope, only);

    manager.setFocus(only);
    CHECK(manager.focusNext() == false);
    CHECK(manager.primaryFocus() == only);
    CHECK(manager.focusPrevious() == false);
    CHECK(manager.primaryFocus() == only);
}

TEST_CASE("FocusManager traversal re-enters after the focused node is detached",
          "[FocusManager]")
{
    FocusManager manager;
    auto node1 = make_rcp<FocusNode>();
    auto node2 = make_rcp<FocusNode>();

    manager.addChild(nullptr, node1);
    manager.addChild(nullptr, node2);

    manager.setFocus(node2);
    // detachChild deliberately keeps focus, leaving the target off both its
    // parent and the root list. The walk has no chain to climb, so it
    // re-enters the list it was handed.
    manager.detachChild(node2);
    REQUIRE(manager.primaryFocus() == node2);

    manager.focusNext();
    CHECK(manager.primaryFocus() == node1);
}

TEST_CASE("FocusManager traversal reports nothing when the tree holds no stop",
          "[FocusManager]")
{
    // A tree of nodes none of which can be focused: both directions have to
    // come back empty rather than land on one anyway.
    FocusManager manager;
    auto parent = make_rcp<FocusNode>();
    auto child = make_rcp<FocusNode>();
    parent->canFocus(false);
    child->canFocus(false);
    manager.addChild(nullptr, parent);
    manager.addChild(parent, child);

    CHECK(manager.focusNext() == false);
    CHECK(manager.primaryFocus() == nullptr);
    CHECK(manager.focusPrevious() == false);
    CHECK(manager.primaryFocus() == nullptr);
    CHECK(manager.getTraversableNodes(nullptr).empty());
}

TEST_CASE("FocusManager traversal does not descend into a detached subtree",
          "[FocusManager]")
{
    // detachChild takes a whole subtree out of the manager (it clears
    // m_manager on every descendant) but leaves the node's children hanging
    // off it, and deliberately keeps focus where it is. Traversal must not
    // walk into that subtree: those nodes are no longer the manager's to hand
    // focus to, and the caller is holding them to re-add or destroy.
    FocusManager manager;
    auto stay = make_rcp<FocusNode>();
    auto detached = make_rcp<FocusNode>();
    auto detachedChild = make_rcp<FocusNode>();

    manager.addChild(nullptr, stay);
    manager.addChild(nullptr, detached);
    manager.addChild(detached, detachedChild);

    manager.setFocus(detached);
    manager.detachChild(detached);
    REQUIRE(manager.primaryFocus() == detached);
    REQUIRE(detached->manager() == nullptr);
    REQUIRE(detachedChild->manager() == nullptr);
    // The subtree is still wired together, which is what makes this reachable.
    REQUIRE(detached->children().size() == 1);

    manager.focusNext();
    CHECK(manager.primaryFocus() == stay);

    manager.clearFocus();
    manager.setFocus(detached);
    manager.focusPrevious();
    CHECK(manager.primaryFocus() == stay);
}

TEST_CASE("FocusManager getTraversableNodes lists a container of stops",
          "[FocusManager]")
{
    FocusManager manager;
    MockFocusable containerFocusable, childFocusable;
    auto container = make_rcp<FocusNode>(&containerFocusable);
    auto child = make_rcp<FocusNode>(&childFocusable);
    container->canFocus(false);
    manager.addChild(nullptr, container);

    // Nothing reachable under it yet, so it contributes no stop.
    CHECK(manager.getTraversableNodes(nullptr).empty());

    manager.addChild(container, child);
    auto roots = manager.getTraversableNodes(nullptr);
    REQUIRE(roots.size() == 1);
    CHECK(roots[0] == container.get());
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

TEST_CASE("FocusManager traversal visits a focusable scope before its children",
          "[FocusManager]")
{
    FocusManager manager;
    MockFocusable scopeFocusable, leaf1Focusable, leaf2Focusable;
    auto scope = make_rcp<FocusNode>(&scopeFocusable);
    auto leaf1 = make_rcp<FocusNode>(&leaf1Focusable);
    auto leaf2 = make_rcp<FocusNode>(&leaf2Focusable);

    manager.addChild(nullptr, scope);
    manager.addChild(scope, leaf1);
    manager.addChild(scope, leaf2);

    // Pre-order: having children doesn't take the scope out of the order.
    manager.focusNext();
    CHECK(manager.primaryFocus() == scope);

    manager.focusNext();
    CHECK(manager.primaryFocus() == leaf1);
    CHECK(manager.hasPrimaryFocus(scope) == false);
    CHECK(scope->hasFocus() == true); // But scope has descendant focus

    manager.focusNext();
    CHECK(manager.primaryFocus() == leaf2);
}

TEST_CASE("FocusManager traversal skips a scope that can't be focused",
          "[FocusManager]")
{
    FocusManager manager;
    MockFocusable scopeFocusable, leaf1Focusable, leaf2Focusable;
    auto scope = make_rcp<FocusNode>(&scopeFocusable);
    auto leaf1 = make_rcp<FocusNode>(&leaf1Focusable);
    auto leaf2 = make_rcp<FocusNode>(&leaf2Focusable);
    scope->canFocus(false);

    manager.addChild(nullptr, scope);
    manager.addChild(scope, leaf1);
    manager.addChild(scope, leaf2);

    // A backed canFocus=false parent used to prune its whole subtree from
    // traversal. It now only takes itself out of the order.
    manager.focusNext();
    CHECK(manager.primaryFocus() == leaf1);
    manager.focusNext();
    CHECK(manager.primaryFocus() == leaf2);
}

TEST_CASE("FocusManager walks nested scopes in pre-order", "[FocusManager]")
{
    FocusManager manager;
    auto scope1 = make_rcp<FocusNode>();
    auto scope2 = make_rcp<FocusNode>();
    auto leaf = make_rcp<FocusNode>();

    manager.addChild(nullptr, scope1);
    manager.addChild(scope1, scope2);
    manager.addChild(scope2, leaf);

    // Outermost first, then down one level per step.
    manager.focusNext();
    CHECK(manager.primaryFocus() == scope1);
    manager.focusNext();
    CHECK(manager.primaryFocus() == scope2);
    manager.focusNext();
    CHECK(manager.primaryFocus() == leaf);
    CHECK(scope1->hasFocus() == true);
    CHECK(scope2->hasFocus() == true);

    // And back out the same way.
    manager.focusPrevious();
    CHECK(manager.primaryFocus() == scope2);
    manager.focusPrevious();
    CHECK(manager.primaryFocus() == scope1);
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

TEST_CASE("Freeing a FocusNode clears the parent pointer of a child that "
          "outlives it",
          "[FocusManager]")
{
    FocusManager manager;
    auto scope = make_rcp<FocusNode>(); // persistent host scope, held here
    {
        auto row = make_rcp<FocusNode>(); // transient list row
        manager.addChild(nullptr, row);
        manager.addChild(row, scope);
        CHECK(scope->parent() == row.get());
        // The list re-sync removes the row from the manager, then drops it.
        manager.removeChild(row);
    } // row FocusNode destroyed here; scope survives via the outer rcp

    REQUIRE(scope->parent() == nullptr);

    // Re-homing the survivor is now safe — no dereference of the freed row.
    auto newParent = make_rcp<FocusNode>();
    manager.addChild(nullptr, newParent);
    manager.addChild(newParent, scope);
    CHECK(scope->parent() == newParent.get());
    CHECK(newParent->children().size() == 1);
}

TEST_CASE("FocusManager::addChild removes a migrating root from its previous "
          "manager",
          "[FocusManager]")
{
    FocusManager internalManager;
    FocusManager parentManager;
    auto scope = make_rcp<FocusNode>();

    internalManager.addChild(nullptr, scope);
    CHECK(scope->manager() == &internalManager);
    CHECK(internalManager.rootNodes().size() == 1);

    // Migrate the scope to the parent manager (no FocusNode parent -> root).
    parentManager.addChild(nullptr, scope);
    CHECK(scope->manager() == &parentManager);
    CHECK(parentManager.rootNodes().size() == 1);

    // The internal manager must no longer reference the migrated scope.
    CHECK(internalManager.rootNodes().empty());
}

TEST_CASE("A migrated focus scope survives destruction of its previous manager",
          "[FocusManager]")
{
    FocusManager parentManager;
    auto scope = make_rcp<FocusNode>();
    {
        FocusManager internalManager;
        internalManager.addChild(nullptr, scope);
        parentManager.addChild(nullptr, scope); // migrate to parent
        CHECK(scope->manager() == &parentManager);
    } // internalManager destroyed here

    // The scope still belongs to parentManager, not the destroyed one.
    CHECK(scope->manager() == &parentManager);

    if (scope->manager() != nullptr)
    {
        scope->manager()->removeChild(scope);
    }
    CHECK(parentManager.rootNodes().empty());
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

    // Reverse pre-order: the scope precedes its own children, so it is the
    // predecessor of `inner` and the walk only leaves the subtree after it.
    manager.focusPrevious();
    CHECK(manager.primaryFocus() == scope);
    manager.focusPrevious();
    CHECK(manager.primaryFocus() == before);
}

TEST_CASE("FocusManager backward exits a scope that can't be focused",
          "[FocusManager]")
{
    FocusManager manager;
    auto root = make_rcp<FocusNode>();
    auto before = make_rcp<FocusNode>();
    auto scope = make_rcp<FocusNode>();
    auto inner = make_rcp<FocusNode>();

    scope->edgeBehavior(EdgeBehavior::parentScope);
    scope->canFocus(false);

    manager.addChild(nullptr, root);
    manager.addChild(root, before);
    manager.addChild(root, scope);
    manager.addChild(scope, inner);

    manager.setFocus(inner);

    // Nothing to stop on at the scope, so the walk leaves its subtree.
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

    // The scope precedes node1 in pre-order, so backward reaches it before
    // the loop has anything to wrap.
    CHECK(manager.primaryFocus() == scope);

    // Now the walk would leave the subtree, and closedLoop sends it to the
    // other end instead.
    manager.focusPrevious();
    CHECK(manager.primaryFocus() == node2);
}

TEST_CASE("FocusManager closedLoop wraps backward past a scope that can't be "
          "focused",
          "[FocusManager]")
{
    FocusManager manager;
    auto scope = make_rcp<FocusNode>();
    auto node1 = make_rcp<FocusNode>();
    auto node2 = make_rcp<FocusNode>();

    scope->edgeBehavior(EdgeBehavior::closedLoop);
    scope->canFocus(false);

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

    // The scope is still inside its own subtree, so `stop` has no say yet.
    CHECK(manager.primaryFocus() == scope);

    // From the scope the walk would leave, and `stop` holds it there.
    manager.focusPrevious();
    CHECK(manager.primaryFocus() == scope);
}

TEST_CASE("FocusManager stop holds a scope that can't be focused",
          "[FocusManager]")
{
    FocusManager manager;
    auto scope = make_rcp<FocusNode>();
    auto node1 = make_rcp<FocusNode>();
    auto node2 = make_rcp<FocusNode>();

    scope->edgeBehavior(EdgeBehavior::stop);
    scope->canFocus(false);

    manager.addChild(nullptr, scope);
    manager.addChild(scope, node1);
    manager.addChild(scope, node2);

    manager.setFocus(node1);
    manager.focusPrevious();

    // Should stay on node1
    CHECK(manager.primaryFocus() == node1);
}

TEST_CASE("StateMachineInstance hasFocusNodes ignores non-traversable scopes",
          "[FocusManager]")
{
    NoOpFactory factory;
    Artboard artboard(&factory);
    auto instance = instanceWithFocus(artboard);
    StateMachine machine;
    StateMachineInstance smi(&machine, instance.get());

    auto scope = make_rcp<FocusNode>();
    scope->canFocus(false);
    scope->canTraverse(false);
    smi.focusManager()->addChild(nullptr, scope);
    CHECK(smi.hasFocusNodes() == false);
}

TEST_CASE("StateMachineInstance hasFocusNodes sees leaves under a "
          "transparent scope",
          "[FocusManager]")
{
    NoOpFactory factory;
    Artboard artboard(&factory);
    auto instance = instanceWithFocus(artboard);
    StateMachine machine;
    StateMachineInstance smi(&machine, instance.get());

    // Transparent structural scope as registered for a data-bound nested
    // artboard host: unbacked (no focusable), canFocus/canTraverse/canTouch
    // false. Traversal descends through it because it has no focusable.
    auto scope = make_rcp<FocusNode>();
    scope->canFocus(false);
    scope->canTraverse(false);
    scope->canTouch(false);
    smi.focusManager()->addChild(nullptr, scope);

    // Empty scope contributes no focus targets (e.g. a bindable artboard with
    // no focus nodes).
    CHECK(smi.hasFocusNodes() == false);

    // Swapping in an artboard that has a focusable leaf must make the state
    // machine report focus nodes, even though the leaf lives under the scope.
    auto leaf = make_rcp<FocusNode>();
    smi.focusManager()->addChild(scope, leaf);
    CHECK(smi.hasFocusNodes() == true);
}

TEST_CASE("StateMachineInstance hasFocusNodes counts focus data that is "
          "currently ineligible for traversal",
          "[FocusManager]")
{
    NoOpFactory factory;
    Artboard artboard(&factory);
    auto instance = instanceWithFocus(artboard);
    StateMachine machine;
    StateMachineInstance smi(&machine, instance.get());

    // hasFocusNodes gates one-time setup in high-level runtimes (attaching
    // tab/shift+tab listeners in JS), so authored focus data must count even
    // while it can't currently be focused: canFocus/canTraverse are
    // data-bindable and collapse/visibility can change on any frame.
    FocusData focusData;
    // canFocus/canTraverse are now bits in the focusFlags bitmask; clear both
    // (leave the rest) to make the node ineligible for traversal.
    focusData.focusFlags(
        focusData.focusFlags() &
        ~(FocusData::canFocusBitmask | FocusData::canTraverseBitmask));
    smi.focusManager()->addChild(nullptr, focusData.focusNode());
    CHECK(smi.hasFocusNodes() == true);
}

TEST_CASE("FocusManager traversal descends through a transparent scope "
          "and keeps sibling order",
          "[FocusManager]")
{
    FocusManager manager;
    auto leafA = make_rcp<FocusNode>();
    auto scope = make_rcp<FocusNode>();
    auto leafC = make_rcp<FocusNode>();

    // scope mirrors a data-bound nested artboard host slot sitting between two
    // sibling focus nodes: unbacked (no focusable) and not a focus target
    // itself, but Tab descends through it to whatever artboard is swapped in.
    scope->canFocus(false);
    scope->canTraverse(false);
    scope->canTouch(false);

    manager.addChild(nullptr, leafA);
    manager.addChild(nullptr, scope);
    manager.addChild(nullptr, leafC);

    // Empty scope is skipped: A -> C.
    manager.focusNext();
    CHECK(manager.primaryFocus() == leafA);
    manager.focusNext();
    CHECK(manager.primaryFocus() == leafC);

    // Populate the scope (artboard swapped in). Its leaf occupies the scope's
    // sibling slot, so traversal order becomes A -> B -> C.
    manager.clearFocus();
    auto leafB = make_rcp<FocusNode>();
    manager.addChild(scope, leafB);

    manager.focusNext();
    CHECK(manager.primaryFocus() == leafA);
    manager.focusNext();
    CHECK(manager.primaryFocus() == leafB);
    manager.focusNext();
    CHECK(manager.primaryFocus() == leafC);
}

TEST_CASE("FocusManager drops focus when a leaf under a transparent scope "
          "becomes hidden",
          "[FocusManager]")
{
    FocusManager manager;
    // Unbacked scope: the shape of a data-bound nested artboard's scope node.
    auto scope = make_rcp<FocusNode>();
    scope->canFocus(false);
    scope->canTraverse(false);
    scope->canTouch(false);
    // A focusable leaf inside it, like a swapped-in nested artboard's element.
    MockFocusable leafFocusable;
    auto leaf = make_rcp<FocusNode>(&leafFocusable);
    manager.addChild(nullptr, scope);
    manager.addChild(scope, leaf);

    // Tab descends through the scope onto the nested leaf.
    manager.focusNext();
    REQUIRE(manager.primaryFocus() == leaf);

    // Hide the nested content (its focusable reports ineligible). Focus must be
    // dropped, not left stranded behind the scope.
    leafFocusable.eligible = false;
    manager.dropFocusIfFocusTargetHidden();
    CHECK(manager.primaryFocus() == nullptr);
}

TEST_CASE("FocusManager rebuilding one scope's subtree preserves focus in a "
          "sibling scope",
          "[FocusManager]")
{
    FocusManager manager;
    // Two sibling transparent scopes, like two data-bound nested artboard
    // hosts.
    auto scopeA = make_rcp<FocusNode>();
    scopeA->canFocus(false);
    scopeA->canTraverse(false);
    scopeA->canTouch(false);
    auto scopeB = make_rcp<FocusNode>();
    scopeB->canFocus(false);
    scopeB->canTraverse(false);
    scopeB->canTouch(false);

    MockFocusable leafAFocusable, leafBFocusable;
    auto leafA = make_rcp<FocusNode>(&leafAFocusable);
    auto leafB = make_rcp<FocusNode>(&leafBFocusable);
    manager.addChild(nullptr, scopeA);
    manager.addChild(scopeA, leafA);
    manager.addChild(nullptr, scopeB);
    manager.addChild(scopeB, leafB);

    // Focus the leaf inside scope A.
    manager.setFocus(leafA);
    REQUIRE(manager.primaryFocus() == leafA);

    // Simulate swapping the artboard in sibling scope B: tear down B's current
    // content and rebuild it with a new focusable leaf under the same scope.
    // Focus held in the unrelated scope A must be untouched.
    manager.removeChild(leafB);
    MockFocusable leafB2Focusable;
    auto leafB2 = make_rcp<FocusNode>(&leafB2Focusable);
    manager.addChild(scopeB, leafB2);

    CHECK(manager.primaryFocus() == leafA);
}

TEST_CASE("FocusActionTraversal perform advances focus with traversalKind next",
          "[FocusActionTraversal]")
{
    NoOpFactory factory;
    Artboard artboard(&factory);
    auto instance = instanceWithFocus(artboard);
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
    auto instance = instanceWithFocus(artboard);
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
    auto instance = instanceWithFocus(artboard);
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
    auto instance = instanceWithFocus(artboard);
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
    auto instance = instanceWithFocus(artboard);
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
    auto instance = instanceWithFocus(artboard);
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
    auto instance = instanceWithFocus(artboard);
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
    auto instance = instanceWithFocus(artboard);
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
    auto instance = instanceWithFocus(artboard);
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
    auto instance = instanceWithFocus(artboard);
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

TEST_CASE("StateMachineInstance directional focus moves by position",
          "[FocusManager]")
{
    NoOpFactory factory;
    Artboard artboard(&factory);
    auto instance = instanceWithFocus(artboard);
    StateMachine machine;
    StateMachineInstance smi(&machine, instance.get());
    auto* manager = smi.focusManager();

    // A plus shape: one node on each side of the center.
    MockFocusable centerF, leftF, rightF, upF, downF;
    auto center = make_rcp<FocusNode>(&centerF);
    auto left = make_rcp<FocusNode>(&leftF);
    auto right = make_rcp<FocusNode>(&rightF);
    auto up = make_rcp<FocusNode>(&upF);
    auto down = make_rcp<FocusNode>(&downF);
    center->worldBounds(AABB(100, 100, 110, 110));
    left->worldBounds(AABB(0, 100, 10, 110));
    right->worldBounds(AABB(200, 100, 210, 110));
    up->worldBounds(AABB(100, 0, 110, 10));
    down->worldBounds(AABB(100, 200, 110, 210));
    for (auto& node : {center, left, right, up, down})
    {
        manager->addChild(nullptr, node);
    }

    SECTION("nothing focused")
    {
        CHECK(smi.focusLeft() == false);
        CHECK(smi.focusRight() == false);
        CHECK(smi.focusUp() == false);
        CHECK(smi.focusDown() == false);
        CHECK(manager->primaryFocus() == nullptr);
    }

    SECTION("each direction from the center")
    {
        manager->setFocus(center);
        CHECK(smi.focusLeft() == true);
        CHECK(manager->primaryFocus() == left);

        manager->setFocus(center);
        CHECK(smi.focusRight() == true);
        CHECK(manager->primaryFocus() == right);

        manager->setFocus(center);
        CHECK(smi.focusUp() == true);
        CHECK(manager->primaryFocus() == up);

        manager->setFocus(center);
        CHECK(smi.focusDown() == true);
        CHECK(manager->primaryFocus() == down);
    }

    SECTION("an edge keeps focus")
    {
        manager->setFocus(left);
        CHECK(smi.focusLeft() == false);
        CHECK(manager->primaryFocus() == left);
    }
}

TEST_CASE("StateMachineInstance directional focus uses the external focus "
          "manager when set",
          "[FocusManager]")
{
    NoOpFactory factory;
    Artboard artboard(&factory);
    auto instance = instanceWithFocus(artboard);
    StateMachine machine;
    StateMachineInstance smi(&machine, instance.get());

    FocusManager external;
    MockFocusable aF, bF;
    auto a = make_rcp<FocusNode>(&aF);
    auto b = make_rcp<FocusNode>(&bF);
    a->worldBounds(AABB(0, 0, 10, 10));
    b->worldBounds(AABB(100, 0, 110, 10));
    external.addChild(nullptr, a);
    external.addChild(nullptr, b);
    external.setFocus(a);

    smi.setExternalFocusManager(&external);

    CHECK(smi.focusRight() == true);
    CHECK(external.primaryFocus() == b);
    CHECK(smi.focusLeft() == true);
    CHECK(external.primaryFocus() == a);
}

TEST_CASE(
    "StateMachineInstance directional focus without a focus manager returns false",
    "[FocusManager]")
{
    NoOpFactory factory;
    Artboard artboard(&factory);
    auto instance = artboard.instance();
    StateMachine machine;
    StateMachineInstance smi(&machine, instance.get());

    REQUIRE(smi.focusManager() == nullptr);
    CHECK(smi.focusLeft() == false);
    CHECK(smi.focusRight() == false);
    CHECK(smi.focusUp() == false);
    CHECK(smi.focusDown() == false);
}

TEST_CASE("StateMachineInstance::clearFocus clears internal focus manager",
          "[FocusState]")
{
    NoOpFactory factory;
    Artboard artboard(&factory);
    auto instance = instanceWithFocus(artboard);
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

namespace
{
// Records what it was offered and answers a fixed claim, so a FocusData with
// two of these shows whether the first one's claim cut the second one off.
class RecordingKeyboardListener : public KeyboardListener
{
public:
    RecordingKeyboardListener(bool claims) : m_claims(claims) {}

    bool keyInput(Key, KeyModifiers, bool, bool) override
    {
        calls++;
        return m_claims;
    }
    bool textInput(const std::string&) override { return false; }

    int calls = 0;

private:
    bool m_claims;
};
} // namespace

// A claim stops the key travelling UP, not sideways. Two listeners on one
// element -- a component's own and one an author added beside it -- both
// asked for the key, and both still get it; only the report to the focus
// manager is shared. This is the split the DOM draws between
// stopPropagation and stopImmediatePropagation, and authored files predate
// any listener being able to claim at all, so peers must keep firing.
TEST_CASE("a claiming keyboard listener does not cut off its peers",
          "[FocusData]")
{
    FocusData focusData;
    RecordingKeyboardListener first(/*claims=*/true);
    RecordingKeyboardListener second(/*claims=*/false);
    focusData.addKeyboardListener(&first);
    focusData.addKeyboardListener(&second);

    CHECK(focusData.keyInput(Key::a, KeyModifiers::none, true, false));
    CHECK(first.calls == 1);
    CHECK(second.calls == 1);
}

TEST_CASE("a focus data with no claiming listener reports unhandled",
          "[FocusData]")
{
    FocusData focusData;
    RecordingKeyboardListener first(/*claims=*/false);
    RecordingKeyboardListener second(/*claims=*/false);
    focusData.addKeyboardListener(&first);
    focusData.addKeyboardListener(&second);

    CHECK(!focusData.keyInput(Key::a, KeyModifiers::none, true, false));
    CHECK(first.calls == 1);
    CHECK(second.calls == 1);
}

TEST_CASE("StateMachineInstance::keyInput and textInput route to the focused "
          "element",
          "[FocusManager]")
{
    NoOpFactory factory;
    Artboard artboard(&factory);
    auto instance = instanceWithFocus(artboard);
    StateMachine machine;
    StateMachineInstance smi(&machine, instance.get());

    MockFocusable focusable;
    focusable.returnValue = true;
    auto node = make_rcp<FocusNode>(&focusable);

    // Nothing focused yet, so there is nobody to route the events to.
    CHECK(smi.keyInput(Key::a, KeyModifiers::none, true, false) == false);
    CHECK(smi.textInput("hello") == false);
    CHECK(focusable.keyInputCount == 0);
    CHECK(focusable.textInputCount == 0);

    smi.focusManager()->addChild(nullptr, node);
    smi.focusManager()->setFocus(node);

    CHECK(smi.keyInput(Key::b, KeyModifiers::shift, true, false) == true);
    CHECK(focusable.keyInputCount == 1);
    CHECK(focusable.lastKey == Key::b);

    CHECK(smi.textInput("world") == true);
    CHECK(focusable.textInputCount == 1);
    CHECK(focusable.lastText == "world");
}

TEST_CASE("StateMachineInstance::keyInput and textInput report unhandled "
          "events from the focused element",
          "[FocusManager]")
{
    NoOpFactory factory;
    Artboard artboard(&factory);
    auto instance = instanceWithFocus(artboard);
    StateMachine machine;
    StateMachineInstance smi(&machine, instance.get());

    MockFocusable focusable;
    focusable.returnValue = false;
    auto node = make_rcp<FocusNode>(&focusable);
    smi.focusManager()->addChild(nullptr, node);
    smi.focusManager()->setFocus(node);

    // The focused element saw both events but declined to handle them.
    CHECK(smi.keyInput(Key::escape, KeyModifiers::none, true, false) == false);
    CHECK(smi.textInput("ignored") == false);
    CHECK(focusable.keyInputCount == 1);
    CHECK(focusable.textInputCount == 1);
}

TEST_CASE("StateMachineInstance::keyInput and textInput use the external focus "
          "manager when set",
          "[FocusManager]")
{
    NoOpFactory factory;
    Artboard artboard(&factory);
    auto instance = instanceWithFocus(artboard);
    StateMachine machine;
    StateMachineInstance smi(&machine, instance.get());

    // The artboard owns the manager the state machine uses by default.
    auto* ownManager = instance->ensureFocusManager();
    MockFocusable internalFocusable;
    internalFocusable.returnValue = true;
    auto internalNode = make_rcp<FocusNode>(&internalFocusable);
    ownManager->addChild(nullptr, internalNode);
    ownManager->setFocus(internalNode);

    FocusManager external;
    MockFocusable externalFocusable;
    externalFocusable.returnValue = true;
    auto externalNode = make_rcp<FocusNode>(&externalFocusable);
    external.addChild(nullptr, externalNode);
    external.setFocus(externalNode);

    smi.setExternalFocusManager(&external);

    CHECK(smi.keyInput(Key::c, KeyModifiers::none, true, false) == true);
    CHECK(smi.textInput("external") == true);

    // Events land on the external tree, not the internal one.
    CHECK(externalFocusable.keyInputCount == 1);
    CHECK(externalFocusable.textInputCount == 1);
    CHECK(externalFocusable.lastText == "external");
    CHECK(internalFocusable.keyInputCount == 0);
    CHECK(internalFocusable.textInputCount == 0);

    smi.setExternalFocusManager(nullptr);
}

TEST_CASE("FocusManager setFocus on a scope focuses the scope",
          "[FocusManager]")
{
    FocusManager manager;
    auto scope = make_rcp<FocusNode>();
    auto leaf1 = make_rcp<FocusNode>();
    auto leaf2 = make_rcp<FocusNode>();

    manager.addChild(nullptr, scope);
    manager.addChild(scope, leaf1);
    manager.addChild(scope, leaf2);

    // Focus lands where it was asked to. Having children no longer redirects
    // it to a descendant the caller never named.
    manager.setFocus(scope);
    CHECK(manager.primaryFocus() == scope);
}

TEST_CASE("FocusManager setFocus never redirects to a descendant",
          "[FocusManager]")
{
    FocusManager manager;
    auto scope = make_rcp<FocusNode>();
    auto row = make_rcp<FocusNode>();
    auto leaf = make_rcp<FocusNode>();
    auto sibling = make_rcp<FocusNode>();

    manager.addChild(nullptr, scope);
    manager.addChild(scope, row);
    manager.addChild(row, leaf);
    manager.addChild(scope, sibling);

    manager.setFocus(scope);
    CHECK(manager.primaryFocus() == scope);

    // Even a scope whose only children are untraversable keeps the focus it
    // was handed, rather than falling back to anything.
    manager.clearFocus();
    row->canTraverse(false);
    leaf->canTraverse(false);
    sibling->canTraverse(false);
    manager.setFocus(scope);
    CHECK(manager.primaryFocus() == scope);
}

TEST_CASE("FocusManager setFocus honours canFocus on a node Tab would skip",
          "[FocusManager]")
{
    FocusManager manager;
    auto traversable = make_rcp<FocusNode>();
    auto reachableOnlyByName = make_rcp<FocusNode>();
    // Out of navigation, but still focusable: a FocusActionTarget or a script
    // naming it directly must still land.
    reachableOnlyByName->canTraverse(false);

    manager.addChild(nullptr, traversable);
    manager.addChild(nullptr, reachableOnlyByName);

    manager.setFocus(reachableOnlyByName);
    CHECK(manager.primaryFocus() == reachableOnlyByName);

    // And it survives the frame's re-home pass, which asks whether the target
    // can hold focus, not whether Tab would have picked it.
    manager.dropFocusIfFocusTargetHidden();
    CHECK(manager.primaryFocus() == reachableOnlyByName);

    // Tab still skips it entirely: it is the last root, so stepping past the
    // traversable one runs off the end of the root list and clears.
    manager.clearFocus();
    manager.focusNext();
    CHECK(manager.primaryFocus() == traversable);
    manager.focusNext();
    CHECK(manager.primaryFocus() == nullptr);
}

TEST_CASE("FocusManager setFocus on an ineligible scope is a no-op",
          "[FocusManager]")
{
    FocusManager manager;
    auto scope = make_rcp<FocusNode>();
    auto leaf = make_rcp<FocusNode>();
    // The requested target itself cannot be focused, so the request does
    // nothing at all — it does not fall through to an eligible descendant.
    scope->canFocus(false);

    manager.addChild(nullptr, scope);
    manager.addChild(scope, leaf);

    manager.setFocus(scope);
    CHECK(manager.primaryFocus() == nullptr);
}

TEST_CASE("FocusManager setFocus on a leaf is unchanged", "[FocusManager]")
{
    FocusManager manager;
    auto scope = make_rcp<FocusNode>();
    auto leaf1 = make_rcp<FocusNode>();
    auto leaf2 = make_rcp<FocusNode>();

    manager.addChild(nullptr, scope);
    manager.addChild(scope, leaf1);
    manager.addChild(scope, leaf2);

    // Directly focusing a leaf focuses that exact leaf.
    manager.setFocus(leaf2);
    CHECK(manager.primaryFocus() == leaf2);
}

TEST_CASE("FocusManager Tab after focusing a scope descends into it",
          "[FocusManager]")
{
    FocusManager manager;
    auto scope = make_rcp<FocusNode>();
    auto leaf1 = make_rcp<FocusNode>();
    auto leaf2 = make_rcp<FocusNode>();

    manager.addChild(nullptr, scope);
    manager.addChild(scope, leaf1);
    manager.addChild(scope, leaf2);

    // Focusing the scope focuses the scope; Tab then steps into its first
    // child, as pre-order says.
    manager.setFocus(scope);
    CHECK(manager.primaryFocus() == scope);

    manager.focusNext();
    CHECK(manager.primaryFocus() == leaf1);

    manager.focusNext();
    CHECK(manager.primaryFocus() == leaf2);
}

// =============================================================================
// FocusActionClear Tests
// =============================================================================

TEST_CASE("FocusActionClear perform clears the primary focus",
          "[FocusActionClear]")
{
    NoOpFactory factory;
    Artboard artboard(&factory);
    auto instance = instanceWithFocus(artboard);
    StateMachine machine;
    StateMachineInstance smi(&machine, instance.get());

    FocusManager* fm = smi.focusManager();
    MockFocusable f1;
    auto node1 = make_rcp<FocusNode>(&f1);
    fm->addChild(nullptr, node1);
    fm->setFocus(node1);
    REQUIRE(fm->primaryFocus() == node1);

    FocusActionClear action;
    action.perform(&smi, ListenerInvocation::none());

    CHECK(fm->primaryFocus() == nullptr);
}

TEST_CASE("FocusActionClear perform is a no-op when nothing is focused",
          "[FocusActionClear]")
{
    NoOpFactory factory;
    Artboard artboard(&factory);
    auto instance = instanceWithFocus(artboard);
    StateMachine machine;
    StateMachineInstance smi(&machine, instance.get());

    REQUIRE(smi.focusManager()->primaryFocus() == nullptr);

    FocusActionClear action;
    action.perform(&smi, ListenerInvocation::none());

    CHECK(smi.focusManager()->primaryFocus() == nullptr);
}

TEST_CASE("FocusActionClear perform ignores null StateMachineInstance",
          "[FocusActionClear]")
{
    FocusActionClear action;
    // Must not dereference the null instance.
    action.perform(nullptr, ListenerInvocation::none());
}

// =============================================================================
// TransitionFocusCondition Tests
// =============================================================================

TEST_CASE("TransitionFocusCondition uses the reassigned core type key",
          "[TransitionFocusCondition]")
{
    // Locks in the collision fix: master's font PR claimed 1035, so this
    // condition was reassigned to 1038. A regression here means a type-key
    // clash on import/export.
    // Copy into a local to avoid ODR-using the in-class static constant
    // (which has no out-of-line definition) when binding it to Catch2's
    // by-reference comparison expressions.
    uint16_t typeKey = TransitionFocusConditionBase::typeKey;
    CHECK(typeKey == 1038);

    auto condition = std::make_unique<TransitionFocusCondition>();
    CHECK(condition->coreType() == typeKey);
    CHECK(condition->is<TransitionFocusCondition>());
}

TEST_CASE("TransitionFocusCondition evaluate returns false for a null "
          "StateMachineInstance",
          "[TransitionFocusCondition]")
{
    // Heap allocation value-initializes the (comparator) members to null, so
    // the guard clauses and destructor are well-defined even without import.
    auto condition = std::make_unique<TransitionFocusCondition>();
    CHECK(condition->evaluate(nullptr, nullptr) == false);
}

TEST_CASE("TransitionFocusCondition evaluate returns false when no target "
          "comparator is configured",
          "[TransitionFocusCondition]")
{
    NoOpFactory factory;
    Artboard artboard(&factory);
    auto instance = instanceWithFocus(artboard);
    StateMachine machine;
    StateMachineInstance smi(&machine, instance.get());

    auto condition = std::make_unique<TransitionFocusCondition>();
    // With neither comparator set to a TransitionPropertyComponentComparator,
    // there is no focus target to evaluate against, so the condition is false.
    CHECK(condition->evaluate(&smi, nullptr) == false);
}

// =============================================================================
// Re-homing focus when the target disappears
// =============================================================================

TEST_CASE("FocusManager re-homes focus to a sibling leaf when the target hides",
          "[FocusManager]")
{
    FocusManager manager;
    MockFocusable parentFocusable, leafAFocusable, leafBFocusable;
    auto parent = make_rcp<FocusNode>(&parentFocusable);
    auto leafA = make_rcp<FocusNode>(&leafAFocusable);
    auto leafB = make_rcp<FocusNode>(&leafBFocusable);
    manager.addChild(nullptr, parent);
    manager.addChild(parent, leafA);
    manager.addChild(parent, leafB);

    manager.setFocus(leafA);
    REQUIRE(manager.primaryFocus() == leafA);

    // A hides: focus lands on its sibling, not on the parent and not nowhere.
    leafAFocusable.eligible = false;
    manager.dropFocusIfFocusTargetHidden();
    CHECK(manager.primaryFocus() == leafB);
    CHECK(leafAFocusable.blurredCount == 1);
    CHECK(leafBFocusable.focusedCount == 1);
    // The shared ancestor never lost focus, so it is not re-notified.
    CHECK(parentFocusable.blurredCount == 0);
    CHECK(parentFocusable.focusedCount == 1);
}

TEST_CASE("FocusManager re-homes focus to the parent when it is the only "
          "remaining stop",
          "[FocusManager]")
{
    FocusManager manager;
    MockFocusable parentFocusable, leafFocusable;
    auto parent = make_rcp<FocusNode>(&parentFocusable);
    auto leaf = make_rcp<FocusNode>(&leafFocusable);
    manager.addChild(nullptr, parent);
    manager.addChild(parent, leaf);

    manager.setFocus(leaf);
    REQUIRE(manager.primaryFocus() == leaf);

    // No sibling to fall back to, so the parent itself takes focus.
    leafFocusable.eligible = false;
    manager.dropFocusIfFocusTargetHidden();
    CHECK(manager.primaryFocus() == parent);
    CHECK(leafFocusable.blurredCount == 1);
}

TEST_CASE("FocusManager walks past a hidden parent to an eligible grandparent",
          "[FocusManager]")
{
    FocusManager manager;
    MockFocusable grandFocusable, parentFocusable, leafFocusable;
    auto grand = make_rcp<FocusNode>(&grandFocusable);
    auto parent = make_rcp<FocusNode>(&parentFocusable);
    auto leaf = make_rcp<FocusNode>(&leafFocusable);
    manager.addChild(nullptr, grand);
    manager.addChild(grand, parent);
    manager.addChild(parent, leaf);

    manager.setFocus(leaf);
    REQUIRE(manager.primaryFocus() == leaf);

    // The whole parent branch hides: skip it, land on the grandparent.
    leafFocusable.eligible = false;
    parentFocusable.eligible = false;
    manager.dropFocusIfFocusTargetHidden();
    CHECK(manager.primaryFocus() == grand);
}

TEST_CASE("FocusManager clears focus when no ancestor can hold it",
          "[FocusManager]")
{
    FocusManager manager;
    MockFocusable parentFocusable, leafFocusable;
    auto parent = make_rcp<FocusNode>(&parentFocusable);
    auto leaf = make_rcp<FocusNode>(&leafFocusable);
    manager.addChild(nullptr, parent);
    manager.addChild(parent, leaf);

    manager.setFocus(leaf);
    REQUIRE(manager.primaryFocus() == leaf);

    // Everything up the chain is gone, so focus really does clear.
    leafFocusable.eligible = false;
    parentFocusable.eligible = false;
    manager.dropFocusIfFocusTargetHidden();
    CHECK(manager.primaryFocus() == nullptr);
}

TEST_CASE("FocusManager re-homing does not land on a non-focusable ancestor",
          "[FocusManager]")
{
    FocusManager manager;
    // canFocus=false ancestor: authored as a pass-through container.
    MockFocusable containerFocusable, leafFocusable;
    auto container = make_rcp<FocusNode>(&containerFocusable);
    container->canFocus(false);
    auto leaf = make_rcp<FocusNode>(&leafFocusable);
    manager.addChild(nullptr, container);
    manager.addChild(container, leaf);

    manager.setFocus(leaf);
    REQUIRE(manager.primaryFocus() == leaf);

    leafFocusable.eligible = false;
    manager.dropFocusIfFocusTargetHidden();
    // The container cannot take focus, and there is nothing above it.
    CHECK(manager.primaryFocus() == nullptr);
}

TEST_CASE("FocusManager clears rather than crossing into another root branch",
          "[FocusManager]")
{
    FocusManager manager;
    MockFocusable rootAFocusable, leafAFocusable, rootBFocusable,
        leafBFocusable;
    auto rootA = make_rcp<FocusNode>(&rootAFocusable);
    auto leafA = make_rcp<FocusNode>(&leafAFocusable);
    auto rootB = make_rcp<FocusNode>(&rootBFocusable);
    auto leafB = make_rcp<FocusNode>(&leafBFocusable);
    manager.addChild(nullptr, rootA);
    manager.addChild(rootA, leafA);
    manager.addChild(nullptr, rootB);
    manager.addChild(rootB, leafB);

    manager.setFocus(leafA);
    REQUIRE(manager.primaryFocus() == leafA);

    // The whole first root branch goes away. Re-homing stays inside the
    // ancestor chain, so it stops at rootA rather than continuing into the
    // second branch — an unrelated tree, a separate artboard in practice.
    // Focus clears; Tab still reaches rootB, it just isn't jumped to.
    leafAFocusable.eligible = false;
    rootAFocusable.eligible = false;
    manager.dropFocusIfFocusTargetHidden();
    CHECK(manager.primaryFocus() == nullptr);
    CHECK(leafBFocusable.focusedCount == 0);
}

TEST_CASE("FocusManager clears when a focused root node hides",
          "[FocusManager]")
{
    FocusManager manager;
    MockFocusable rootAFocusable, rootBFocusable;
    auto rootA = make_rcp<FocusNode>(&rootAFocusable);
    auto rootB = make_rcp<FocusNode>(&rootBFocusable);
    manager.addChild(nullptr, rootA);
    manager.addChild(nullptr, rootB);

    manager.setFocus(rootA);
    REQUIRE(manager.primaryFocus() == rootA);

    // Same rule with no ancestors at all to walk: a root node's siblings are
    // other root branches, so there is nothing in scope to re-home to.
    rootAFocusable.eligible = false;
    manager.dropFocusIfFocusTargetHidden();
    CHECK(manager.primaryFocus() == nullptr);
    CHECK(rootBFocusable.focusedCount == 0);
}

TEST_CASE("FocusManager only drops a hidden target for its own root",
          "[FocusManager]")
{
    NoOpFactory factory;
    Artboard artboardA(&factory);
    Artboard artboardB(&factory);

    FocusManager manager;
    MockFocusable parentFocusable, leafAFocusable, leafBFocusable;
    parentFocusable.artboard = &artboardB;
    leafAFocusable.artboard = &artboardB;
    leafBFocusable.artboard = &artboardB;
    auto parent = make_rcp<FocusNode>(&parentFocusable);
    auto leafA = make_rcp<FocusNode>(&leafAFocusable);
    auto leafB = make_rcp<FocusNode>(&leafBFocusable);
    manager.addChild(nullptr, parent);
    manager.addChild(parent, leafA);
    manager.addChild(parent, leafB);

    manager.setFocus(leafA);
    REQUIRE(manager.primaryFocus() == leafA);

    leafAFocusable.eligible = false;
    // Root A's update pass hasn't refreshed anything in root B, so it must
    // not re-home B's target off what it reads there.
    manager.dropFocusIfFocusTargetHidden(&artboardA);
    CHECK(manager.primaryFocus() == leafA);

    // B's own pass does it.
    manager.dropFocusIfFocusTargetHidden(&artboardB);
    CHECK(manager.primaryFocus() == leafB);
}

} // namespace rive

namespace
{
// Walks Tab from nothing focused to the end of the order, collecting every
// stop it lands on. focusNext() reports false once it runs off the end of the
// root list (which also clears focus), so that ends the walk.
std::vector<rive::FocusNode*> collectTabOrder(rive::FocusManager* manager)
{
    manager->clearFocus();
    std::vector<rive::FocusNode*> order;
    // Guard against a cycle turning a failure into a hang.
    for (size_t step = 0; step < 32; step++)
    {
        if (!manager->focusNext() || manager->primaryFocusPtr() == nullptr)
        {
            break;
        }
        order.push_back(manager->primaryFocusPtr());
    }
    return order;
}

// The same order walked backwards, for comparing against a reversed forward
// walk: Shift+Tab has to retrace Tab exactly.
std::vector<rive::FocusNode*> collectShiftTabOrder(rive::FocusManager* manager)
{
    manager->clearFocus();
    std::vector<rive::FocusNode*> order;
    for (size_t step = 0; step < 32; step++)
    {
        if (!manager->focusPrevious() || manager->primaryFocusPtr() == nullptr)
        {
            break;
        }
        order.push_back(manager->primaryFocusPtr());
    }
    return order;
}
} // namespace

TEST_CASE("A click requests focus only while the child is focusable",
          "[silver]")
{
    // Each child artboard in focus_traversal_test.riv carries a pointer
    // listener that asks for focus on itself. That request goes through the
    // same gate as any other: canFocus decides whether focus may land, so a
    // child whose bound `focusable` is off must swallow the click, and must
    // start taking it again the moment the binding turns back on.
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/focus_traversal_test.riv", &silver);

    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);
    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);

    auto* viewModel = file->defaultArtboardViewModel(artboard.get());
    REQUIRE(viewModel != nullptr);
    auto vmi = viewModel->createDefaultInstance();
    REQUIRE(vmi != nullptr);
    stateMachine->bindViewModelInstance(vmi->instance());

    // `child1` drives the second child of its parent and `child2` the first —
    // the view model names run opposite to hierarchy order.
    auto* layoutChildren = vmi->propertyList("layoutChildren");
    REQUIRE(layoutChildren != nullptr);
    REQUIRE(layoutChildren->size() == 3);
    std::vector<rive::rcp<rive::ViewModelInstanceRuntime>> rowInstances;
    for (int i = 0; i < 3; i++)
    {
        rowInstances.push_back(layoutChildren->instanceAt(i));
        REQUIRE(rowInstances.back() != nullptr);
    }
    // Start with every child reachable so the clicks below have somewhere to
    // land; the toggles later take individual ones away again.
    auto setLeaf = [&](rive::ViewModelInstanceRuntime* instance,
                       const std::string& child,
                       bool focusable,
                       bool traversable) {
        instance->propertyBoolean(child + "/focusable")->value(focusable);
        instance->propertyBoolean(child + "/traversable")->value(traversable);
    };
    setLeaf(vmi.get(), "node/child1", true, true);
    setLeaf(vmi.get(), "node/child2", true, true);
    for (auto& row : rowInstances)
    {
        setLeaf(row.get(), "child1", true, true);
        setLeaf(row.get(), "child2", true, true);
    }
    stateMachine->advanceAndApply(0.016f);
    stateMachine->advanceAndApply(0.016f);

    auto* manager = stateMachine->focusManager();
    REQUIRE(manager != nullptr);

    // The eight children, in the order they sit down the artboard: the nested
    // artboard's two, then two per list row.
    const auto& roots = manager->rootNodes();
    REQUIRE(roots.size() == 2);
    std::vector<rive::FocusNode*> children;
    rive::FocusNode* nestedParent = roots[0]->children()[0].get();
    REQUIRE(nestedParent->children().size() == 2);
    children.push_back(nestedParent->children()[0].get());
    children.push_back(nestedParent->children()[1].get());
    rive::FocusNode* listScope = roots[1]->children()[0].get();
    REQUIRE(listScope->children().size() == 3);
    for (const auto& row : listScope->children())
    {
        rive::FocusNode* rowParent = row->children()[0].get();
        REQUIRE(rowParent->children().size() == 2);
        children.push_back(rowParent->children()[0].get());
        children.push_back(rowParent->children()[1].get());
    }
    REQUIRE(children.size() == 8);

    auto renderer = silver.makeRenderer();

    // Click the middle of a node's own world bounds rather than a hardcoded
    // eighth of the artboard, so the test keeps aiming at the right child if
    // the layout is ever re-proportioned.
    auto clickCenterOf = [&](rive::FocusNode* node) {
        rive::AABB bounds;
        REQUIRE(node->focusable() != nullptr);
        REQUIRE(node->focusable()->worldBounds(bounds));
        const rive::Vec2D center = bounds.center();
        stateMachine->pointerDown(center);
        stateMachine->pointerUp(center);
        stateMachine->advanceAndApply(0.016f);
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
        silver.addFrame();
    };

    // === 1. While focusable, a click focuses the child it landed on ========
    // Every one of the eight, so the listener is proven wired on all of them
    // and no click leaks to a neighbour or to an enclosing parent.
    for (size_t i = 0; i < children.size(); i++)
    {
        manager->clearFocus();
        clickCenterOf(children[i]);
        CHECK(manager->primaryFocusPtr() == children[i]);
    }

    // === 2. Not focusable: the click is swallowed =========================
    // Focus is parked on a different child first, so this asserts the click
    // does nothing at all rather than merely failing to land — a request that
    // cleared focus would pass a null check but still be wrong.
    rive::FocusNode* nestedFirst = children[0];
    rive::FocusNode* nestedSecond = children[1];
    setLeaf(vmi.get(),
            "node/child2",
            /*focusable=*/false,
            /*traversable=*/true);
    stateMachine->advanceAndApply(0.016f);
    REQUIRE(!nestedFirst->canFocus());

    manager->clearFocus();
    manager->setFocus(ref_rcp(nestedSecond));
    REQUIRE(manager->primaryFocusPtr() == nestedSecond);
    clickCenterOf(nestedFirst);
    CHECK(manager->primaryFocusPtr() == nestedSecond);

    // === 3. Focusable again: the same click works ========================
    setLeaf(vmi.get(), "node/child2", /*focusable=*/true, /*traversable=*/true);
    stateMachine->advanceAndApply(0.016f);
    REQUIRE(nestedFirst->canFocus());

    clickCenterOf(nestedFirst);
    CHECK(manager->primaryFocusPtr() == nestedFirst);

    // === 4. The same cycle inside a list row =============================
    // Each row is driven by its own view model instance, so switching the
    // middle row's child off must leave the other rows clickable.
    rive::FocusNode* middleRowSecond = children[5];
    rive::FocusNode* lastRowSecond = children[7];
    setLeaf(rowInstances[1].get(),
            "child1",
            /*focusable=*/false,
            /*traversable=*/true);
    stateMachine->advanceAndApply(0.016f);
    REQUIRE(!middleRowSecond->canFocus());
    REQUIRE(lastRowSecond->canFocus());

    manager->clearFocus();
    manager->setFocus(ref_rcp(lastRowSecond));
    clickCenterOf(middleRowSecond);
    CHECK(manager->primaryFocusPtr() == lastRowSecond);

    // The untouched rows still take a click.
    clickCenterOf(children[3]);
    CHECK(manager->primaryFocusPtr() == children[3]);

    setLeaf(rowInstances[1].get(),
            "child1",
            /*focusable=*/true,
            /*traversable=*/true);
    stateMachine->advanceAndApply(0.016f);
    clickCenterOf(middleRowSecond);
    CHECK(manager->primaryFocusPtr() == middleRowSecond);

    // === 5. canTraverse has no say over a click ==========================
    // Tab skips a child with traversable off, but a pointer names it
    // directly, and canFocus is the only gate on that.
    setLeaf(vmi.get(),
            "node/child1",
            /*focusable=*/true,
            /*traversable=*/false);
    stateMachine->advanceAndApply(0.016f);
    REQUIRE(nestedSecond->canFocus());
    REQUIRE(!nestedSecond->canTraverse());

    manager->clearFocus();
    clickCenterOf(nestedSecond);
    CHECK(manager->primaryFocusPtr() == nestedSecond);

    // And with both off it is unreachable by either route.
    setLeaf(vmi.get(),
            "node/child1",
            /*focusable=*/false,
            /*traversable=*/false);
    stateMachine->advanceAndApply(0.016f);
    manager->clearFocus();
    manager->setFocus(ref_rcp(nestedFirst));
    clickCenterOf(nestedSecond);
    CHECK(manager->primaryFocusPtr() == nestedFirst);

    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("focus_traversal_click_to_focus"));
}

TEST_CASE("Data bound focus flags drive traversal through a nested artboard "
          "and a list",
          "[silver]")
{
    // focus_traversal_test.riv: a Main artboard with two layouts, each owning a
    // FocusData. One hosts a nested artboard, the other an artboard list of
    // three rows. Both hosted artboards carry a FocusData of their own plus two
    // focusable children, and every child's canFocus/canTraverse is data bound
    // to a `focusable`/`traversable` boolean on its own view model instance.
    //
    // That shape exercises all three of the traversal rules at once:
    //   - the two Main layouts are canTraverse=false, and must not take their
    //     subtrees out of the order with them;
    //   - the hosted artboards' FocusData are canFocus+canTraverse, so they are
    //     stops in their own right and come before their children;
    //   - the children are only reachable when BOTH their booleans are on.
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/focus_traversal_test.riv", &silver);

    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);
    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);

    auto* viewModel = file->defaultArtboardViewModel(artboard.get());
    REQUIRE(viewModel != nullptr);
    auto vmi = viewModel->createDefaultInstance();
    REQUIRE(vmi != nullptr);
    stateMachine->bindViewModelInstance(vmi->instance());
    stateMachine->advanceAndApply(0.016f);

    auto* manager = stateMachine->focusManager();
    REQUIRE(manager != nullptr);

    // === Handles into the focus tree =======================================
    const auto& roots = manager->rootNodes();
    REQUIRE(roots.size() == 2);

    // Both Main-level layout nodes can hold focus but opt out of traversal.
    rive::FocusNode* nestedHost = roots[0].get();
    rive::FocusNode* listHost = roots[1].get();
    REQUIRE(nestedHost->canFocus());
    REQUIRE(!nestedHost->canTraverse());
    REQUIRE(listHost->canFocus());
    REQUIRE(!listHost->canTraverse());

    REQUIRE(nestedHost->children().size() == 1);
    rive::FocusNode* nestedParent = nestedHost->children()[0].get();
    REQUIRE(nestedParent->canFocus());
    REQUIRE(nestedParent->canTraverse());
    REQUIRE(nestedParent->children().size() == 2);
    rive::FocusNode* nestedLeafA = nestedParent->children()[0].get();
    rive::FocusNode* nestedLeafB = nestedParent->children()[1].get();

    // The list sits under a structural scope, one structural row per item.
    REQUIRE(listHost->children().size() == 1);
    rive::FocusNode* listScope = listHost->children()[0].get();
    REQUIRE(listScope->children().size() == 3);
    std::vector<rive::FocusNode*> rowParents;
    std::vector<rive::FocusNode*> rowLeafA;
    std::vector<rive::FocusNode*> rowLeafB;
    for (const auto& row : listScope->children())
    {
        REQUIRE(row->children().size() == 1);
        rive::FocusNode* rowParent = row->children()[0].get();
        REQUIRE(rowParent->children().size() == 2);
        rowParents.push_back(rowParent);
        rowLeafA.push_back(rowParent->children()[0].get());
        rowLeafB.push_back(rowParent->children()[1].get());
    }

    // === Handles into the bound booleans ===================================
    // The view model names run opposite to hierarchy order: `child1` drives the
    // SECOND child of its parent and `child2` the first. Bind by what each one
    // actually moves rather than by its name.
    auto boolFor = [](rive::ViewModelInstanceRuntime* instance,
                      const std::string& path) {
        auto* property = instance->propertyBoolean(path);
        REQUIRE(property != nullptr);
        return property;
    };
    auto* nestedLeafAFocusable = boolFor(vmi.get(), "node/child2/focusable");
    auto* nestedLeafATraversable =
        boolFor(vmi.get(), "node/child2/traversable");
    auto* nestedLeafBFocusable = boolFor(vmi.get(), "node/child1/focusable");
    auto* nestedLeafBTraversable =
        boolFor(vmi.get(), "node/child1/traversable");

    auto* layoutChildren = vmi->propertyList("layoutChildren");
    REQUIRE(layoutChildren != nullptr);
    REQUIRE(layoutChildren->size() == 3);
    std::vector<rive::rcp<rive::ViewModelInstanceRuntime>> rowInstances;
    for (int i = 0; i < 3; i++)
    {
        auto item = layoutChildren->instanceAt(i);
        REQUIRE(item != nullptr);
        rowInstances.push_back(item);
    }

    auto renderer = silver.makeRenderer();
    // Settle, draw, and report the order the current flags produce.
    auto settleAndDraw = [&]() {
        stateMachine->advanceAndApply(0.016f);
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
        silver.addFrame();
    };

    // === 1. Defaults: every leaf boolean is off ============================
    // Only the hosted artboards' own FocusData are stops. The two Main layouts
    // are canTraverse=false, yet everything beneath them is still reachable —
    // a parent's flags speak only for itself.
    settleAndDraw();
    {
        const std::vector<rive::FocusNode*> expected{
            nestedParent,
            rowParents[0],
            rowParents[1],
            rowParents[2],
        };
        CHECK(collectTabOrder(manager) == expected);

        std::vector<rive::FocusNode*> reversed(expected.rbegin(),
                                               expected.rend());
        CHECK(collectShiftTabOrder(manager) == reversed);
    }

    // === 2. focusable alone is not enough to be navigable ==================
    nestedLeafBFocusable->value(true);
    settleAndDraw();
    {
        CHECK(nestedLeafB->canFocus());
        CHECK(!nestedLeafB->canTraverse());

        // Tab still skips it...
        const std::vector<rive::FocusNode*> expected{
            nestedParent,
            rowParents[0],
            rowParents[1],
            rowParents[2],
        };
        CHECK(collectTabOrder(manager) == expected);

        // ...but naming it directly still focuses it, because canFocus is what
        // decides whether focus may land, and canTraverse only decides whether
        // navigation goes looking.
        manager->setFocus(ref_rcp(nestedLeafB));
        CHECK(manager->primaryFocusPtr() == nestedLeafB);
    }

    // === 3. traversable alone is not enough either =========================
    nestedLeafBFocusable->value(false);
    nestedLeafBTraversable->value(true);
    settleAndDraw();
    {
        CHECK(!nestedLeafB->canFocus());
        CHECK(nestedLeafB->canTraverse());

        const std::vector<rive::FocusNode*> expected{
            nestedParent,
            rowParents[0],
            rowParents[1],
            rowParents[2],
        };
        CHECK(collectTabOrder(manager) == expected);

        // And it cannot be focused by name either.
        manager->clearFocus();
        manager->setFocus(ref_rcp(nestedLeafB));
        CHECK(manager->primaryFocusPtr() == nullptr);
    }

    // === 4. Both on: the leaf joins, after its parent =======================
    nestedLeafBFocusable->value(true);
    settleAndDraw();
    {
        const std::vector<rive::FocusNode*> expected{
            nestedParent,
            nestedLeafB,
            rowParents[0],
            rowParents[1],
            rowParents[2],
        };
        CHECK(collectTabOrder(manager) == expected);
    }

    // === 5. Both leaves of the nested artboard, in hierarchy order ==========
    nestedLeafAFocusable->value(true);
    nestedLeafATraversable->value(true);
    settleAndDraw();
    {
        const std::vector<rive::FocusNode*> expected{
            nestedParent,
            nestedLeafA,
            nestedLeafB,
            rowParents[0],
            rowParents[1],
            rowParents[2],
        };
        CHECK(collectTabOrder(manager) == expected);

        std::vector<rive::FocusNode*> reversed(expected.rbegin(),
                                               expected.rend());
        CHECK(collectShiftTabOrder(manager) == reversed);
    }

    // === 6. One list row opts its children in ==============================
    // Each row is driven by its own view model instance, so enabling the middle
    // row must leave the other two alone.
    rowInstances[1]->propertyBoolean("child2/focusable")->value(true);
    rowInstances[1]->propertyBoolean("child2/traversable")->value(true);
    rowInstances[1]->propertyBoolean("child1/focusable")->value(true);
    rowInstances[1]->propertyBoolean("child1/traversable")->value(true);
    settleAndDraw();
    {
        const std::vector<rive::FocusNode*> expected{
            nestedParent,
            nestedLeafA,
            nestedLeafB,
            rowParents[0],
            rowParents[1],
            rowLeafA[1],
            rowLeafB[1],
            rowParents[2],
        };
        CHECK(collectTabOrder(manager) == expected);
    }

    // === 7. Every leaf in the file, forward and back =======================
    for (int i = 0; i < 3; i++)
    {
        for (const char* path : {"child1/focusable",
                                 "child1/traversable",
                                 "child2/focusable",
                                 "child2/traversable"})
        {
            rowInstances[static_cast<size_t>(i)]->propertyBoolean(path)->value(
                true);
        }
    }
    settleAndDraw();
    {
        const std::vector<rive::FocusNode*> expected{
            nestedParent,
            nestedLeafA,
            nestedLeafB,
            rowParents[0],
            rowLeafA[0],
            rowLeafB[0],
            rowParents[1],
            rowLeafA[1],
            rowLeafB[1],
            rowParents[2],
            rowLeafA[2],
            rowLeafB[2],
        };
        CHECK(collectTabOrder(manager) == expected);

        std::vector<rive::FocusNode*> reversed(expected.rbegin(),
                                               expected.rend());
        CHECK(collectShiftTabOrder(manager) == reversed);
    }

    // === 8. Turning a row's children back off removes just those ===========
    for (const char* path : {"child1/focusable",
                             "child1/traversable",
                             "child2/focusable",
                             "child2/traversable"})
    {
        rowInstances[0]->propertyBoolean(path)->value(false);
    }
    settleAndDraw();
    {
        const std::vector<rive::FocusNode*> expected{
            nestedParent,
            nestedLeafA,
            nestedLeafB,
            rowParents[0],
            rowParents[1],
            rowLeafA[1],
            rowLeafB[1],
            rowParents[2],
            rowLeafA[2],
            rowLeafB[2],
        };
        CHECK(collectTabOrder(manager) == expected);
    }

    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("focus_traversal_data_bound"));
}

TEST_CASE("Swapping bindable artboard registers nested focus nodes for Tab",
          "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/bindable_focus_tree_swap.riv", &silver);

    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);
    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);

    auto vmi = file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(vmi != nullptr);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.016f);

    auto* focusManager = stateMachine->focusManager();
    REQUIRE(focusManager != nullptr);
    REQUIRE(stateMachine->hasFocusNodes() == true);

    focusManager->focusNext();
    stateMachine->advanceAndApply(0.016f);
    REQUIRE(focusManager->primaryFocus() != nullptr);

    // The main artboard offers two stops: the container that holds the focus
    // tree, then the single leaf under it. The empty bindable slot is a
    // structural scope and contributes none.
    CHECK(stateMachine->focusNext() == true);
    CHECK(stateMachine->focusNext() == false);
    // Running off the end cleared focus; step back onto that last node.
    stateMachine->focusPrevious();

    auto* artboardProp = vmi->propertyValue("bindedArt");
    REQUIRE(artboardProp != nullptr);
    REQUIRE(artboardProp->is<rive::ViewModelInstanceArtboard>());
    auto* vmiArtboard = artboardProp->as<rive::ViewModelInstanceArtboard>();

    // Has other focus nodes in this artboard
    auto focusableSource = file->bindableArtboardNamed("Focusable");
    REQUIRE(focusableSource != nullptr);

    vmiArtboard->asset(focusableSource);
    stateMachine->advanceAndApply(0.016f);

    rive::NestedArtboard* focusableHost = nullptr;
    for (auto* nestedHost : artboard->nestedArtboards())
    {
        auto* source = nestedHost->sourceArtboard();
        if (source != nullptr && source->name() == "Focusable")
        {
            focusableHost = nestedHost;
            break;
        }
    }
    REQUIRE(focusableHost != nullptr);
    auto* focusableInstance = focusableHost->artboardInstance(0);
    REQUIRE(focusableInstance != nullptr);

    CHECK(stateMachine->focusNext() == true);
    CHECK(focusManager->primaryFocus() != nullptr);
    CHECK(focusManager->primaryFocusImmediateArtboard() == focusableInstance);
}

TEST_CASE("Swapping a bindable nested artboard preserves focus held elsewhere",
          "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/bindable_focus_tree_swap.riv", &silver);

    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);
    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);

    auto vmi = file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(vmi != nullptr);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.016f);

    auto* focusManager = stateMachine->focusManager();
    REQUIRE(focusManager != nullptr);

    // Focus the main artboard's own focus node. Before the swap the bindable
    // host is "Plain" (no focus nodes), so the main node is the only focusable.
    focusManager->focusNext();
    auto focused = focusManager->primaryFocus();
    REQUIRE(focused != nullptr);
    REQUIRE(focusManager->primaryFocusImmediateArtboard() == artboard.get());

    // Swap the (unrelated) bindable nested artboard to one that HAS focus
    // nodes.
    auto* artboardProp = vmi->propertyValue("bindedArt");
    REQUIRE(artboardProp != nullptr);
    REQUIRE(artboardProp->is<rive::ViewModelInstanceArtboard>());
    auto* vmiArtboard = artboardProp->as<rive::ViewModelInstanceArtboard>();
    auto focusableSource = file->bindableArtboardNamed("Focusable");
    REQUIRE(focusableSource != nullptr);
    vmiArtboard->asset(focusableSource);
    stateMachine->advanceAndApply(0.016f);

    // Focus held on the main artboard must survive the unrelated nested swap:
    // the swap only re-syncs the swapped host's subtree, not the whole tree.
    CHECK(focusManager->primaryFocus() == focused);
    CHECK(focusManager->primaryFocusImmediateArtboard() == artboard.get());
}

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
    // The first focusable is now inside a data-bound nested artboard
    REQUIRE(focusManager->primaryFocus() != nullptr);
    REQUIRE(focusManager->primaryFocusImmediateArtboard() != nullptr);
    REQUIRE(focusManager->primaryFocusImmediateArtboard() != artboard.get());
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    // ===> Frame 2
    silver.addFrame();

    // Tab next into the main artboard's own element — the one `opacity`
    // controls.
    focusManager->focusNext();
    REQUIRE(focusManager->primaryFocus() != nullptr);
    REQUIRE(focusManager->primaryFocusImmediateArtboard() == artboard.get());
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    silver.addFrame();

    // Hide that focused element; focus must be dropped.
    opacityProp->propertyValue(0);
    // The advance that zeroes the opacity is the one that drops the focus:
    // the check runs after updatePass, so it reads this frame's opacity
    // rather than the previous frame's.
    stateMachine->advanceAndApply(0.016f);
    REQUIRE(focusManager->primaryFocus() == nullptr);
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

    // The silver is a rendered image, so on its own it pins nothing a reader
    // can check and a rebaseline would bless any order at all. State the order
    // here too: pre-order over this file's tree — a top-level node, the
    // container, its three children, the last top-level node — then off the
    // end of the root list (which clears), then round again.
    auto* manager = stateMachine->focusManager();
    const auto& roots = manager->rootNodes();
    REQUIRE(roots.size() == 3);
    rive::FocusNode* firstTop = roots[0].get();
    rive::FocusNode* container = roots[1].get();
    rive::FocusNode* lastTop = roots[2].get();
    REQUIRE(container->children().size() == 3);

    const std::vector<rive::FocusNode*> expected{
        firstTop,
        // The container is a stop in its own right now; holding focusable
        // children no longer takes it out of the order.
        container,
        container->children()[0].get(),
        container->children()[1].get(),
        container->children()[2].get(),
        lastTop,
        // Past the last root the walk runs off the end, which clears — the
        // thing this case is named for.
        nullptr,
    };

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());
    silver.addFrame();
    for (size_t step = 0; step < expected.size(); step++)
    {
        manager->focusNext();
        stateMachine->advanceAndApply(0.1f);
        CHECK(manager->primaryFocusPtr() == expected[step]);
        artboard->draw(renderer.get());
        if (step + 1 < expected.size())
        {
            silver.addFrame();
        }
    }

    CHECK(silver.matches("focusable_element"));
}

TEST_CASE("ArtboardComponentList list scope is registered on shared "
          "FocusManager",
          "[FocusManager][list]")
{
    auto file = ReadRiveFile("assets/component_list_1.riv");
    auto artboard = file->artboardNamed("Main");
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
    // Transparent structural scope: not a focus target itself; traversal
    // descends through it (focusNodeTraversable) to reach item focusables.
    CHECK(scope->canFocus() == false);
    CHECK(scope->canTraverse() == false);
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
    auto artboard = file->artboardNamed("Main");
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

// The test asset carries Luau bytecode scripts, which only the Luau
// backend runs.
#ifdef WITH_RIVE_SCRIPTING_LUAU
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
#endif

TEST_CASE("Focus based transitions work", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/focus_test.riv", &silver);

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

    stateMachine->pointerDown(rive::Vec2D(55.0, 65.0));
    stateMachine->pointerUp(rive::Vec2D(55.0, 65.0));
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    silver.addFrame();

    stateMachine->pointerDown(rive::Vec2D(442.0, 65.0));
    stateMachine->pointerUp(rive::Vec2D(442.0, 65.0));
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("focus_test"));
}
TEST_CASE("List item focus tree stays under its row when the item's state "
          "machine is (re)wired during the focus sync",
          "[FocusManager][list]")
{
    // Regression for the syncListRowNodesWithList ordering bug: each list
    // item's state machine must be wired to the shared FocusManager BEFORE the
    // item's focus tree is (re)built under its row. setExternalFocusManager
    // rebuilds the item's focus tree at the manager ROOT as a side effect, so
    // if it runs after the build-under-row it clobbers the row placement and
    // the item's focus nodes end up detached from the list scope (at the
    // manager root).
    //
    // The natural build path happens to wire the manager first (via
    // linkStateMachineToArtboard, whose setExternalFocusManager runs before the
    // row sync), so the in-loop call is normally skipped by the
    // `smi->focusManager() != fm` guard. Force the mismatch to exercise the
    // ordering directly.
    auto file = ReadRiveFile("assets/list_focus_order.riv");
    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);
    auto* fm = stateMachine->focusManager();
    REQUIRE(fm != nullptr);

    auto vmi = file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(vmi != nullptr);
    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.016f);

    REQUIRE(artboard->artboardComponentLists().size() == 1);
    auto* list = artboard->artboardComponentLists()[0];
    REQUIRE(list != nullptr);
    const int itemCount = static_cast<int>(list->artboardCount());
    REQUIRE(itemCount > 0);

    // A row node with children means the item's focus subtree is parented under
    // it (inside the list scope) — the invariant the bug breaks.
    auto rowForItem = [&](int i) -> rive::FocusNode* {
        auto scope = list->listScopeFocusNode();
        if (scope == nullptr || i >= static_cast<int>(scope->children().size()))
        {
            return nullptr;
        }
        return scope->children()[static_cast<size_t>(i)].get();
    };

    // Pick a list item that (after the normal build) has focus content placed
    // under its row AND owns a state machine — the only case where the in-loop
    // setExternalFocusManager fires.
    int targetIndex = -1;
    for (int i = 0; i < itemCount; i++)
    {
        rive::FocusNode* row = rowForItem(i);
        if (row != nullptr && !row->children().empty() &&
            list->stateMachineInstance(i) != nullptr)
        {
            targetIndex = i;
            break;
        }
    }
    REQUIRE(targetIndex != -1);

    // Force the mismatch: drop the item's shared-manager wiring so the next
    // focus sync must call setExternalFocusManager(fm) again — the exact call
    // whose manager-root rebuild would clobber the row placement if it ran
    // after the build-under-row.
    list->stateMachineInstance(targetIndex)->setExternalFocusManager(nullptr);
    CHECK(list->stateMachineInstance(targetIndex)->focusManager() != fm);

    // Re-run the parent focus build; this recreates the list scope/rows and
    // re-syncs each item under its row.
    artboard->cleanupFocusTree();
    artboard->buildFocusTree(fm, nullptr);

    // With the fix (wire first, place last) the item's focus subtree is
    // parented under its row inside the list scope. With the bug it was rebuilt
    // at the manager root, leaving the row empty.
    rive::FocusNode* targetRow = rowForItem(targetIndex);
    REQUIRE(targetRow != nullptr);
    CHECK(targetRow->manager() == fm);
    CHECK_FALSE(targetRow->children().empty());
    CHECK(list->stateMachineInstance(targetIndex)->focusManager() == fm);
}

TEST_CASE("State machines over one artboard instance share the artboard's "
          "FocusManager",
          "[FocusManager][list]")
{
    // The FocusManager belongs to the artboard, not to a state machine. A
    // second StateMachineInstance over the same ArtboardInstance must reuse it
    // rather than stand up a second manager and migrate every FocusNode onto
    // it. That migration is what left persistent nodes -- the component list's
    // scope and row nodes, which outlive any state machine -- stamped with a
    // manager that could later die while they still pointed at it.
    auto file = ReadRiveFile("assets/list_focus_order.riv");
    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);

    // Root instances own a manager from the moment they are instanced, before
    // any state machine exists.
    auto* fm = artboard->focusManager();
    REQUIRE(fm != nullptr);

    auto vmi = file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(vmi != nullptr);
    artboard->bindViewModelInstance(vmi);

    auto first = artboard->stateMachineAt(0);
    REQUIRE(first != nullptr);
    CHECK(first->focusManager() == fm);
    first->advanceAndApply(0.016f);

    REQUIRE(artboard->artboardComponentLists().size() == 1);
    auto* list = artboard->artboardComponentLists()[0];
    REQUIRE(list->listScopeFocusNode() != nullptr);
    REQUIRE(list->listScopeFocusNode()->manager() == fm);

    // This is the shape the Android controller produces: an input queued for a
    // state machine that has not been instanced yet builds a second one over
    // the live artboard.
    auto second = artboard->stateMachineAt(0);
    REQUIRE(second != nullptr);

    CHECK(second->focusManager() == fm);
    CHECK(first->focusManager() == fm);
    CHECK(artboard->focusManager() == fm);

    // The list's persistent nodes were not migrated onto a different manager.
    auto scope = list->listScopeFocusNode();
    REQUIRE(scope != nullptr);
    CHECK(scope->manager() == fm);
    for (auto& row : scope->children())
    {
        CHECK(row->manager() == fm);
    }

    CHECK(fm->focusNext() == true);
    CHECK(fm->primaryFocus() != nullptr);
}

TEST_CASE("Destroying one state machine leaves another's focus tree intact",
          "[FocusManager][list]")
{
    // ~StateMachineInstance used to call cleanupFocusTree() whenever it owned
    // the manager, ripping out a tree a second state machine over the same
    // artboard was still using. With the manager owned by the artboard there
    // is nothing for a state machine to tear down.
    auto file = ReadRiveFile("assets/list_focus_order.riv");
    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);
    auto* fm = artboard->focusManager();
    REQUIRE(fm != nullptr);

    auto vmi = file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(vmi != nullptr);
    artboard->bindViewModelInstance(vmi);

    auto first = artboard->stateMachineAt(0);
    REQUIRE(first != nullptr);
    first->advanceAndApply(0.016f);

    auto second = artboard->stateMachineAt(0);
    REQUIRE(second != nullptr);

    REQUIRE(artboard->artboardComponentLists().size() == 1);
    auto* list = artboard->artboardComponentLists()[0];
    REQUIRE(list->listScopeFocusNode() != nullptr);

    first.reset();

    // The manager, the artboard's pointer to it, and the list's scope all
    // outlive the first state machine.
    CHECK(artboard->focusManager() == fm);
    CHECK(second->focusManager() == fm);
    auto scope = list->listScopeFocusNode();
    REQUIRE(scope != nullptr);
    CHECK(scope->manager() == fm);

    // And the surviving state machine can still drive focus through it.
    CHECK(fm->focusNext() == true);
    CHECK(fm->primaryFocus() != nullptr);
    second->advanceAndApply(0.016f);
    CHECK(fm->primaryFocus() != nullptr);
}

TEST_CASE("Swappable artboard slot keeps its place in tab order",
          "[FocusManager]")
{
    // File: https://editor.uat.rive.app/file/untitled/36028
    auto file = ReadRiveFile("assets/swappable_artboards_focus.riv");
    auto artboard = file->artboardNamed("Main");
    REQUIRE(artboard != nullptr);

    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);
    auto* focusManager = stateMachine->focusManager();
    REQUIRE(focusManager != nullptr);

    auto vmi = file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(vmi != nullptr);
    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.016f);
    stateMachine->advanceAndApply(0.016f);

    // Only the data-bound slot is flagged as swappable; static nested
    // artboards get no placeholder scope regardless of whether their artboard
    // contains focusables.
    rive::NestedArtboard* slotHost = nullptr;
    for (auto* host : artboard->nestedArtboards())
    {
        auto* source = host->sourceArtboard();
        REQUIRE(source != nullptr);
        if (source->name() == "Swappable1" || source->name() == "Swappable2")
        {
            CHECK(host->isArtboardDataBound() == true);
            slotHost = host;
        }
        else
        {
            CHECK(host->isArtboardDataBound() == false);
        }
    }
    REQUIRE(slotHost != nullptr);

    CHECK(stateMachine->hasFocusNodes() == true);

    // The artboard owning the currently focused element.
    auto focusedArtboardName = [&]() -> std::string {
        auto* ab = focusManager->primaryFocusImmediateArtboard();
        return ab != nullptr ? ab->name() : "<none>";
    };

    // Initial tab order follows the Main hierarchy: Rectangle (Main) -> slot
    // (Swappable1) -> StaticNestWithFocusable. StaticNestWithoutFocusable
    // contributes nothing.
    CHECK(stateMachine->focusNext() == true);
    CHECK(focusedArtboardName() == "Main");
    CHECK(stateMachine->focusNext() == true);
    CHECK(focusedArtboardName() == "Swappable1");
    CHECK(stateMachine->focusNext() == true);
    CHECK(focusedArtboardName() == "StaticNestWithFocusable");
    // Edge of the root scope clears focus.
    CHECK(stateMachine->focusNext() == false);
    CHECK(focusManager->primaryFocus() == nullptr);

    // Swap the slot to an artboard with no focusables: the slot contributes
    // no focus stop and the rest of the order is untouched.
    auto* artboardProp = vmi->propertyValue("artboardProp");
    REQUIRE(artboardProp != nullptr);
    REQUIRE(artboardProp->is<rive::ViewModelInstanceArtboard>());
    auto* vmiArtboard = artboardProp->as<rive::ViewModelInstanceArtboard>();
    auto swappable2 = file->bindableArtboardNamed("Swappable2");
    REQUIRE(swappable2 != nullptr);
    vmiArtboard->asset(swappable2);
    stateMachine->advanceAndApply(0.016f);

    CHECK(stateMachine->focusNext() == true);
    CHECK(focusedArtboardName() == "Main");
    CHECK(stateMachine->focusNext() == true);
    CHECK(focusedArtboardName() == "StaticNestWithFocusable");
    CHECK(stateMachine->focusNext() == false);

    // Focus the Main rectangle, then swap back to the focusable artboard:
    // focus held elsewhere survives the (unrelated) swap...
    CHECK(stateMachine->focusNext() == true);
    CHECK(focusedArtboardName() == "Main");
    auto heldFocus = focusManager->primaryFocus();
    auto swappable1 = file->bindableArtboardNamed("Swappable1");
    REQUIRE(swappable1 != nullptr);
    vmiArtboard->asset(swappable1);
    stateMachine->advanceAndApply(0.016f);
    CHECK(focusManager->primaryFocus() == heldFocus);
    CHECK(focusedArtboardName() == "Main");

    // ...and the swapped-in focusable takes the slot's place in the middle of
    // the tab order (its hierarchy position), not the end.
    CHECK(stateMachine->focusNext() == true);
    CHECK(focusedArtboardName() == "Swappable1");
    CHECK(stateMachine->focusNext() == true);
    CHECK(focusedArtboardName() == "StaticNestWithFocusable");
    CHECK(stateMachine->focusNext() == false);
}

TEST_CASE("Repeat focus-tree build keeps focus inside an untouched nested "
          "artboard",
          "[FocusManager]")
{
    // #4 regression: a second full buildFocusTree pass over an already-wired
    // tree (same manager) must not tear down and rebuild nested artboards that
    // did not change — doing so blurs focus resting inside them. Only the
    // non-destructive scope placement should run on the repeat pass.
    auto file = ReadRiveFile("assets/swappable_artboards_focus.riv");
    auto artboard = file->artboardNamed("Main");
    REQUIRE(artboard != nullptr);

    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);
    auto* focusManager = stateMachine->focusManager();
    REQUIRE(focusManager != nullptr);

    auto vmi = file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(vmi != nullptr);
    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.016f);
    stateMachine->advanceAndApply(0.016f);

    auto focusedArtboardName = [&]() -> std::string {
        auto* ab = focusManager->primaryFocusImmediateArtboard();
        return ab != nullptr ? ab->name() : "<none>";
    };

    // Tab into the focusable that lives inside the STATIC nested artboard.
    // Order (established by the sibling test): Main -> Swappable1 ->
    // StaticNestWithFocusable.
    CHECK(stateMachine->focusNext() == true);
    CHECK(stateMachine->focusNext() == true);
    CHECK(stateMachine->focusNext() == true);
    REQUIRE(focusedArtboardName() == "StaticNestWithFocusable");
    auto heldFocus = focusManager->primaryFocus();
    REQUIRE(heldFocus != nullptr);

    // Repeat the full build pass with the SAME manager (mirrors the host's
    // documented two-phase build, or any later focus-tree re-wire). Nothing
    // about the static nested artboard changed, so the focus resting inside it
    // must survive rather than being blurred by a needless rebuild.
    artboard->buildFocusTree(focusManager, nullptr);

    CHECK(focusManager->primaryFocus() == heldFocus);
    CHECK(focusedArtboardName() == "StaticNestWithFocusable");
}

TEST_CASE("Cross-file swaps keep slot order and share the focus manager",
          "[FocusManager]")
{
    // The slot's host, bind, and scope all live in the main file; the
    // swapped-in artboard may come from a different .riv. Loading the asset
    // twice yields two independent Files, so pulling bindable artboards from
    // the second File exercises the cross-file path.
    auto file = ReadRiveFile("assets/swappable_artboards_focus.riv");
    auto otherFile = ReadRiveFile("assets/swappable_artboards_focus.riv");

    auto artboard = file->artboardNamed("Main");
    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);
    auto* focusManager = stateMachine->focusManager();
    REQUIRE(focusManager != nullptr);

    auto vmi = file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(vmi != nullptr);
    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.016f);

    auto focusedArtboard = [&]() -> rive::Artboard* {
        return focusManager->primaryFocusImmediateArtboard();
    };
    auto focusedArtboardName = [&]() -> std::string {
        auto* ab = focusedArtboard();
        return ab != nullptr ? ab->name() : "<none>";
    };
    // The slot host's bound state machine (created by the latest swap).
    auto slotBoundStateMachine = [&]() -> rive::StateMachineInstance* {
        for (auto* host : artboard->nestedArtboards())
        {
            if (!host->isArtboardDataBound())
            {
                continue;
            }
            for (auto* animation : host->nestedAnimations())
            {
                if (animation->is<rive::NestedStateMachine>())
                {
                    return animation->as<rive::NestedStateMachine>()
                        ->stateMachineInstance();
                }
            }
        }
        return nullptr;
    };

    auto* artboardProp = vmi->propertyValue("artboardProp");
    REQUIRE(artboardProp != nullptr);
    REQUIRE(artboardProp->is<rive::ViewModelInstanceArtboard>());
    auto* vmiArtboard = artboardProp->as<rive::ViewModelInstanceArtboard>();

    // Swap in a LEAF artboard (one focusable, no nested hosts) from the
    // other file.
    auto foreignSwappable = otherFile->bindableArtboardNamed("Swappable1");
    REQUIRE(foreignSwappable != nullptr);
    vmiArtboard->asset(foreignSwappable);
    stateMachine->advanceAndApply(0.016f);

    // The foreign artboard's focus node sits at the slot's hierarchy
    // position, exactly like a same-file swap.
    CHECK(stateMachine->focusNext() == true);
    CHECK(focusedArtboardName() == "Main");
    CHECK(stateMachine->focusNext() == true);
    CHECK(focusedArtboardName() == "Swappable1");
    CHECK(stateMachine->focusNext() == true);
    CHECK(focusedArtboardName() == "StaticNestWithFocusable");
    CHECK(stateMachine->focusNext() == false);

    // The swapped-in artboard's own state machine must share the parent
    // FocusManager, so its focus/keyboard listener groups act on the same
    // focus state that Tab traversal uses.
    auto* leafSmi = slotBoundStateMachine();
    REQUIRE(leafSmi != nullptr);
    CHECK(leafSmi->focusManager() == focusManager);
}

TEST_CASE("Unresolvable artboard swap leaves focus and tab order untouched",
          "[FocusManager]")
{
    auto file = ReadRiveFile("assets/swappable_artboards_focus.riv");
    auto artboard = file->artboardNamed("Main");
    REQUIRE(artboard != nullptr);

    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);
    auto* focusManager = stateMachine->focusManager();
    REQUIRE(focusManager != nullptr);

    auto vmi = file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(vmi != nullptr);
    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.016f);
    stateMachine->advanceAndApply(0.016f);

    auto focusedArtboardName = [&]() -> std::string {
        auto* ab = focusManager->primaryFocusImmediateArtboard();
        return ab != nullptr ? ab->name() : "<none>";
    };

    // Default order (per the sibling test): Main -> Swappable1 ->
    // StaticNestWithFocusable. Rest focus on Main's Rectangle and hold the rcp.
    CHECK(stateMachine->focusNext() == true);
    REQUIRE(focusedArtboardName() == "Main");
    auto heldFocus = focusManager->primaryFocus();
    REQUIRE(heldFocus != nullptr);

    // Drive the slot's VM artboard property into the UNRESOLVABLE state: no
    // bindable asset and a bogus (non -1) id that matches no artboard. This is
    // distinct from an explicit clear (asset null AND propertyValue == -1), so
    // updateArtboard must return early and leave the on-screen slot alone.
    auto* artboardProp = vmi->propertyValue("artboardProp");
    REQUIRE(artboardProp != nullptr);
    REQUIRE(artboardProp->is<rive::ViewModelInstanceArtboard>());
    auto* vmiArtboard = artboardProp->as<rive::ViewModelInstanceArtboard>();
    vmiArtboard->propertyValue(9999u);
    REQUIRE(vmiArtboard->asset() == nullptr);
    REQUIRE(vmiArtboard->propertyValue() != static_cast<uint32_t>(-1));
    stateMachine->advanceAndApply(0.016f);

    // Focus held on Main survives the failed swap...
    CHECK(focusManager->primaryFocus() == heldFocus);
    CHECK(focusedArtboardName() == "Main");

    // ...and the outgoing Swappable1 kept its focus nodes, so the full tab
    // order is unchanged: Main -> Swappable1 -> StaticNestWithFocusable.
    CHECK(stateMachine->focusNext() == true);
    CHECK(focusedArtboardName() == "Swappable1");
    CHECK(stateMachine->focusNext() == true);
    CHECK(focusedArtboardName() == "StaticNestWithFocusable");
    CHECK(stateMachine->focusNext() == false);
}

TEST_CASE("Initially-empty bindable slot keeps its authored tab position on "
          "first swap",
          "[FocusManager]")
{
    auto file = ReadRiveFile("assets/swappable_artboards_focus.riv");
    auto artboard = file->artboardNamed("Main");
    REQUIRE(artboard != nullptr);

    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);
    auto* focusManager = stateMachine->focusManager();
    REQUIRE(focusManager != nullptr);

    auto vmi = file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(vmi != nullptr);

    // Clear the slot to explicit null (asset null, propertyValue -1) BEFORE the
    // first advance, so the slot is empty when the focus tree is first built.
    auto* artboardProp = vmi->propertyValue("artboardProp");
    REQUIRE(artboardProp != nullptr);
    REQUIRE(artboardProp->is<rive::ViewModelInstanceArtboard>());
    auto* vmiArtboard = artboardProp->as<rive::ViewModelInstanceArtboard>();
    vmiArtboard->asset(nullptr);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.016f);
    stateMachine->advanceAndApply(0.016f);

    auto focusedArtboardName = [&]() -> std::string {
        auto* ab = focusManager->primaryFocusImmediateArtboard();
        return ab != nullptr ? ab->name() : "<none>";
    };

    // The empty slot's scope holds its place but offers no focus stop, so the
    // order skips it: Main -> StaticNestWithFocusable.
    CHECK(stateMachine->focusNext() == true);
    CHECK(focusedArtboardName() == "Main");
    CHECK(stateMachine->focusNext() == true);
    CHECK(focusedArtboardName() == "StaticNestWithFocusable");
    CHECK(stateMachine->focusNext() == false);

    // Swap Swappable1 in for the first time: it must build under the scope the
    // empty-slot build pass already placed, entering the MIDDLE of the tab
    // order (Main -> Swappable1 -> StaticNestWithFocusable), not the end.
    auto swappable1 = file->bindableArtboardNamed("Swappable1");
    REQUIRE(swappable1 != nullptr);
    vmiArtboard->asset(swappable1);
    stateMachine->advanceAndApply(0.016f);
    CHECK(stateMachine->focusNext() == true);
    CHECK(focusedArtboardName() == "Main");
    CHECK(stateMachine->focusNext() == true);
    CHECK(focusedArtboardName() == "Swappable1");
    CHECK(stateMachine->focusNext() == true);
    CHECK(focusedArtboardName() == "StaticNestWithFocusable");
    CHECK(stateMachine->focusNext() == false);
}

TEST_CASE("Focus bounds track a nested artboard host that moves",
          "[FocusManager]")
{
    // The focusable's own layout geometry never changes here -- only the
    // NestedArtboard hosting it slides. The bounds cached on the FocusNode
    // during the update pass are written from that unchanged geometry, so they
    // describe where the element used to be. FocusData::worldBounds recomputes
    // through the root transform at call time, which is what keeps a focus
    // bracket attached to the element as its host animates.
    auto file = ReadRiveFile("assets/focus_bounds_moving_host.riv");
    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);

    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);

    // Settle without consuming time so the host is still at its authored
    // position when the first bounds are read. The scene focuses itself: the
    // hosted artboard's entry state fires an event at start and its own
    // listener performs the FocusActionTarget.
    stateMachine->advanceAndApply(0.0f);
    stateMachine->advanceAndApply(0.0f);

    auto* focusManager = stateMachine->focusManager();
    REQUIRE(focusManager != nullptr);
    REQUIRE(focusManager->primaryFocus() != nullptr);

    rive::AABB atRest;
    REQUIRE(focusManager->primaryFocusBounds(atRest) == true);

    // Host authored at (100, 100); the target layout sits at its artboard's
    // origin, since yoga owns a LayoutComponent's position and ignores the
    // node's own x/y. Anchoring the start makes a drift in absolute placement
    // visible, not just a wrong delta.
    CHECK(atRest.minX == Approx(100.0f).margin(0.5f));
    CHECK(atRest.minY == Approx(100.0f).margin(0.5f));
    CHECK(atRest.width() == Approx(120.0f).margin(0.5f));
    CHECK(atRest.height() == Approx(80.0f).margin(0.5f));

    // Run the slide halfway: the full travel is 200pt right and 60pt down over
    // 60 frames, linearly. Stop at 30 rather than 60 -- the animation
    // ping-pongs, so the far end is a turning point and float accumulation
    // could land either side of it.
    for (int i = 0; i < 30; i++)
    {
        stateMachine->advanceAndApply(1.0f / 60.0f);
    }

    rive::AABB moved;
    REQUIRE(focusManager->primaryFocusBounds(moved) == true);

    CHECK(moved.minX == Approx(atRest.minX + 100.0f).margin(0.5f));
    CHECK(moved.minY == Approx(atRest.minY + 30.0f).margin(0.5f));
    // The element itself did not resize; only its host moved.
    CHECK(moved.width() == Approx(atRest.width()).margin(0.5f));
    CHECK(moved.height() == Approx(atRest.height()).margin(0.5f));
}

TEST_CASE("Focus change with gamepad navigation", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/gamepad_inputs_test.riv", &silver);

    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);

    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);

    auto vmi = file->createViewModelInstance(artboard.get());

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.0f);

    // This file nests two containers above three leaves, and all five are
    // stops — a container is no longer a pass-through just because something
    // focusable lives under it. That is what the traversal below steps
    // through, and why this baseline moved.
    {
        const auto& roots = stateMachine->focusManager()->rootNodes();
        REQUIRE(roots.size() == 1);
        REQUIRE(roots[0]->children().size() == 1);
        REQUIRE(roots[0]->children()[0]->children().size() == 3);
    }

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());
    silver.addFrame();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    // An update is only accepted for a device the state machine has already
    // seen connected, so announce the pad before pressing anything.
    constexpr int32_t kDeviceId = 0;
    rive::GamepadWire connect;
    connect.connected(kDeviceId);
    REQUIRE(stateMachine->submitGamepadsFromBuffer(connect.buf.data(),
                                                   connect.buf.size()));

    silver.addFrame();
    rive::GamepadWire press;
    press.button(kDeviceId, rive::StandardGamepadButton::rightShoulder, 1.0f);
    REQUIRE(stateMachine->submitGamepadsFromBuffer(press.buf.data(),
                                                   press.buf.size()));
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    silver.addFrame();
    rive::GamepadWire release;
    release.button(kDeviceId, rive::StandardGamepadButton::rightShoulder, 0.0f);
    REQUIRE(stateMachine->submitGamepadsFromBuffer(release.buf.data(),
                                                   release.buf.size()));
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("gamepad_inputs_test"));
}

TEST_CASE("Uncollapse and focus element on the same action", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/gamepad_inputs_test.riv", &silver);

    auto artboard = file->artboardNamed("UncollapsedParent");
    REQUIRE(artboard != nullptr);

    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);

    auto vmi = file->createDefaultViewModelInstance(artboard.get());

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.0f);
    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    silver.addFrame();
    stateMachine->pointerDown(rive::Vec2D(475.0f, 475.0f));
    stateMachine->pointerUp(rive::Vec2D(475.0f, 475.0f));
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    silver.addFrame();
    stateMachine->pointerDown(rive::Vec2D(475.0f, 475.0f));
    stateMachine->pointerUp(rive::Vec2D(475.0f, 475.0f));
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("gamepad_inputs_test-collapsing"));
}
