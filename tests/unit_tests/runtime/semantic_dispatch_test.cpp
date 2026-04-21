/*
 * Copyright 2026 Rive
 */

// Unit tests for the dispatch path that
// StateMachineInstance::fireSemanticAction depends on:
//   1. SemanticData's listener subscription / dispatch.
//   2. SemanticManager::nodeById + SemanticNode::semanticData() back-pointer
//      routing, which is how an action id resolves to the target SemanticData
//      without walking any scene graph.
//
// The full StateMachineInstance flow (advance→processSemanticEvents→
// listener->performChanges) is not reached here because it requires loading a
// .riv with an authored semantic listener; those paths are covered by the
// Dart integration tests under packages/rive_native/test/semantic/.

#include <rive/animation/semantic_listener_group.hpp>
#include <rive/refcnt.hpp>
#include <rive/semantic/semantic_data.hpp>
#include <rive/semantic/semantic_listener.hpp>
#include <rive/semantic/semantic_manager.hpp>
#include <rive/semantic/semantic_node.hpp>
#include <rive/semantic/semantic_role.hpp>
#include <rive/semantic/semantic_state.hpp>
#include <catch.hpp>

using namespace rive;

namespace
{
class MockSemanticListener : public SemanticListener
{
public:
    int tapCount = 0;
    int increaseCount = 0;
    int decreaseCount = 0;

    void onSemanticTap() override { tapCount++; }
    void onSemanticIncrease() override { increaseCount++; }
    void onSemanticDecrease() override { decreaseCount++; }
};
} // namespace

// ---------------------------------------------------------------------------
// SemanticData listener subscription & dispatch.
// ---------------------------------------------------------------------------

TEST_CASE("fireSemanticTap invokes onSemanticTap on all registered listeners",
          "[semantics][dispatch]")
{
    SemanticData sd;
    MockSemanticListener a;
    MockSemanticListener b;
    sd.addSemanticListener(&a);
    sd.addSemanticListener(&b);

    sd.fireSemanticTap();

    CHECK(a.tapCount == 1);
    CHECK(b.tapCount == 1);
    CHECK(a.increaseCount == 0);
    CHECK(a.decreaseCount == 0);
}

TEST_CASE("fireSemanticIncrease only invokes onSemanticIncrease",
          "[semantics][dispatch]")
{
    SemanticData sd;
    MockSemanticListener listener;
    sd.addSemanticListener(&listener);

    sd.fireSemanticIncrease();

    CHECK(listener.increaseCount == 1);
    CHECK(listener.tapCount == 0);
    CHECK(listener.decreaseCount == 0);
}

TEST_CASE("fireSemanticDecrease only invokes onSemanticDecrease",
          "[semantics][dispatch]")
{
    SemanticData sd;
    MockSemanticListener listener;
    sd.addSemanticListener(&listener);

    sd.fireSemanticDecrease();

    CHECK(listener.decreaseCount == 1);
    CHECK(listener.tapCount == 0);
    CHECK(listener.increaseCount == 0);
}

TEST_CASE(
    "removeSemanticListener stops future dispatches from reaching the listener",
    "[semantics][dispatch]")
{
    SemanticData sd;
    MockSemanticListener listener;
    sd.addSemanticListener(&listener);

    sd.fireSemanticTap();
    CHECK(listener.tapCount == 1);

    sd.removeSemanticListener(&listener);
    sd.fireSemanticTap();
    CHECK(listener.tapCount == 1); // no change
}

TEST_CASE("removeSemanticListener on an unregistered pointer is a no-op",
          "[semantics][dispatch]")
{
    SemanticData sd;
    MockSemanticListener registered;
    MockSemanticListener ghost;
    sd.addSemanticListener(&registered);

    // Never added; removing it must not affect the real subscription.
    sd.removeSemanticListener(&ghost);

    sd.fireSemanticTap();
    CHECK(registered.tapCount == 1);
}

TEST_CASE("fireSemantic* with no listeners is a silent no-op",
          "[semantics][dispatch]")
{
    SemanticData sd;
    sd.fireSemanticTap();
    sd.fireSemanticIncrease();
    sd.fireSemanticDecrease();
    SUCCEED("no crash, no observable effect");
}

// ---------------------------------------------------------------------------
// Routing from SemanticManager through the SemanticNode::semanticData()
// back-pointer to the owning SemanticData. This is exactly what
// StateMachineInstance::fireSemanticAction does — isolating the routing
// lets us cover it without a full state-machine harness.
// ---------------------------------------------------------------------------

TEST_CASE("SemanticNode::semanticData is the back-pointer set at node creation",
          "[semantics][dispatch]")
{
    SemanticData sd;
    auto node = sd.semanticNode();
    REQUIRE(node != nullptr);
    CHECK(node->semanticData() == &sd);
}

