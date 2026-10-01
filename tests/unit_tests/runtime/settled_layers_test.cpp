#include <rive/file.hpp>
#include <rive/animation/layer_state.hpp>
#include <rive/animation/state_machine_input_instance.hpp>
#include <rive/animation/state_machine_instance.hpp>
#include <rive/animation/state_machine_layer.hpp>
#include <rive/animation/state_transition.hpp>
#include <rive/shapes/shape.hpp>
#include <rive/shapes/paint/color.hpp>
#include <rive/shapes/paint/fill.hpp>
#include <rive/shapes/paint/solid_color.hpp>
#include <rive/viewmodel/viewmodel_instance_enum.hpp>
#include <rive/viewmodel/viewmodel_instance_trigger.hpp>
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <memory>

// A layer whose transitions only read view model values stops searching them
// once a search finds nothing ("settles"), until a bound value, its state or
// the data context changes. Every settled search skipped under TESTING is also
// re-run and aborts the run if it would have found a transition, so the whole
// suite checks the cache; these cases pin down that it engages and wakes up.

using namespace rive;

static ColorInt fillColor(ArtboardInstance* artboard)
{
    auto shape = artboard->find<Shape>("color_rectangle");
    REQUIRE(shape != nullptr);
    REQUIRE(shape->children()[1]->is<Fill>());
    return shape->children()[1]
        ->as<Fill>()
        ->paint()
        ->as<SolidColor>()
        ->colorValue();
}

TEST_CASE("View model conditions settle until a bound value changes",
          "[state machine]")
{
    auto file = ReadRiveFile("assets/data_binding_test.riv");
    auto artboard = file->artboard("artboard-2")->instance();
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    auto machine = artboard->defaultStateMachine();
    machine->bindViewModelInstance(viewModelInstance);
    auto shape = artboard->find<Shape>("color_rectangle");
    auto state =
        viewModelInstance->propertyValue("state")->as<ViewModelInstanceEnum>();
    auto trigger = viewModelInstance->propertyValue("trigger-prop")
                       ->as<ViewModelInstanceTrigger>();

    machine->advanceAndApply(0.0f);
    REQUIRE(fillColor(artboard.get()) == colorARGB(255, 255, 0, 0));

    // Nothing changes, so the layers settle and stop searching.
    auto skips = StateMachineInstance::sm_settledLayerSkips;
    for (int i = 0; i < 10; i++)
    {
        machine->advanceAndApply(1.0f / 60.0f);
    }
    CHECK(StateMachineInstance::sm_settledLayerSkips > skips);
    CHECK(fillColor(artboard.get()) == colorARGB(255, 255, 0, 0));

    // A bound enum change wakes them on the very next advance.
    state->value(1);
    machine->advanceAndApply(0.0f);
    CHECK(fillColor(artboard.get()) == colorARGB(255, 0, 255, 0));
    CHECK(shape->x() == 150);
    CHECK(shape->y() == 250);

    for (int i = 0; i < 10; i++)
    {
        machine->advanceAndApply(1.0f / 60.0f);
    }

    // So do an enum change and a trigger fired together on settled layers.
    state->value("state-blue");
    trigger->propertyValue(1);
    machine->advanceAndApply(0.0f);
    CHECK(fillColor(artboard.get()) == colorARGB(255, 0, 0, 255));
    CHECK(shape->x() == 350);
    CHECK(shape->y() == 250);

    for (int i = 0; i < 10; i++)
    {
        machine->advanceAndApply(1.0f / 60.0f);
    }

    // And a trigger on its own.
    trigger->propertyValue(1);
    machine->advanceAndApply(0.0f);
    CHECK(shape->x() == 350);
    CHECK(shape->y() == 350);
}

TEST_CASE("States reading state machine inputs are always searched",
          "[state machine]")
{
    auto file = ReadRiveFile("assets/state_machine_triggers.riv");
    auto artboard = file->artboard("main");
    auto stateMachine = artboard->stateMachine("State Machine 1");

    auto abi = artboard->instance();
    auto machine =
        std::make_unique<StateMachineInstance>(stateMachine, abi.get());
    machine->advanceAndApply(0.1f);
    auto idleState = machine->layerState(0);
    REQUIRE(idleState != nullptr);

    // Its transitions wait on a trigger input, which takes the slow path: the
    // layer never settles, however long nothing happens.
    auto skips = StateMachineInstance::sm_settledLayerSkips;
    for (int i = 0; i < 10; i++)
    {
        machine->advanceAndApply(1.0f / 60.0f);
    }
    CHECK(StateMachineInstance::sm_settledLayerSkips == skips);
    CHECK(machine->layerState(0) == idleState);

    machine->getTrigger("Trigger 1")->fire();
    machine->advanceAndApply(0.1f);
    CHECK(machine->layerState(0) != idleState);
}

