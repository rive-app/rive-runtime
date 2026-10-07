/*
 * Copyright 2026 Rive
 */

// A view model trigger fire, a component trigger fire, or a change to a value
// a "changed" (Self) condition reads, is pending for a state machine for one
// frame: from the moment it happens until the end of the first frame in which
// the machine searched its transitions after it. A layer takes at most one
// transition on a view model value in that frame. Changes from before the
// machine was created or reset, or from while it was inactive, are never
// pending for it, unless another machine's advance made or reset it: then it
// starts in that machine's frame. These cases pin down both ends of that
// window.

#include <rive/animation/animation_state.hpp>
#include <rive/animation/linear_animation.hpp>
#include <rive/animation/nested_state_machine.hpp>
#include <rive/animation/state_machine_instance.hpp>
#include <rive/artboard_component_list.hpp>
#include <rive/custom_property_trigger.hpp>
#include <rive/data_bind/data_context.hpp>
#include <rive/event.hpp>
#include <rive/file.hpp>
#include <rive/nested_artboard.hpp>
#include <rive/viewmodel/viewmodel.hpp>
#include <rive/viewmodel/viewmodel_instance_boolean.hpp>
#include <rive/viewmodel/viewmodel_instance_list.hpp>
#include <rive/viewmodel/viewmodel_instance_list_item.hpp>
#include <rive/viewmodel/viewmodel_instance_number.hpp>
#include <rive/viewmodel/viewmodel_instance_string.hpp>
#include <rive/viewmodel/viewmodel_instance_trigger.hpp>
#include <rive/viewmodel/viewmodel_instance_viewmodel.hpp>
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <string>
#include <vector>

using namespace rive;

static std::string layerStateName(StateMachineInstance* machine, size_t layer)
{
    auto state = machine->layerState(layer);
    if (state == nullptr || !state->is<AnimationState>())
    {
        return "";
    }
    auto animation = state->as<AnimationState>()->animation();
    return animation == nullptr ? "" : animation->name();
}

static ViewModelInstanceTrigger* trigger(rcp<ViewModelInstance> instance,
                                         const char* name)
{
    auto value = instance->propertyValue(name);
    REQUIRE(value != nullptr);
    REQUIRE(value->is<ViewModelInstanceTrigger>());
    return value->as<ViewModelInstanceTrigger>();
}

TEST_CASE("a fire from earlier in a frame reaches a state entered later in it",
          "[data binding]")
{
    // artboard-2's second layer goes Entry -> left unconditionally, then
    // left -> right and right -> bottom on trigger-prop.
    auto file = ReadRiveFile("assets/data_binding_test.riv");
    auto artboard = file->artboard("artboard-2")->instance();
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    auto machine = artboard->defaultStateMachine();
    machine->bindViewModelInstance(viewModelInstance);
    auto fire = trigger(viewModelInstance, "trigger-prop");

    // Fired before the advance that enters `left`, and still taken there.
    fire->trigger();
    machine->advanceAndApply(0.0f);
    CHECK(layerStateName(machine.get(), 1) == "right");

    // One fire takes one transition: right -> bottom waits for the next one.
    machine->advanceAndApply(0.0f);
    CHECK(layerStateName(machine.get(), 1) == "right");
    fire->trigger();
    machine->advanceAndApply(0.0f);
    CHECK(layerStateName(machine.get(), 1) == "bottom");
}

