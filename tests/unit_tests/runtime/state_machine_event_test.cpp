#include "rive/core/binary_reader.hpp"
#include "rive/file.hpp"
#include "rive/event.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/animation/state_machine_bool.hpp"
#include "rive/animation/state_machine_layer.hpp"
#include "rive/animation/state_machine_listener.hpp"
#include "rive/animation/animation_state.hpp"
#include "rive/animation/entry_state.hpp"
#include "rive/animation/state_transition.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/animation/state_machine_input_instance.hpp"
#include "rive/animation/blend_state_1d.hpp"
#include "rive/animation/blend_animation_1d.hpp"
#include "rive/animation/blend_state_direct.hpp"
#include "rive/animation/blend_state_transition.hpp"
#include "rive/animation/listener_input_change.hpp"
#include "rive/animation/listener_fire_event.hpp"
#include "rive/animation/nested_state_machine.hpp"
#include "rive/animation/entry_state.hpp"
#include "rive/node.hpp"
#include "catch.hpp"
#include "rive_file_reader.hpp"
#include "utils/serializing_factory.hpp"
#include <cstdio>

using namespace rive;

TEST_CASE("file with state machine listeners be read", "[file]")
{
    auto file = ReadRiveFile("assets/bullet_man.riv");

    auto artboard = file->artboard("Bullet Man");
    REQUIRE(artboard != nullptr);
    REQUIRE(artboard->stateMachineCount() == 1);

    auto stateMachine = artboard->stateMachine(0);
    REQUIRE(stateMachine != nullptr);

    REQUIRE(stateMachine->listenerCount() == 3);
    REQUIRE(stateMachine->inputCount() == 4);

    // Expect each of the three listeners to have one input change each.
    auto listener1 = stateMachine->listener(0);
    auto target1 = artboard->resolve(listener1->targetId());
    REQUIRE(target1->is<Node>());
    REQUIRE(target1->as<Node>()->name() == "HandWickHit");
    REQUIRE(listener1->actionCount() == 1);
    auto inputChange1 = listener1->action(0);
    REQUIRE(inputChange1 != nullptr);
    REQUIRE(inputChange1->is<ListenerInputChange>());
    REQUIRE(inputChange1->as<ListenerInputChange>()->inputId() == 0);

    auto listener2 = stateMachine->listener(1);
    auto target2 = artboard->resolve(listener2->targetId());
    REQUIRE(target2->is<Node>());
    REQUIRE(target2->as<Node>()->name() == "HandCannonHit");
    REQUIRE(listener2->actionCount() == 1);
    auto inputChange2 = listener2->action(0);
    REQUIRE(inputChange2 != nullptr);
    REQUIRE(inputChange2->is<ListenerInputChange>());
    REQUIRE(inputChange2->as<ListenerInputChange>()->inputId() == 1);

    auto listener3 = stateMachine->listener(2);
    auto target3 = artboard->resolve(listener3->targetId());
    REQUIRE(target3->is<Node>());
    REQUIRE(target3->as<Node>()->name() == "HandHelmetHit");
    REQUIRE(listener3->actionCount() == 1);
    auto inputChange3 = listener3->action(0);
    REQUIRE(inputChange3 != nullptr);
    REQUIRE(inputChange3->is<ListenerInputChange>());
    REQUIRE(inputChange3->as<ListenerInputChange>()->inputId() == 2);
}

TEST_CASE("hit testing via a state machine works", "[file]")
{
    auto file = ReadRiveFile("assets/bullet_man.riv");

    auto artboard = file->artboard("Bullet Man")->instance();
    REQUIRE(artboard != nullptr);
    REQUIRE(artboard->stateMachineCount() == 1);

    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);
    // Advance artboard once so design time state is effectively in the
    // transforms.
    artboard->advance(0.0f);
    stateMachine->advance(0.0f);
    // Don't advance artboard again after applying state machine or our
    // pointerDown will be off. The coordinates used in this test were from the
    // design-time state.

    auto trigger = stateMachine->getTrigger("Light");
    REQUIRE(trigger != nullptr);
    stateMachine->pointerDown(Vec2D(71.0f, 263.0f));

    REQUIRE(trigger->didFire());
}

TEST_CASE("hit a toggle boolean listener", "[file]")
{
    auto file = ReadRiveFile("assets/light_switch.riv");

    auto artboard = file->artboard()->instance();
    REQUIRE(artboard != nullptr);
    REQUIRE(artboard->stateMachineCount() == 1);

    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);

    artboard->advance(0.0f);
    stateMachine->advance(0.0f);

    auto switchButton = stateMachine->getBool("On");
    REQUIRE(switchButton != nullptr);
    REQUIRE(switchButton->value() == true);

    stateMachine->pointerDown(Vec2D(150.0f, 258.0f));
    stateMachine->pointerUp(Vec2D(150.0f, 258.0f));
    // Got toggled off after pressing
    REQUIRE(switchButton->value() == false);

    stateMachine->pointerDown(Vec2D(150.0f, 258.0f));
    stateMachine->pointerUp(Vec2D(150.0f, 258.0f));
    // Got toggled back on after pressing
    REQUIRE(switchButton->value() == true);
}