TEST_CASE("manager.nodeById + node->semanticData routes dispatch to the SD",
          "[semantics][dispatch]")
{
    SemanticManager mgr;
    SemanticData sd;
    sd.role(static_cast<uint32_t>(SemanticRole::button));
    sd.label("Go");

    auto node = sd.semanticNode();
    mgr.addChild(nullptr, node);
    const uint32_t id = node->id();
    REQUIRE(id != 0);

    MockSemanticListener listener;
    sd.addSemanticListener(&listener);

    auto* found = mgr.nodeById(id);
    REQUIRE(found != nullptr);
    auto* ownerSd = found->semanticData();
    REQUIRE(ownerSd == &sd);

    ownerSd->fireSemanticTap();
    CHECK(listener.tapCount == 1);

    // Remove the listener before the SD destructs, mirroring what
    // SemanticListenerGroup does in its own destructor.
    sd.removeSemanticListener(&listener);
}

TEST_CASE("manager.nodeById returns null for an unknown id",
          "[semantics][dispatch]")
{
    SemanticManager mgr;
    SemanticData sd;
    sd.role(static_cast<uint32_t>(SemanticRole::button));
    auto node = sd.semanticNode();
    mgr.addChild(nullptr, node);

    CHECK(mgr.nodeById(9999) == nullptr);
}

TEST_CASE("boundary node has no owning SemanticData", "[semantics][dispatch]")
{
    SemanticManager mgr;
    auto boundary = rcp<SemanticNode>(new SemanticNode());
    boundary->isBoundaryNode(true);
    mgr.addChild(nullptr, boundary);

    auto* found = mgr.nodeById(boundary->id());
    REQUIRE(found != nullptr);
    CHECK(found->isBoundaryNode());
    CHECK(found->semanticData() == nullptr);
}

TEST_CASE("removing an SD from the manager drops its id from the index",
          "[semantics][dispatch]")
{
    SemanticManager mgr;
    SemanticData sd;
    sd.role(static_cast<uint32_t>(SemanticRole::button));
    auto node = sd.semanticNode();
    mgr.addChild(nullptr, node);
    const uint32_t id = node->id();

    REQUIRE(mgr.nodeById(id) == node.get());

    mgr.removeChild(node);
    CHECK(mgr.nodeById(id) == nullptr);
}

// ---------------------------------------------------------------------------
// Runtime-driven state modification.
// ---------------------------------------------------------------------------

TEST_CASE("setFocusedState toggles the Focused bit and preserves siblings",
          "[semantics][dispatch]")
{
    SemanticData sd;
    auto node = sd.semanticNode();

    const uint32_t initial = static_cast<uint32_t>(SemanticState::Selected) |
                             static_cast<uint32_t>(SemanticState::Expanded);
    node->stateFlags(initial);

    sd.setFocusedState(true);
    CHECK(hasSemanticState(node->stateFlags(), SemanticState::Focused));
    CHECK(hasSemanticState(node->stateFlags(), SemanticState::Selected));
    CHECK(hasSemanticState(node->stateFlags(), SemanticState::Expanded));

    sd.setFocusedState(false);
    CHECK_FALSE(hasSemanticState(node->stateFlags(), SemanticState::Focused));
    CHECK(hasSemanticState(node->stateFlags(), SemanticState::Selected));
    CHECK(hasSemanticState(node->stateFlags(), SemanticState::Expanded));
}

TEST_CASE("setFocusedState is a no-op before semanticNode() has been created",
          "[semantics][dispatch]")
{
    SemanticData sd;
    CHECK_FALSE(sd.hasSemanticNode());

    // Must not crash or lazily create a node — the Focused bit is
    // runtime-driven and meaningless without a node.
    sd.setFocusedState(true);
    sd.setFocusedState(false);

    CHECK_FALSE(sd.hasSemanticNode());
}

// ---------------------------------------------------------------------------
// SemanticManager::requestFocus — negative paths. The positive path (a SD
// with a sibling FocusData and a real FocusManager) requires a fixture
// authored with focus trait; none of the current fixtures exercise it
// directly, so the happy case is deferred to future fixture work.
// ---------------------------------------------------------------------------

TEST_CASE("requestFocus with an unknown id returns false",
          "[semantics][dispatch]")
{
    SemanticManager mgr;
    CHECK_FALSE(mgr.requestFocus(42));
}

TEST_CASE("requestFocus on a registered node with no coreOwner returns false",
          "[semantics][dispatch]")
{
    SemanticManager mgr;
    auto node = rcp<SemanticNode>(new SemanticNode());
    mgr.addChild(nullptr, node);
    REQUIRE(node->coreOwner() == nullptr);

    CHECK_FALSE(mgr.requestFocus(node->id()));
}

TEST_CASE("requestFocus on a boundary node returns false",
          "[semantics][dispatch]")
{
    SemanticManager mgr;
    auto boundary = rcp<SemanticNode>(new SemanticNode());
    boundary->isBoundaryNode(true);
    mgr.addChild(nullptr, boundary);

    // Boundary nodes may or may not have a Node coreOwner, but they have no
    // SD child either way → no focusable association.
    CHECK_FALSE(mgr.requestFocus(boundary->id()));
}