TEST_CASE("a state machine ignores fires from before it was created",
          "[data binding]")
{
    // StateEnter's any state goes to `ready` on the `go` trigger. `ready`'s
    // enter action sets the `ready` boolean, which unlocks ready -> done; the
    // any state returns to `idle` while it is false.
    auto file = ReadRiveFile("assets/state_action_vm_sync.riv");
    auto firstArtboard = file->artboardNamed("StateEnter");
    REQUIRE(firstArtboard != nullptr);
    auto firstMachine = firstArtboard->stateMachineAt(0);
    auto viewModel =
        file->createViewModelInstance(firstArtboard->viewModelId(), 0);
    REQUIRE(viewModel != nullptr);
    firstMachine->bindViewModelInstance(viewModel);
    firstMachine->advanceAndApply(0.0f);
    trigger(viewModel, "go")->trigger();
    firstMachine->advanceAndApply(0.016f);
    REQUIRE(layerStateName(firstMachine.get(), 0) == "done");

    // A second machine on the same view model, created after that fire.
    viewModel->propertyValue("ready")
        ->as<ViewModelInstanceBoolean>()
        ->propertyValue(false);
    auto secondArtboard = file->artboardNamed("StateEnter");
    auto secondMachine = secondArtboard->stateMachineAt(0);
    secondMachine->bindViewModelInstance(viewModel);
    for (int i = 0; i < 3; i++)
    {
        secondMachine->advanceAndApply(0.016f);
    }
    CHECK(layerStateName(secondMachine.get(), 0) == "idle");

    // Its own fires still reach it.
    trigger(viewModel, "go")->trigger();
    secondMachine->advanceAndApply(0.016f);
    CHECK(layerStateName(secondMachine.get(), 0) == "done");
}

TEST_CASE("resetState drops changes made before it", "[data binding]")
{
    // A list reuses a pooled state machine for another item by resetting it.
    // Like changes made before a machine is created, changes made before the
    // reset aren't pending for its new run: otherwise a machine that sat in
    // the pool would replay every change made since it was last used.
    auto file = ReadRiveFile("assets/state_action_vm_sync.riv");
    auto artboard = file->artboardNamed("StateEnter");
    auto machine = artboard->stateMachineAt(0);
    auto viewModel = file->createViewModelInstance(artboard->viewModelId(), 0);
    machine->bindViewModelInstance(viewModel);
    machine->advanceAndApply(0.0f);
    REQUIRE(layerStateName(machine.get(), 0) == "idle");

    trigger(viewModel, "go")->trigger();
    machine->resetState();
    machine->advanceAndApply(0.016f);
    CHECK(layerStateName(machine.get(), 0) == "idle");

    trigger(viewModel, "go")->trigger();
    machine->advanceAndApply(0.016f);
    CHECK(layerStateName(machine.get(), 0) == "done");
}

TEST_CASE("a settled state machine bound to another instance ignores its past "
          "fires",
          "[data binding]")
{
    auto file = ReadRiveFile("assets/state_action_vm_sync.riv");
    auto settledArtboard = file->artboardNamed("StateEnter");
    auto settled = settledArtboard->stateMachineAt(0);
    auto first =
        file->createViewModelInstance(settledArtboard->viewModelId(), 0);
    settled->bindViewModelInstance(first);
    for (int i = 0; i < 3; i++)
    {
        settled->advanceAndApply(0.016f);
    }
    REQUIRE(layerStateName(settled.get(), 0) == "idle");

    // Another machine takes a fire on its own instance over a few frames
    // while the first stays settled.
    auto otherArtboard = file->artboardNamed("StateEnter");
    auto other = otherArtboard->stateMachineAt(0);
    auto second =
        file->createViewModelInstance(settledArtboard->viewModelId(), 0);
    other->bindViewModelInstance(second);
    other->advanceAndApply(0.0f);
    trigger(second, "go")->trigger();
    for (int i = 0; i < 3; i++)
    {
        other->advanceAndApply(0.016f);
        settled->advanceAndApply(0.016f);
    }
    REQUIRE(layerStateName(other.get(), 0) == "done");

    settled->bindViewModelInstance(second);
    settled->advanceAndApply(0.016f);
    CHECK(layerStateName(settled.get(), 0) == "idle");

    trigger(second, "go")->trigger();
    settled->advanceAndApply(0.016f);
    CHECK(layerStateName(settled.get(), 0) == "done");
}