TEST_CASE("can query for all rive events", "[events]")
{
    auto file = ReadRiveFile("assets/event_on_listener.riv");
    auto artboard = file->artboard();

    auto eventCount = artboard->count<Event>();
    REQUIRE(eventCount == 4);
}

TEST_CASE("can query for a rive event at a given index", "[events]")
{
    auto file = ReadRiveFile("assets/event_on_listener.riv");
    auto artboard = file->artboard();

    auto event = artboard->objectAt<Event>(0);
    REQUIRE(event->name() == "Somewhere.com");
}

TEST_CASE("events load correctly on a listener", "[events]")
{
    auto file = ReadRiveFile("assets/event_on_listener.riv");

    auto artboard = file->artboard()->instance();
    REQUIRE(artboard != nullptr);
    REQUIRE(artboard->stateMachineCount() == 1);

    auto stateMachineInstance = artboard->stateMachineAt(0);
    REQUIRE(stateMachineInstance != nullptr);

    artboard->advance(0.0f);
    stateMachineInstance->advance(0.0f);

    auto events = artboard->find<Event>();
    REQUIRE(events.size() == 4);

    REQUIRE(stateMachineInstance->stateMachine()->listenerCount() == 1);
    auto listener1 = stateMachineInstance->stateMachine()->listener(0);
    auto target1 = artboard->resolve(listener1->targetId());
    REQUIRE(target1->is<Shape>());
    REQUIRE(listener1->actionCount() == 2);
    auto fireEvent1 = listener1->action(0);
    REQUIRE(fireEvent1 != nullptr);
    REQUIRE(fireEvent1->is<ListenerFireEvent>());
    REQUIRE(fireEvent1->as<ListenerFireEvent>()->eventId() != 0);
    auto event =
        artboard->resolve(fireEvent1->as<ListenerFireEvent>()->eventId());
    REQUIRE(event->is<Event>());
    REQUIRE(event->as<Event>()->name() == "Footstep");

    REQUIRE(stateMachineInstance->reportedEventCount() == 0);
    stateMachineInstance->pointerDown(Vec2D(343.0f, 116.0f));
    stateMachineInstance->pointerUp(Vec2D(343.0f, 116.0f));

    // There are two events on the listener.
    REQUIRE(stateMachineInstance->reportedEventCount() == 2);
    auto reportedEvent1 = stateMachineInstance->reportedEventAt(0);
    REQUIRE(reportedEvent1.event()->name() == "Footstep");
    auto reportedEvent2 = stateMachineInstance->reportedEventAt(1);
    REQUIRE(reportedEvent2.event()->name() == "Event 3");

    // After advancing again the reportedEventCount should return to 0.
    stateMachineInstance->advance(0.0f);
    REQUIRE(stateMachineInstance->reportedEventCount() == 0);
}

TEST_CASE("events load correctly on a state and transition", "[events]")
{
    auto file = ReadRiveFile("assets/events_on_states.riv");

    auto artboard = file->artboard()->instance();
    REQUIRE(artboard != nullptr);
    REQUIRE(artboard->stateMachineCount() == 1);

    auto stateMachineInstance = artboard->stateMachineAt(0);
    REQUIRE(stateMachineInstance != nullptr);

    artboard->advance(0.0f);
    stateMachineInstance->advance(0.0f);

    REQUIRE(stateMachineInstance->stateMachine()->layerCount() == 1);
    auto layer = stateMachineInstance->stateMachine()->layer(0);
    REQUIRE(layer->stateCount() == 5);
    REQUIRE(layer->entryState()->transitionCount() == 1);
    auto transition = layer->entryState()->transition(0);

    // No events on transition from entry.
    REQUIRE(transition->events().size() == 0);
    REQUIRE(transition->stateTo()->is<AnimationState>());
    auto firstAnimationState = transition->stateTo()->as<AnimationState>();
    REQUIRE(firstAnimationState->events().size() == 2);
    REQUIRE(firstAnimationState->transitionCount() == 1);
    transition = firstAnimationState->transition(0);
    // Transition from first animation state to next one should have two events.
    REQUIRE(transition->events().size() == 2);

    // First should've fired as we immediately went to Timeline 1.
    REQUIRE(stateMachineInstance->reportedEventCount() == 1);
    REQUIRE(stateMachineInstance->reportedEventAt(0).event()->name() ==
            "First");

    stateMachineInstance->advance(1.0f);
    // Exits after 2 seconds so 1 second in no events should've fired yet
    REQUIRE(stateMachineInstance->reportedEventCount() == 0);

    stateMachineInstance->advance(1.0f);
    // At 2 seconds 2 events should fire, one for exiting the state and for
    // taking the transition.
    REQUIRE(stateMachineInstance->reportedEventCount() == 2);
    REQUIRE(stateMachineInstance->reportedEventAt(0).event()->name() ==
            "Second");
    REQUIRE(stateMachineInstance->reportedEventAt(1).event()->name() ==
            "Third");

    stateMachineInstance->advance(1.0f);
    // Another second in the transition should complete
    REQUIRE(stateMachineInstance->reportedEventCount() == 1);
    REQUIRE(stateMachineInstance->reportedEventAt(0).event()->name() ==
            "Fourth");
}