TEST_CASE("Binding a data context wakes settled layers", "[state machine]")
{
    auto file = ReadRiveFile("assets/data_binding_test.riv");
    auto artboard = file->artboard("artboard-2")->instance();
    auto machine = artboard->defaultStateMachine();

    // Without a data context no view model condition can pass: the layers
    // settle on that. They wait in their entry states, which key nothing and
    // have no exit time, so they freeze too.
    auto skips = StateMachineInstance::sm_settledLayerSkips;
    auto frozen = StateMachineInstance::sm_frozenLayerAdvances;
    for (int i = 0; i < 10; i++)
    {
        machine->advanceAndApply(1.0f / 60.0f);
    }
    CHECK(StateMachineInstance::sm_settledLayerSkips > skips);
    CHECK(StateMachineInstance::sm_frozenLayerAdvances > frozen);

    machine->bindViewModelInstance(
        file->createDefaultViewModelInstance(artboard.get()));
    machine->advanceAndApply(0.0f);
    CHECK(fillColor(artboard.get()) == colorARGB(255, 255, 0, 0));
}

TEST_CASE("Setting a view model instance without binding wakes settled layers",
          "[state machine]")
{
    // setViewModelInstance can create the data context without binding
    // anything, which alone lets view model conditions evaluate. A machine
    // that settled first must end up where one that never ran does.
    auto file = ReadRiveFile("assets/data_binding_test.riv");

    auto settledArtboard = file->artboard("artboard-2")->instance();
    auto settled = settledArtboard->defaultStateMachine();
    for (int i = 0; i < 10; i++)
    {
        settled->advanceAndApply(1.0f / 60.0f);
    }
    settled->setViewModelInstance(
        file->createDefaultViewModelInstance(settledArtboard.get()));
    settled->advanceAndApply(0.0f);

    auto freshArtboard = file->artboard("artboard-2")->instance();
    auto fresh = freshArtboard->defaultStateMachine();
    fresh->setViewModelInstance(
        file->createDefaultViewModelInstance(freshArtboard.get()));
    fresh->advanceAndApply(0.0f);

    for (size_t i = 0; i < settled->stateMachine()->layerCount(); i++)
    {
        CHECK(settled->layerState(i) == fresh->layerState(i));
    }
    CHECK(fillColor(settledArtboard.get()) == fillColor(freshArtboard.get()));
}

TEST_CASE("States whose transitions wait on an exit time never freeze",
          "[state machine]")
{
    // A frozen layer stops advancing its state, and with it the clock an exit
    // time reads. This file has states that key nothing and wait on view model
    // conditions *and* exit times: those must keep advancing while settled,
    // or the exit time would never be reached once the condition passes.
    auto file = ReadRiveFile("assets/bidirectional_binding_source.riv");
    auto artboard = file->artboard("avatar_child_artboard");
    REQUIRE(artboard != nullptr);
    int timed = 0;
    int untimed = 0;
    for (size_t m = 0; m < artboard->stateMachineCount(); m++)
    {
        auto stateMachine = artboard->stateMachine(m);
        for (size_t l = 0; l < stateMachine->layerCount(); l++)
        {
            auto layer = stateMachine->layer(l);
            for (size_t s = 0; s < layer->stateCount(); s++)
            {
                auto state = layer->state(s);
                bool waitsOnExitTime = false;
                for (size_t t = 0; t < state->transitionCount(); t++)
                {
                    auto transition = state->transition(t);
                    waitsOnExitTime =
                        waitsOnExitTime || (!transition->isDisabled() &&
                                            transition->enableExitTime());
                }
                CHECK(state->transitionsIgnoreTime() == !waitsOnExitTime);
                (waitsOnExitTime ? timed : untimed)++;
            }
        }
    }
    CHECK(timed > 0);
    CHECK(untimed > 0);
}