TEST_CASE("a fire its transition can't take in its frame is not kept for later",
          "[data binding]")
{
    // main's second layer goes Timeline 1 -> small-green when `tri` fired and
    // `num` changed.
    auto file = ReadRiveFile("assets/transition_self_comparator_test.riv");
    auto artboard = file->artboardNamed("main");
    REQUIRE(artboard != nullptr);
    auto machine = artboard->stateMachineAt(0);
    auto viewModel = file->createViewModelInstance(artboard->viewModelId(), 0);
    REQUIRE(viewModel != nullptr);
    machine->bindViewModelInstance(viewModel);
    machine->advanceAndApply(0.0f);
    REQUIRE(layerStateName(machine.get(), 1) == "Timeline 1");
    auto number =
        viewModel->propertyValue("num")->as<ViewModelInstanceNumber>();

    // The trigger alone can't take it, and is gone by the time num changes.
    trigger(viewModel, "tri")->trigger();
    machine->advanceAndApply(0.016f);
    machine->advanceAndApply(0.016f);
    number->propertyValue(number->propertyValue() + 1);
    machine->advanceAndApply(0.016f);
    CHECK(layerStateName(machine.get(), 1) == "Timeline 1");

    // Both in the same frame take it.
    trigger(viewModel, "tri")->trigger();
    number->propertyValue(number->propertyValue() + 1);
    machine->advanceAndApply(0.016f);
    CHECK(layerStateName(machine.get(), 1) == "small-green");
}

TEST_CASE("a fire during a transition's mix is not taken once it ends",
          "[data binding]")
{
    // TriggerVis goes to TriggerShow from any state on Test Trigger, and on
    // from TriggerShow to TriggerHide unconditionally with a 500ms mix that
    // can't be interrupted.
    auto file = ReadRiveFile("assets/data_bind_test_cmdq.riv");
    auto artboard = file->artboardNamed("Test Artboard");
    REQUIRE(artboard != nullptr);
    auto machine = artboard->stateMachineAt(0);
    auto viewModel = file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModel != nullptr);
    machine->bindViewModelInstance(viewModel);
    machine->advanceAndApply(0.0f);
    REQUIRE(layerStateName(machine.get(), 0) == "TriggerHide");

    // Show, then start mixing back to TriggerHide.
    trigger(viewModel, "Test Trigger")->trigger();
    machine->advanceAndApply(0.016f);
    REQUIRE(machine->stateChangedCount() == 1);
    REQUIRE(layerStateName(machine.get(), 0) == "TriggerHide");

    // Fired mid-mix: nothing can take it then, and nothing does afterwards.
    machine->advanceAndApply(0.1f);
    trigger(viewModel, "Test Trigger")->trigger();
    size_t changes = 0;
    for (int i = 0; i < 60; i++)
    {
        machine->advanceAndApply(0.016f);
        changes += machine->stateChangedCount();
    }
    CHECK(changes == 0);
}

// Runs collapsed_databinds_test until its nested Child is shown, fires the
// trigger Child's Timeline 1 -> Timeline 2 waits on, optionally with the nested
// artboard paused for a few frames around the fire, and returns the state
// Child's machine ends up in.
static std::string childStateAfterFire(bool pausedDuringFire)
{
    auto file = ReadRiveFile("assets/collapsed_databinds_test.riv");
    auto artboard = file->artboardDefault();
    auto machine = artboard->stateMachineAt(0);
    auto viewModel = file->createDefaultViewModelInstance(artboard.get());
    machine->bindViewModelInstance(viewModel);
    // Main shows the nested artboard once it reaches Timeline 2.
    machine->advanceAndApply(0.0f);
    for (int i = 0; i < 9; i++)
    {
        machine->advanceAndApply(0.25f);
    }
    auto nested = artboard->nestedArtboards();
    REQUIRE(nested.size() == 1);
    REQUIRE_FALSE(nested[0]->isCollapsed());
    auto childMachine = [&]() {
        for (auto animation : nested[0]->nestedAnimations())
        {
            if (animation->is<NestedStateMachine>())
            {
                return animation->as<NestedStateMachine>()
                    ->stateMachineInstance();
            }
        }
        return static_cast<StateMachineInstance*>(nullptr);
    };
    REQUIRE(childMachine() != nullptr);
    REQUIRE(layerStateName(childMachine(), 0) == "Timeline 1");

    nested[0]->isPaused(pausedDuringFire);
    trigger(viewModel, "tri")->trigger();
    for (int i = 0; i < 3; i++)
    {
        machine->advanceAndApply(0.016f);
    }
    nested[0]->isPaused(false);
    for (int i = 0; i < 3; i++)
    {
        machine->advanceAndApply(0.016f);
    }
    return layerStateName(childMachine(), 0);
}

