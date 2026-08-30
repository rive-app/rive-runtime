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

TEST_CASE("FocusManager setFocus on a scope descends to first leaf",
          "[FocusManager]")
{
    FocusManager manager;
    auto scope = make_rcp<FocusNode>();
    auto leaf1 = make_rcp<FocusNode>();
    auto leaf2 = make_rcp<FocusNode>();

    manager.addChild(nullptr, scope);
    manager.addChild(scope, leaf1);
    manager.addChild(scope, leaf2);

    // Focusing the scope resolves to its first eligible leaf.
    manager.setFocus(scope);
    CHECK(manager.primaryFocus() == leaf1);
}

TEST_CASE("FocusManager setFocus on a scope descends depth-first",
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

    // Depth-first: first leaf is the leaf nested under the first child (row).
    manager.setFocus(scope);
    CHECK(manager.primaryFocus() == leaf);
}

TEST_CASE("FocusManager setFocus on a scope with no eligible leaf falls back",
          "[FocusManager]")
{
    FocusManager manager;
    auto scope = make_rcp<FocusNode>();
    auto child = make_rcp<FocusNode>();
    // Child cannot be traversed, so the scope has no eligible leaf to descend
    // to. The scope itself remains the focus target (preserves prior behavior).
    child->canTraverse(false);

    manager.addChild(nullptr, scope);
    manager.addChild(scope, child);

    manager.setFocus(scope);
    CHECK(manager.primaryFocus() == scope);
}

TEST_CASE("FocusManager setFocus on an ineligible scope is a no-op",
          "[FocusManager]")
{
    FocusManager manager;
    auto scope = make_rcp<FocusNode>();
    auto leaf = make_rcp<FocusNode>();
    // The requested target itself cannot be focused. Descent must not reach an
    // eligible descendant — focus stays unchanged (no-op), matching the prior
    // early-return guard behavior.
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

    // Directly focusing a leaf still focuses that exact leaf (no-op descent).
    manager.setFocus(leaf2);
    CHECK(manager.primaryFocus() == leaf2);
}

TEST_CASE("FocusManager Tab after focusing a scope traverses leaf siblings",
          "[FocusManager]")
{
    FocusManager manager;
    auto scope = make_rcp<FocusNode>();
    auto leaf1 = make_rcp<FocusNode>();
    auto leaf2 = make_rcp<FocusNode>();

    manager.addChild(nullptr, scope);
    manager.addChild(scope, leaf1);
    manager.addChild(scope, leaf2);

    // Focusing the scope lands on the first leaf; Tab then advances to the
    // scope's next leaf rather than skipping the scope's children.
    manager.setFocus(scope);
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

    manager.setFocus(parent); // descends to the first leaf
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

    manager.setFocus(parent);
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

    manager.setFocus(grand);
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

    manager.setFocus(parent);
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

    manager.setFocus(rootA);
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

// =============================================================================
// Re-descending focus when a hidden child reappears
// =============================================================================

TEST_CASE("FocusManager descends focus to a child that becomes eligible",
          "[FocusManager]")
{
    FocusManager manager;
    MockFocusable parentFocusable, childFocusable;
    auto parent = make_rcp<FocusNode>(&parentFocusable);
    auto child = make_rcp<FocusNode>(&childFocusable);
    manager.addChild(nullptr, parent);
    manager.addChild(parent, child);

    // With the child hidden, the parent is a leaf and keeps focus itself.
    childFocusable.eligible = false;
    manager.setFocus(parent);
    REQUIRE(manager.primaryFocus() == parent);
    CHECK(parentFocusable.focusedCount == 1);

    // Still a leaf, so an update pass changes nothing.
    manager.descendFocusToLeaf(nullptr);
    CHECK(manager.primaryFocus() == parent);
    CHECK(childFocusable.focusedCount == 0);

    // The child becomes visible: the parent has silently become a scope, and
    // focus has to follow to the leaf Tab would have picked.
    childFocusable.eligible = true;
    manager.descendFocusToLeaf(nullptr);
    CHECK(manager.primaryFocus() == child);
    CHECK(childFocusable.focusedCount == 1);
    // The parent stays on the focus path, so it is neither blurred nor
    // re-focused.
    CHECK(parentFocusable.blurredCount == 0);
    CHECK(parentFocusable.focusedCount == 1);

    // Focus rests on a leaf again; further passes are no-ops.
    manager.descendFocusToLeaf(nullptr);
    CHECK(manager.primaryFocus() == child);
    CHECK(childFocusable.focusedCount == 1);
}

TEST_CASE("FocusManager only descends focus for the root that just updated",
          "[FocusManager]")
{
    NoOpFactory factory;
    Artboard artboardA(&factory);
    Artboard artboardB(&factory);

    FocusManager manager;
    MockFocusable parentFocusable, childFocusable;
    parentFocusable.artboard = &artboardB;
    childFocusable.artboard = &artboardB;
    auto parent = make_rcp<FocusNode>(&parentFocusable);
    auto child = make_rcp<FocusNode>(&childFocusable);
    manager.addChild(nullptr, parent);
    manager.addChild(parent, child);

    childFocusable.eligible = false;
    manager.setFocus(parent);
    REQUIRE(manager.primaryFocus() == parent);

    childFocusable.eligible = true;
    // Root A advancing must not touch a target living in root B: only B's own
    // update pass has refreshed what eligibility reads.
    manager.descendFocusToLeaf(&artboardA);
    CHECK(manager.primaryFocus() == parent);

    // B's pass does the descent.
    manager.descendFocusToLeaf(&artboardB);
    CHECK(manager.primaryFocus() == child);
}

TEST_CASE("FocusManager scopes descent by where focus would land",
          "[FocusManager]")
{
    NoOpFactory factory;
    Artboard artboardA(&factory);
    Artboard artboardB(&factory);

    FocusManager manager;
    // A host-created parent: no artboard backs it, so no root owns it. Its
    // child does live in a real artboard, which is the eligibility the
    // descent would be acting on.
    MockFocusable parentFocusable, childFocusable;
    childFocusable.artboard = &artboardB;
    auto parent = make_rcp<FocusNode>(&parentFocusable);
    auto child = make_rcp<FocusNode>(&childFocusable);
    manager.addChild(nullptr, parent);
    manager.addChild(parent, child);

    childFocusable.eligible = false;
    manager.setFocus(parent);
    REQUIRE(manager.primaryFocus() == parent);

    childFocusable.eligible = true;
    // Scoping on the unattributable parent would let root A descend into
    // root B's child on B's stale state; scoping on the destination defers.
    manager.descendFocusToLeaf(&artboardA);
    CHECK(manager.primaryFocus() == parent);

    // And it still descends — B's own pass claims it, rather than the
    // unattributable target being deferred forever.
    manager.descendFocusToLeaf(&artboardB);
    CHECK(manager.primaryFocus() == child);
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

    manager.setFocus(parent);
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

    CHECK(stateMachine->focusNext() == false);
    // There's only one focus node in the main artboard, go back to that last
    // node
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
