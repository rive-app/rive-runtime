/*
 * Copyright 2026 Rive
 */

#include <rive/animation/animation_state.hpp>
#include <rive/animation/listener_action.hpp>
#include <rive/animation/listener_bool_change.hpp>
#include <rive/animation/listener_fire_event.hpp>
#include <rive/animation/state_machine_fire_action.hpp>
#include <rive/animation/state_machine_listener_single.hpp>
#include <rive/animation/state_transition.hpp>
#include <rive/generated/animation/state_machine_layer_component_base.hpp>
#include <rive/generated/animation/state_machine_listener_base.hpp>
#include <rive/importers/import_stack.hpp>
#include <rive/importers/state_machine_layer_component_importer.hpp>
#include <rive/importers/state_machine_listener_importer.hpp>

#include <catch.hpp>

using namespace rive;

TEST_CASE("ListenerAction::parentKind decodes flags bits 1-2",
          "[listener_action_flags]")
{
    ListenerBoolChange action;

    // Bits 1-2 = 0 -> Listener (also the default for pre-flag files).
    action.flags(0u);
    CHECK(action.parentKind() == ListenerAction::ParentKind::Listener);

    // Bits 1-2 = 1 -> Transition.
    action.flags(1u << 1);
    CHECK(action.parentKind() == ListenerAction::ParentKind::Transition);

    // Bits 1-2 = 2 -> State.
    action.flags(2u << 1);
    CHECK(action.parentKind() == ListenerAction::ParentKind::State);

    // Bits 1-2 = 3 is reserved; the getter clamps back to the Listener
    // default so unexpected values don't misroute at import time.
    action.flags(3u << 1);
    CHECK(action.parentKind() == ListenerAction::ParentKind::Listener);
}

TEST_CASE("ListenerAction flag fields are independent",
          "[listener_action_flags]")
{
    ListenerBoolChange action;

    // Setting bit 0 alone (onExit) must leave parentKind at its default.
    action.flags(1u);
    CHECK(action.parentKind() == ListenerAction::ParentKind::Listener);
    CHECK(action.matchesScheduledOccurrence(StateMachineFireOccurance::atEnd));
    CHECK_FALSE(
        action.matchesScheduledOccurrence(StateMachineFireOccurance::atStart));

    // Setting only bits 1-2 (parentKind=State) must leave the occurance
    // bit as atStart.
    action.flags(2u << 1);
    CHECK(action.parentKind() == ListenerAction::ParentKind::State);
    CHECK(
        action.matchesScheduledOccurrence(StateMachineFireOccurance::atStart));
    CHECK_FALSE(
        action.matchesScheduledOccurrence(StateMachineFireOccurance::atEnd));

    // Both fields set together must decode independently.
    action.flags(1u | (1u << 1)); // onExit + Transition
    CHECK(action.parentKind() == ListenerAction::ParentKind::Transition);
    CHECK(action.matchesScheduledOccurrence(StateMachineFireOccurance::atEnd));
}

TEST_CASE("ListenerAction::matchesScheduledOccurrence covers both phases",
          "[listener_action_flags]")
{
    ListenerBoolChange action;

    action.flags(0u);
    CHECK(
        action.matchesScheduledOccurrence(StateMachineFireOccurance::atStart));
    CHECK_FALSE(
        action.matchesScheduledOccurrence(StateMachineFireOccurance::atEnd));

    action.flags(1u);
    CHECK(action.matchesScheduledOccurrence(StateMachineFireOccurance::atEnd));
    CHECK_FALSE(
        action.matchesScheduledOccurrence(StateMachineFireOccurance::atStart));
}

namespace
{
// Pushes both importers onto the stack — mirrors the mid-stream state that
// caused the original bug (a stale StateMachineListener left in m_latests
// alongside a freshly pushed StateMachineLayerComponent). Each import test
// constructs this setup so the parent-kind flag is the only thing telling
// import() which branch to take.
struct RoutingFixture
{
    StateMachineListenerSingle listener;
    StateTransition transition;
    AnimationState state;
    ImportStack stack;

    RoutingFixture()
    {
        stack.makeLatest(StateMachineListenerBase::typeKey,
                         std::unique_ptr<ImportStackObject>(
                             new StateMachineListenerImporter(&listener)));
        stack.makeLatest(
            StateMachineLayerComponentBase::typeKey,
            std::unique_ptr<ImportStackObject>(
                new StateMachineLayerComponentImporter(&transition)));
    }
};

uint32_t parentKindFlags(ListenerAction::ParentKind kind)
{
    return static_cast<uint32_t>(kind) << 1;
}
} // namespace

TEST_CASE("ListenerAction::import routes Transition parent to layer importer "
          "even when a listener importer is also on the stack",
          "[listener_action_flags]")
{
    RoutingFixture f;
    auto* action = new ListenerFireEvent();
    action->flags(parentKindFlags(ListenerAction::ParentKind::Transition));

    REQUIRE(action->import(f.stack) == StatusCode::Ok);

    CHECK(f.transition.listenerActions().size() == 1);
    CHECK(f.transition.listenerActions()[0].get() == action);
    CHECK(f.listener.actionCount() == 0);
}

TEST_CASE("ListenerAction::import routes State parent to layer importer "
          "even when a listener importer is also on the stack",
          "[listener_action_flags]")
{
    RoutingFixture f;
    // Swap in a LayerState as the active layer-component importer.
    f.stack.makeLatest(StateMachineLayerComponentBase::typeKey,
                       std::unique_ptr<ImportStackObject>(
                           new StateMachineLayerComponentImporter(&f.state)));

    auto* action = new ListenerFireEvent();
    action->flags(parentKindFlags(ListenerAction::ParentKind::State));

    REQUIRE(action->import(f.stack) == StatusCode::Ok);

    CHECK(f.state.listenerActions().size() == 1);
    CHECK(f.state.listenerActions()[0].get() == action);
    CHECK(f.transition.listenerActions().empty());
    CHECK(f.listener.actionCount() == 0);
}

TEST_CASE("ListenerAction::import routes Listener parent to the listener "
          "importer when both are on the stack",
          "[listener_action_flags]")
{
    RoutingFixture f;
    auto* action = new ListenerFireEvent();
    action->flags(parentKindFlags(ListenerAction::ParentKind::Listener));

    REQUIRE(action->import(f.stack) == StatusCode::Ok);

    CHECK(f.listener.actionCount() == 1);
    CHECK(f.transition.listenerActions().empty());
}

TEST_CASE("ListenerAction::import fails when the Listener parent kind has "
          "no listener importer on the stack",
          "[listener_action_flags]")
{
    StateTransition transition;
    ImportStack stack;
    stack.makeLatest(StateMachineLayerComponentBase::typeKey,
                     std::unique_ptr<ImportStackObject>(
                         new StateMachineLayerComponentImporter(&transition)));

    // Construct on the heap because import() is expected to take ownership
    // on success via the addAction(std::unique_ptr<...>) call. On failure
    // ownership stays here so we can clean up.
    std::unique_ptr<ListenerFireEvent> action(new ListenerFireEvent());
    action->flags(0u); // Listener is the default.

    REQUIRE(action->import(stack) == StatusCode::MissingObject);
    CHECK(transition.listenerActions().empty());
}