TEST_CASE("a nested state machine ignores fires from while it was paused",
          "[data binding]")
{
    // Advancing normally, Child takes the fire.
    CHECK(childStateAfterFire(false) == "Timeline 2");
    // Paused, it couldn't search then, and the fire is long gone by the time
    // it runs again. Collapsing the nested artboard works the same way.
    CHECK(childStateAfterFire(true) == "Timeline 1");
}

// The nested state machine running the nested artboard whose artboard is
// named `name`.
static StateMachineInstance* nestedMachine(ArtboardInstance* artboard,
                                           const char* name)
{
    for (auto nested : artboard->nestedArtboards())
    {
        if (nested->artboardInstance() == nullptr ||
            nested->artboardInstance()->name() != name)
        {
            continue;
        }
        for (auto animation : nested->nestedAnimations())
        {
            if (animation->is<NestedStateMachine>())
            {
                return animation->as<NestedStateMachine>()
                    ->stateMachineInstance();
            }
        }
    }
    return nullptr;
}

TEST_CASE("a trigger a nested state machine fires in the settle loop reaches "
          "a sibling that already searched",
          "[data binding]")
{
    // nested_trigger_relay.riv (built by packages/rive_core/test/src/
    // nested_trigger_relay_file.dart): Main nests Consumer, Producer2 and
    // Producer1, in that order. Producer1 fires `a1` on `go`; Producer2 fires
    // `relay` on `a1`, which it only sees in a settle pass, after Consumer
    // already searched in it; Consumer goes waiting -> received on `relay`.
    // Main's per-frame consume used to clear `relay` before Consumer searched
    // again, so it never got it.
    auto file = ReadRiveFile("assets/nested_trigger_relay.riv");
    auto artboard = file->artboardNamed("Main");
    REQUIRE(artboard != nullptr);
    auto machine = artboard->stateMachineAt(0);
    auto viewModel = file->createViewModelInstance(artboard->viewModelId(), 0);
    REQUIRE(viewModel != nullptr);
    machine->bindViewModelInstance(viewModel);
    machine->advanceAndApply(0.0f);
    auto consumer = nestedMachine(artboard.get(), "Consumer");
    REQUIRE(consumer != nullptr);
    REQUIRE(layerStateName(consumer, 0) == "waiting");

    trigger(viewModel, "go")->trigger();
    machine->advanceAndApply(0.016f);
    CHECK(layerStateName(nestedMachine(artboard.get(), "Producer1"), 0) ==
          "fired");
    CHECK(layerStateName(nestedMachine(artboard.get(), "Producer2"), 0) ==
          "fired");
    machine->advanceAndApply(0.016f);
    CHECK(layerStateName(consumer, 0) == "received");
}

TEST_CASE("a trigger fired before a view model listener binds still reaches it",
          "[data binding]")
{
    // vm_listener_fire_event.riv's view model listener fires `ding` on `go`.
    // Firing before the listener binds is what a script's init() does: it runs
    // as the artboard binds, before the state machine's own listeners do.
    auto file = ReadRiveFile("assets/vm_listener_fire_event.riv");
    auto artboard = file->artboardDefault();
    auto machine = artboard->stateMachineAt(0);
    auto viewModel = file->createViewModelInstance(artboard->viewModelId(), 0);
    REQUIRE(viewModel != nullptr);
    trigger(viewModel, "go")->trigger();
    machine->bindViewModelInstance(viewModel);
    machine->advanceAndApply(0.016f);
    REQUIRE(machine->reportedEventCount() == 1);
    CHECK(machine->reportedEventAt(0).event()->name() == "ding");
    machine->advanceAndApply(0.016f);
    CHECK(machine->reportedEventCount() == 0);
}