TEST_CASE("timeline events load correctly and report", "[events]")
{
    auto file = ReadRiveFile("assets/timeline_event_test.riv");

    auto artboard = file->artboard()->instance();
    REQUIRE(artboard != nullptr);
    REQUIRE(artboard->stateMachineCount() == 1);

    auto stateMachineInstance = artboard->stateMachineAt(0);
    REQUIRE(stateMachineInstance != nullptr);

    artboard->advance(0.0f);
    stateMachineInstance->advance(0.0f);
    REQUIRE(stateMachineInstance->reportedEventCount() == 0);

    stateMachineInstance->advance(0.4f);
    REQUIRE(stateMachineInstance->reportedEventCount() == 0);

    stateMachineInstance->advance(0.2f);
    REQUIRE(stateMachineInstance->reportedEventCount() == 1);
    REQUIRE(stateMachineInstance->reportedEventAt(0).event()->name() == "Half");

    // Event should've occurred right at 0.5 seconds.
    REQUIRE(stateMachineInstance->reportedEventAt(0).secondsDelay() ==
            Approx(0.1f));
}

TEST_CASE("events from a nested artboard propagate to a listener on a parent",
          "[events]")
{
    auto file = ReadRiveFile("assets/nested_event_test.riv");

    auto artboard = file->artboard()->instance();
    REQUIRE(artboard != nullptr);
    REQUIRE(artboard->stateMachineCount() == 1);

    auto stateMachineInstance = artboard->stateMachineAt(0);
    REQUIRE(stateMachineInstance != nullptr);
    REQUIRE(stateMachineInstance->stateMachine()->inputCount() == 1);

    // Input is on the main artboard
    auto input = stateMachineInstance->getBool("Boolean 1");
    REQUIRE(input->value() == false);

    artboard->advance(0.0f);
    stateMachineInstance->advance(0.0f);

    auto nested = artboard->find<NestedArtboard>();
    REQUIRE(nested.size() == 1);
    auto nestedArtboard = nested[0]->artboardInstance();
    auto nestedStateMachineInstance = nested[0]
                                          ->nestedAnimations()[0]
                                          ->as<NestedStateMachine>()
                                          ->stateMachineInstance();
    REQUIRE(nestedStateMachineInstance != nullptr);
    auto events = nestedArtboard->find<Event>();
    REQUIRE(events.size() == 1);

    // Validate listener on the nested artboard
    REQUIRE(nestedStateMachineInstance->stateMachine()->listenerCount() == 1);
    auto listener1 = nestedStateMachineInstance->stateMachine()->listener(0);
    auto target1 = nestedArtboard->resolve(listener1->targetId());
    REQUIRE(target1->is<Shape>());
    REQUIRE(listener1->actionCount() == 1);
    auto fireEvent1 = listener1->action(0);
    REQUIRE(fireEvent1 != nullptr);
    REQUIRE(fireEvent1->is<ListenerFireEvent>());
    REQUIRE(fireEvent1->as<ListenerFireEvent>()->eventId() != 0);
    auto event =
        nestedArtboard->resolve(fireEvent1->as<ListenerFireEvent>()->eventId());
    REQUIRE(event->is<Event>());
    REQUIRE(event->as<Event>()->name() == "NestedEvent");

    // Validate the event is reported to the nested artboard
    REQUIRE(nestedStateMachineInstance->reportedEventCount() == 0);
    stateMachineInstance->pointerDown(Vec2D(250.0f, 100.0f));
    REQUIRE(nestedStateMachineInstance->reportedEventCount() == 1);
    auto nestedReportedEvent1 = nestedStateMachineInstance->reportedEventAt(0);
    REQUIRE(nestedReportedEvent1.event()->name() == "NestedEvent");

    artboard->advance(0.0f);

    // Validate the input on the main artboard updates as a result of the event
    // from the nested artboard
    REQUIRE(input->value() == true);

    // After advancing again the reportedEventCount should return to 0.
    stateMachineInstance->advance(0.0f);
    REQUIRE(stateMachineInstance->reportedEventCount() == 0);
}

TEST_CASE("event targetting an event object triggers correctly", "[events]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/target_event.riv", &silver);

    auto artboard = file->artboardDefault();

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    silver.addFrame();
    stateMachine->advanceAndApply(0.5f);
    artboard->draw(renderer.get());

    silver.addFrame();
    stateMachine->advanceAndApply(0.5f);
    artboard->draw(renderer.get());

    // Extra frame advance for the event to be processed
    silver.addFrame();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("target_event"));
}