TEST_CASE("a view model listener doesn't replay a fire whose frame has passed",
          "[data binding]")
{
    auto file = ReadRiveFile("assets/vm_listener_fire_event.riv");
    auto artboard = file->artboardDefault();
    auto machine = artboard->stateMachineAt(0);
    auto viewModel = file->createViewModelInstance(artboard->viewModelId(), 0);
    REQUIRE(viewModel != nullptr);
    machine->bindViewModelInstance(viewModel);
    machine->advanceAndApply(0.0f);
    trigger(viewModel, "go")->trigger();
    machine->advanceAndApply(0.016f);
    REQUIRE(machine->reportedEventCount() == 1);
    machine->advanceAndApply(0.016f);

    // Binding again, or another machine binding the same view model, hears
    // nothing until the next fire.
    machine->bindViewModelInstance(viewModel);
    machine->advanceAndApply(0.016f);
    CHECK(machine->reportedEventCount() == 0);
    auto secondArtboard = file->artboardDefault();
    auto secondMachine = secondArtboard->stateMachineAt(0);
    secondMachine->bindViewModelInstance(viewModel);
    secondMachine->advanceAndApply(0.016f);
    CHECK(secondMachine->reportedEventCount() == 0);
    trigger(viewModel, "go")->trigger();
    secondMachine->advanceAndApply(0.016f);
    CHECK(secondMachine->reportedEventCount() == 1);
}

static std::string stringValue(rcp<ViewModelInstance> instance,
                               const char* name)
{
    auto value = instance->propertyValue(name);
    REQUIRE(value != nullptr);
    REQUIRE(value->is<ViewModelInstanceString>());
    return value->as<ViewModelInstanceString>()->propertyValue();
}

TEST_CASE("a view model listener moved to another instance hears its fires",
          "[data binding]")
{
    // trigger_based_listeners.riv: firing vm1's tri1 sets its str1.
    auto file = ReadRiveFile("assets/trigger_based_listeners.riv");
    auto artboard = file->artboardNamed("main");
    REQUIRE(artboard != nullptr);
    auto machine = artboard->stateMachineAt(0);
    auto viewModel = file->createViewModelInstance(artboard->viewModelId(), 0);
    REQUIRE(viewModel != nullptr);
    machine->bindViewModelInstance(viewModel);
    machine->advanceAndApply(0.0f);
    auto first = viewModel->propertyValue("vm1")
                     ->as<ViewModelInstanceViewModel>()
                     ->referenceViewModelInstance();
    REQUIRE(first != nullptr);
    trigger(first, "tri1")->trigger();
    machine->advanceAndApply(0.016f);
    REQUIRE(stringValue(first, "str1") == "changed-text-1");

    // A fresh instance's first fire leaves its count where the old one's was.
    auto second = file->createViewModelInstance(first->viewModel()->name());
    REQUIRE(second != nullptr);
    viewModel->replaceViewModelByName("vm1", second);
    machine->advanceAndApply(0.016f);
    trigger(second, "tri1")->trigger();
    machine->advanceAndApply(0.016f);
    CHECK(stringValue(second, "str1") == "changed-text-1");

    // Fired just before the swap, in the same frame: still heard.
    auto third = file->createViewModelInstance(first->viewModel()->name());
    trigger(third, "tri1")->trigger();
    viewModel->replaceViewModelByName("vm1", third);
    machine->advanceAndApply(0.016f);
    CHECK(stringValue(third, "str1") == "changed-text-1");
}

TEST_CASE("a listener action fires a view model trigger every time it runs",
          "[data binding]")
{
    // trigger_based_listeners.riv: a press fires vm1's tri1 and tri2 through
    // listener actions writing them over to-source binds. Every press fires
    // them again, though a trigger's count never goes back to 0.
    auto file = ReadRiveFile("assets/trigger_based_listeners.riv");
    auto artboard = file->artboardNamed("main");
    REQUIRE(artboard != nullptr);
    auto machine = artboard->stateMachineAt(0);
    auto viewModel = file->createViewModelInstance(artboard->viewModelId(), 0);
    REQUIRE(viewModel != nullptr);
    machine->bindViewModelInstance(viewModel);
    machine->advanceAndApply(0.0f);
    auto vm1 = viewModel->propertyValue("vm1")
                   ->as<ViewModelInstanceViewModel>()
                   ->referenceViewModelInstance();
    auto tri1 = trigger(vm1, "tri1");
    auto tri2 = trigger(vm1, "tri2");
    for (uint32_t press = 1; press <= 3; press++)
    {
        machine->pointerDown(Vec2D(25.0f, 25.0f));
        machine->advanceAndApply(0.016f);
        machine->pointerUp(Vec2D(25.0f, 25.0f));
        machine->advanceAndApply(0.016f);
        CHECK(tri1->propertyValue() == press);
        CHECK(tri2->propertyValue() == press);
    }
}

TEST_CASE("binding a trigger to a view model trigger doesn't fire it",
          "[data binding]")
{
    // custom_property_trigger.riv binds Main's custom property trigger Trig to
    // its view model item's ItemTrigger.
    auto file = ReadRiveFile("assets/custom_property_trigger.riv");
    auto first = file->artboard("Main")->instance();
    auto viewModel = file->createDefaultViewModelInstance(first.get());
    REQUIRE(viewModel != nullptr);
    auto firstMachine = first->defaultStateMachine();
    firstMachine->bindViewModelInstance(viewModel);
    firstMachine->advanceAndApply(0.0f);
    auto item = viewModel->propertyValue("property of ItemVM")
                    ->as<ViewModelInstanceViewModel>()
                    ->referenceViewModelInstance();
    auto itemTrigger = trigger(item, "ItemTrigger");
    itemTrigger->trigger();
    firstMachine->advanceAndApply(0.016f);
    REQUIRE(itemTrigger->propertyValue() != 0);

    // A second artboard bound to it after that: its Trig starts unfired,
    // and fires on the next fire.
    auto second = file->artboard("Main")->instance();
    auto secondTrig = second->find<CustomPropertyTrigger>("Trig");
    REQUIRE(secondTrig != nullptr);
    auto secondMachine = second->defaultStateMachine();
    secondMachine->bindViewModelInstance(viewModel);
    secondMachine->advanceAndApply(0.0f);
    CHECK(secondTrig->propertyValue() == 0);
    itemTrigger->trigger();
    secondMachine->advanceAndApply(0.016f);
    CHECK(secondTrig->propertyValue() != 0);
}

static void fire(CustomPropertyTrigger* trigger)
{
    trigger->propertyValue(trigger->propertyValue() + 1);
}

TEST_CASE("a component trigger condition holds only in its fire's frame",
          "[data binding]")
{
    // component_trigger_condition.riv (built by packages/rive_core/test/src/
    // component_trigger_condition_file.dart): Main goes idle -> pressed when
    // its component trigger Press fires, and pressed -> idle when Release
    // fires. A trigger's count only goes up, so a condition reads the trigger
    // as fired in its fire's frame and as unfired after that.
    auto file = ReadRiveFile("assets/component_trigger_condition.riv");
    auto artboard = file->artboardNamed("Main");
    REQUIRE(artboard != nullptr);
    auto press = artboard->find<CustomPropertyTrigger>("Press");
    auto release = artboard->find<CustomPropertyTrigger>("Release");
    REQUIRE(press != nullptr);
    REQUIRE(release != nullptr);
    auto machine = artboard->stateMachineAt(0);
    machine->advanceAndApply(0.0f);
    REQUIRE(layerStateName(machine.get(), 0) == "idle");

    for (int round = 0; round < 2; round++)
    {
        fire(press);
        machine->advanceAndApply(0.016f);
        CHECK(layerStateName(machine.get(), 0) == "pressed");
        machine->advanceAndApply(0.016f);
        CHECK(layerStateName(machine.get(), 0) == "pressed");

        // Press fired frames ago, so idle doesn't go back to pressed on it.
        fire(release);
        machine->advanceAndApply(0.016f);
        CHECK(layerStateName(machine.get(), 0) == "idle");
        machine->advanceAndApply(0.016f);
        CHECK(layerStateName(machine.get(), 0) == "idle");
    }
}

TEST_CASE("a component trigger fired before a state machine's first advance "
          "reaches it",
          "[data binding]")
{
    auto file = ReadRiveFile("assets/component_trigger_condition.riv");
    auto artboard = file->artboardNamed("Main");
    REQUIRE(artboard != nullptr);
    auto press = artboard->find<CustomPropertyTrigger>("Press");
    REQUIRE(press != nullptr);

    // From before the machine was created: ignored, like a view model
    // trigger's.
    fire(press);
    auto machine = artboard->stateMachineAt(0);
    machine->advanceAndApply(0.0f);
    CHECK(layerStateName(machine.get(), 0) == "idle");

    auto fresh = artboard->stateMachineAt(0);
    fire(press);
    fresh->advanceAndApply(0.0f);
    CHECK(layerStateName(fresh.get(), 0) == "pressed");
}

// The state the machine of each row of `artboard`'s list is in, in list order.
static std::vector<std::string> rowStates(ArtboardInstance* artboard)
{
    auto list = artboard->find<ArtboardComponentList>("Rows");
    REQUIRE(list != nullptr);
    std::vector<std::string> states;
    for (size_t i = 0; i < list->artboardCount(); i++)
    {
        auto row = list->stateMachineInstance((int)i);
        states.push_back(row == nullptr ? "" : layerStateName(row, 0));
    }
    return states;
}

TEST_CASE("a fire made before a list's first advance reaches the rows it makes",
          "[data binding]")
{
    // Main's list makes its rows' machines in its first advance, after the
    // app bound the view model and fired `play`. The rows start in the frame
    // the fire is pending in, so it's pending for them as it is for Main.
    auto file = ReadRiveFile("assets/list_item_parent_trigger.riv");
    auto artboard = file->artboardNamed("Main");
    REQUIRE(artboard != nullptr);
    auto machine = artboard->stateMachineAt(0);
    auto viewModel = file->createDefaultViewModelInstance(artboard.get());
    machine->bindViewModelInstance(viewModel);

    trigger(viewModel, "play")->trigger();
    machine->advanceAndApply(0.0f);
    CHECK(layerStateName(machine.get(), 0) == "played");
    CHECK(rowStates(artboard.get()) ==
          std::vector<std::string>{"played", "played"});
}

TEST_CASE("a host machine that reads no values still sets its rows' frame, "
          "however it is bound",
          "[data binding]")
{
    // Quiet's own machine reads no view model values. Whether the app binds
    // its view model or a script binds it to a data context (as one does an
    // artboard it made under its own context), its rows start in its frame.
    auto file = ReadRiveFile("assets/list_item_parent_trigger.riv");
    for (bool toDataContext : {false, true})
    {
        INFO("bound to a data context: " << toDataContext);
        auto artboard = file->artboardNamed("Quiet");
        REQUIRE(artboard != nullptr);
        auto machine = artboard->stateMachineAt(0);
        auto viewModel = file->createDefaultViewModelInstance(artboard.get());
        if (toDataContext)
        {
            machine->bindDataContext(make_rcp<DataContext>(viewModel));
        }
        else
        {
            machine->bindViewModelInstance(viewModel);
        }

        trigger(viewModel, "play")->trigger();
        machine->advanceAndApply(0.0f);
        CHECK(rowStates(artboard.get()) ==
              std::vector<std::string>{"played", "played"});
    }
}

TEST_CASE("a fire reaches a row added in its frame but not one added later",
          "[data binding]")
{
    auto file = ReadRiveFile("assets/list_item_parent_trigger.riv");
    auto artboard = file->artboardNamed("Main");
    REQUIRE(artboard != nullptr);
    auto machine = artboard->stateMachineAt(0);
    auto viewModel = file->createDefaultViewModelInstance(artboard.get());
    machine->bindViewModelInstance(viewModel);
    machine->advanceAndApply(0.0f);
    REQUIRE(rowStates(artboard.get()) ==
            std::vector<std::string>{"waiting", "waiting"});

    auto items = viewModel->propertyValue("items")->as<ViewModelInstanceList>();
    auto addRow = [&]() {
        auto item = make_rcp<ViewModelInstanceListItem>();
        item->viewModelInstance(
            file->createDefaultViewModelInstance(file->viewModel("Item")));
        items->addItem(item);
    };

    // Added with the fire: its machine is made in the fire's frame.
    trigger(viewModel, "play")->trigger();
    addRow();
    machine->advanceAndApply(0.016f);
    CHECK(rowStates(artboard.get()) ==
          std::vector<std::string>{"played", "played", "played"});

    // Added a frame later: the fire's frame has passed, so it isn't replayed.
    addRow();
    machine->advanceAndApply(0.016f);
    CHECK(rowStates(artboard.get()) ==
          std::vector<std::string>{"played", "played", "played", "waiting"});
}
