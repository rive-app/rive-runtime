#include <rive/file.hpp>
#include <rive/animation/state_machine_bool.hpp>
#include <rive/animation/state_machine_trigger.hpp>
#include <rive/animation/state_machine_layer.hpp>
#include <rive/animation/animation_state.hpp>
#include <rive/animation/entry_state.hpp>
#include <rive/animation/state_transition.hpp>
#include <rive/animation/state_machine_instance.hpp>
#include <rive/animation/state_machine_input_instance.hpp>
#include <rive/animation/blend_state_1d.hpp>
#include <rive/animation/blend_animation_1d.hpp>
#include <rive/animation/blend_state_direct.hpp>
#include <rive/animation/blend_state_transition.hpp>
#include <rive/animation/animation_reset_factory.hpp>
#include <rive/shapes/paint/solid_color.hpp>
#include <rive/shapes/paint/stroke.hpp>
#include <rive/shapes/shape.hpp>
#include "catch.hpp"
#include "rive_file_reader.hpp"
#include "utils/serializing_factory.hpp"
#include <cstdio>

using namespace rive;

TEST_CASE("file with state machine be read", "[file]")
{
    auto file = ReadRiveFile("assets/rocket.riv");

    auto artboard = file->artboard();
    REQUIRE(artboard != nullptr);
    REQUIRE(artboard->animationCount() == 3);
    REQUIRE(artboard->stateMachineCount() == 1);

    auto stateMachine = artboard->stateMachine("Button");
    REQUIRE(stateMachine != nullptr);

    REQUIRE(stateMachine->layerCount() == 1);
    REQUIRE(stateMachine->inputCount() == 2);

    auto hover = stateMachine->input("Hover");
    REQUIRE(hover != nullptr);
    REQUIRE(hover->is<rive::StateMachineBool>());

    auto press = stateMachine->input("Press");
    REQUIRE(press != nullptr);
    REQUIRE(press->is<rive::StateMachineBool>());

    auto layer = stateMachine->layer(0);
    REQUIRE(layer->stateCount() == 6);

    REQUIRE(layer->anyState() != nullptr);
    REQUIRE(layer->entryState() != nullptr);
    REQUIRE(layer->exitState() != nullptr);

    int foundAnimationStates = 0;
    for (int i = 0; i < layer->stateCount(); i++)
    {
        auto state = layer->state(i);
        if (state->is<rive::AnimationState>())
        {
            foundAnimationStates++;
            REQUIRE(state->as<rive::AnimationState>()->animation() != nullptr);
        }
    }

    REQUIRE(foundAnimationStates == 3);

    REQUIRE(layer->entryState()->transitionCount() == 1);
    auto stateTo = layer->entryState()->transition(0)->stateTo();
    REQUIRE(stateTo != nullptr);
    REQUIRE(stateTo->is<rive::AnimationState>());
    REQUIRE(stateTo->as<rive::AnimationState>()->animation() != nullptr);
    REQUIRE(stateTo->as<rive::AnimationState>()->animation()->name() == "idle");

    auto idleState = stateTo->as<rive::AnimationState>();
    REQUIRE(idleState->transitionCount() == 2);
    for (int i = 0; i < idleState->transitionCount(); i++)
    {
        auto transition = idleState->transition(i);
        if (transition->stateTo()
                ->as<rive::AnimationState>()
                ->animation()
                ->name() == "Roll_over")
        {
            // Check the condition
            REQUIRE(transition->conditionCount() == 1);
        }
    }

    auto abi = artboard->instance();
    rive::StateMachineInstance smi(artboard->stateMachine("Button"), abi.get());

    REQUIRE(smi.getBool("Hover")->name() == "Hover");
    REQUIRE(smi.getBool("Press")->name() == "Press");
    REQUIRE(smi.getBool("Hover") != nullptr);
    REQUIRE(smi.getBool("Press") != nullptr);
    REQUIRE(smi.stateChangedCount() == 0);
    REQUIRE(smi.currentAnimationCount() == 0);
}

TEST_CASE("file with blend states loads correctly", "[file]")
{
    auto file = ReadRiveFile("assets/blend_test.riv");

    auto artboard = file->artboard();
    REQUIRE(artboard != nullptr);
    REQUIRE(artboard->animationCount() == 4);
    REQUIRE(artboard->stateMachineCount() == 2);

    auto stateMachine = artboard->stateMachine("blend");
    REQUIRE(stateMachine != nullptr);

    REQUIRE(stateMachine->layerCount() == 1);
    auto layer = stateMachine->layer(0);
    REQUIRE(layer->stateCount() == 5);

    REQUIRE(layer->anyState() != nullptr);
    REQUIRE(layer->entryState() != nullptr);
    REQUIRE(layer->exitState() != nullptr);

    REQUIRE(layer->state(1)->is<rive::BlendState1D>());
    REQUIRE(layer->state(2)->is<rive::BlendState1D>());

    auto blendStateA = layer->state(1)->as<rive::BlendState1D>();
    auto blendStateB = layer->state(2)->as<rive::BlendState1D>();

    REQUIRE(blendStateA->animationCount() == 3);
    REQUIRE(blendStateB->animationCount() == 3);

    auto animation = blendStateA->animation(0);
    REQUIRE(animation->is<rive::BlendAnimation1D>());
    auto animation1D = animation->as<rive::BlendAnimation1D>();
    REQUIRE(animation1D->animation() != nullptr);
    REQUIRE(animation1D->animation()->name() == "horizontal");
    REQUIRE(animation1D->value() == 0.0f);

    animation = blendStateA->animation(1);
    REQUIRE(animation->is<rive::BlendAnimation1D>());
    animation1D = animation->as<rive::BlendAnimation1D>();
    REQUIRE(animation1D->animation() != nullptr);
    REQUIRE(animation1D->animation()->name() == "vertical");
    REQUIRE(animation1D->value() == 100.0f);

    animation = blendStateA->animation(2);
    REQUIRE(animation->is<rive::BlendAnimation1D>());
    animation1D = animation->as<rive::BlendAnimation1D>();
    REQUIRE(animation1D->animation() != nullptr);
    REQUIRE(animation1D->animation()->name() == "rotate");
    REQUIRE(animation1D->value() == 0.0f);

    REQUIRE(blendStateA->transitionCount() == 1);
    REQUIRE(blendStateA->transition(0)->is<rive::BlendStateTransition>());
    REQUIRE(blendStateA->transition(0)
                ->as<rive::BlendStateTransition>()
                ->exitBlendAnimation() != nullptr);
}

TEST_CASE("animation state with no animation doesn't crash", "[file]")
{
    auto file = ReadRiveFile("assets/multiple_state_machines.riv");

    auto artboard = file->artboard();
    REQUIRE(artboard != nullptr);
    REQUIRE(artboard->animationCount() == 1);
    REQUIRE(artboard->stateMachineCount() == 4);

    auto stateMachine = artboard->stateMachine("two");
    REQUIRE(stateMachine != nullptr);

    REQUIRE(stateMachine->layerCount() == 1);
    auto layer = stateMachine->layer(0);
    REQUIRE(layer->stateCount() == 4);

    REQUIRE(layer->anyState() != nullptr);
    REQUIRE(layer->entryState() != nullptr);
    REQUIRE(layer->exitState() != nullptr);

    REQUIRE(layer->state(3)->is<rive::AnimationState>());

    auto animationState = layer->state(3)->as<rive::AnimationState>();
    REQUIRE(animationState->animation() == nullptr);

    auto abi = artboard->instance();
    rive::StateMachineInstance(stateMachine, abi.get()).advance(0.0f);
}

TEST_CASE("1D blend state keeps keepsGoing true even when animations "
          "themselves have stopped",
          "[file]")
{
    auto file = ReadRiveFile("assets/oneshotblend.riv");

    auto artboard = file->artboard();
    auto stateMachine = artboard->stateMachine("State Machine 1");

    auto abi = artboard->instance();
    rive::StateMachineInstance* stateMachineInstance =
        new rive::StateMachineInstance(stateMachine, abi.get());
    stateMachineInstance->advance(0.0f);
    REQUIRE(stateMachineInstance->needsAdvance() == true);

    // after advancing into the 1DBlendState we still need to keep going.
    stateMachineInstance->advance(0.5f);
    REQUIRE(stateMachineInstance->needsAdvance() == true);

    // even after advancing past the duration of the animations in the blend
    // states we need to keep going.
    stateMachineInstance->advance(1.0f);
    REQUIRE(stateMachineInstance->needsAdvance() == true);

    delete stateMachineInstance;
}

TEST_CASE("Transitions with duration completes the state correctly before "
          "changing states",
          "[file]")
{
    auto file = ReadRiveFile("assets/state_machine_transition.riv");
    auto black_color = 0xFF000000;
    auto white_color = 0xFFFFFFFF;

    auto artboard = file->artboard();
    auto stateMachine = artboard->stateMachine("State-Machine-Test");

    REQUIRE(artboard != nullptr);
    REQUIRE(artboard->animationCount() == 3);
    REQUIRE(artboard->stateMachineCount() == 1);

    REQUIRE(stateMachine->layerCount() == 1);
    auto layer = stateMachine->layer(0);
    REQUIRE(layer->stateCount() == 6);

    auto abi = artboard->instance();
    REQUIRE(abi->children()[0]->is<rive::Shape>());
    REQUIRE(abi->children()[0]->name() == "Star-Stroke");
    auto shape = abi->children()[0]->as<rive::Shape>();
    REQUIRE(shape->children()[1]->is<rive::Stroke>());
    auto stroke = shape->children()[1]->as<rive::Stroke>();
    REQUIRE(stroke->paint()->is<rive::SolidColor>());
    auto solidColor = stroke->paint()->as<rive::SolidColor>();
    // Before the transition, the color has to be full black
    REQUIRE(solidColor->colorValue() == black_color);
    rive::StateMachineInstance* stateMachineInstance =
        new rive::StateMachineInstance(stateMachine, abi.get());
    stateMachineInstance->advanceAndApply(0.1f);
    abi->advance(0.1f);
    REQUIRE(stateMachineInstance->currentAnimationByIndex(0)->name() ==
            "State-2");
    // After the transition has passed, the color has to be full white.
    stateMachineInstance->advanceAndApply(2.0f);
    abi->advance(2.0f);
    REQUIRE(stateMachineInstance->currentAnimationByIndex(0)->name() ==
            "State-3");
    REQUIRE(solidColor->colorValue() == white_color);

    delete stateMachineInstance;
}

TEST_CASE("Blend state animations with reset applied to them.", "[file]")
{
    auto file = ReadRiveFile("assets/animation_reset_cases.riv");

    auto artboard = file->artboard();
    auto stateMachine = artboard->stateMachine("blend-states-state-machine");

    // We empty all factory reset resources to start the test clean
    rive::AnimationResetFactory::releaseResources();
    REQUIRE(rive::AnimationResetFactory::resourcesCount() == 0);

    REQUIRE(artboard != nullptr);
    REQUIRE(artboard->animationCount() == 9);
    REQUIRE(artboard->stateMachineCount() == 1);

    auto abi = artboard->instance();
    rive::StateMachineInstance* stateMachineInstance =
        new rive::StateMachineInstance(stateMachine, abi.get());
    stateMachineInstance->advanceAndApply(0.1f);
    abi->advance(0.1f);

    auto blendValueNumber = stateMachineInstance->getNumber("blend-value");
    REQUIRE(blendValueNumber != nullptr);
    REQUIRE(blendValueNumber->value() == 0);
    blendValueNumber->value(50);
    REQUIRE(blendValueNumber->value() == 50);
    stateMachineInstance->advanceAndApply(0.1f);
    abi->advance(0.1f);

    auto rect1 = abi->children()[9]->as<rive::Shape>();
    REQUIRE(rect1->name() == "rect1");

    auto rect2 = abi->children()[7]->as<rive::Shape>();
    REQUIRE(rect2->name() == "rect2");

    auto triangle = abi->children()[5]->as<rive::Shape>();
    REQUIRE(triangle->name() == "triangle");

    // This blend rotates 2 * Pi. At 50% it should have rotated 1 * Pi
    REQUIRE(rect1->rotation() == Approx(3.141592f));

    auto state2Bool = stateMachineInstance->getBool("state-2");
    REQUIRE(state2Bool != nullptr);
    REQUIRE(state2Bool->value() == false);
    state2Bool->value(true);
    blendValueNumber->value(50);
    stateMachineInstance->advanceAndApply(0.1f);
    abi->advance(0.1f);
    stateMachineInstance->advanceAndApply(0.1f);
    REQUIRE(rive::AnimationResetFactory::resourcesCount() == 0);
    REQUIRE(state2Bool->value() == true);
    // X and Y interpolation in this state ranges are [75; 425] and [50; 450]
    // so at 50% they should be at 250, 250
    REQUIRE(rect1->x() == 250.0f);
    REQUIRE(rect1->y() == 250.0f);
    // rect2 rotation range is [0; -360]
    // so at 50% it should be at -180
    REQUIRE(rect2->rotation() == Approx(-3.141592f));

    auto state3Bool = stateMachineInstance->getBool("state-3");
    REQUIRE(state3Bool != nullptr);
    REQUIRE(state3Bool->value() == false);
    state3Bool->value(true);
    REQUIRE(rive::AnimationResetFactory::resourcesCount() == 0);
    stateMachineInstance->advanceAndApply(0.1f);
    abi->advance(0.1f);
    stateMachineInstance->advanceAndApply(0.1f);
    blendValueNumber->value(100);
    abi->advance(0.1f);
    stateMachineInstance->advanceAndApply(0.1f);
    REQUIRE(state3Bool->value() == true);
    REQUIRE(triangle->y() == Approx(43.13281f));

    REQUIRE(rive::AnimationResetFactory::resourcesCount() == 1);

    auto state4Bool = stateMachineInstance->getBool("state-4");
    REQUIRE(state4Bool != nullptr);
    REQUIRE(state4Bool->value() == false);
    state4Bool->value(true);
    stateMachineInstance->advanceAndApply(0.1f);
    abi->advance(0.1f);
    REQUIRE(state4Bool->value() == true);
    REQUIRE(rive::AnimationResetFactory::resourcesCount() == 2);

    state4Bool->value(false);
    stateMachineInstance->advanceAndApply(0.1f);
    abi->advance(0.1f);
    stateMachineInstance->advanceAndApply(0.1f);
    REQUIRE(state4Bool->value() == false);
    // After switching states mutiple times resources stay at 2 because they are
    // released and retrieved from the pool
    REQUIRE(rive::AnimationResetFactory::resourcesCount() == 2);
    delete stateMachineInstance;
}

TEST_CASE("Transitions with reset applied to them.", "[file]")
{
    auto file = ReadRiveFile("assets/animation_reset_cases.riv");

    auto artboard = file->artboard("transitions");
    auto stateMachine = artboard->stateMachine("transitions-state-machine");

    // We empty all factory reset resources to start the test clean
    rive::AnimationResetFactory::releaseResources();
    REQUIRE(rive::AnimationResetFactory::resourcesCount() == 0);

    REQUIRE(artboard != nullptr);
    REQUIRE(artboard->animationCount() == 5);
    REQUIRE(artboard->stateMachineCount() == 1);

    auto abi = artboard->instance();
    rive::StateMachineInstance* stateMachineInstance =
        new rive::StateMachineInstance(stateMachine, abi.get());
    stateMachineInstance->advanceAndApply(0.1f);
    abi->advance(0.1f);

    auto rect = abi->children()[7]->as<rive::Shape>();
    REQUIRE(rect->name() == "rectangle");
    REQUIRE(rect->x() == 50);

    auto ellipse = abi->children()[5]->as<rive::Shape>();
    REQUIRE(ellipse->name() == "ellipse");
    REQUIRE(ellipse->x() == Approx(440.31241));

    auto stateNumber = stateMachineInstance->getNumber("Number 1");
    REQUIRE(stateNumber != nullptr);
    REQUIRE(stateNumber->value() == 0);
    stateNumber->value(1);
    REQUIRE(stateNumber->value() == 1);
    stateMachineInstance->advanceAndApply(0.1f);
    abi->advance(0.1f);
    stateMachineInstance->advanceAndApply(1.25f);

    // rect transitions in 2.5 secs from x->50 to x->433
    // so if the translation is linear, after 1.25s it should have
    // traversed half the path
    REQUIRE(rect->x() == 241.5f);

    stateNumber->value(2);
    REQUIRE(stateNumber->value() == 2);
    stateMachineInstance->advanceAndApply(0.1f);
    abi->advance(0.1f);
    stateMachineInstance->advanceAndApply(1.25f);
    // range is [440.31241; 42.69]
    // half if the path is 42.69 + (440.21241 - 42.60) = 241.4962
    REQUIRE(ellipse->x() == Approx(241.49992f));

    // Transitions release their instance immediately so it's available for the
    // next instance to use
    REQUIRE(rive::AnimationResetFactory::resourcesCount() == 0);

    stateNumber->value(3);
    REQUIRE(stateNumber->value() == 3);
    stateMachineInstance->advanceAndApply(0.1f);
    abi->advance(0.1f);
    stateMachineInstance->advanceAndApply(1.25f);

    REQUIRE(rive::AnimationResetFactory::resourcesCount() == 0);

    stateNumber->value(4);
    REQUIRE(stateNumber->value() == 4);
    stateMachineInstance->advanceAndApply(0.1f);
    abi->advance(0.1f);
    stateMachineInstance->advanceAndApply(1.25f);

    // The last two states don't have a transition with duration set so the
    // instance is released and available
    REQUIRE(rive::AnimationResetFactory::resourcesCount() == 1);

    delete stateMachineInstance;
}

TEST_CASE("Triggers will only be used on allowed state changes.", "[file]")
{
    auto file = ReadRiveFile("assets/state_machine_triggers.riv");

    auto artboard = file->artboard("main");

    // We empty all factory reset resources to start the test clean
    rive::AnimationResetFactory::releaseResources();
    REQUIRE(rive::AnimationResetFactory::resourcesCount() == 0);

    REQUIRE(artboard != nullptr);
    REQUIRE(artboard->animationCount() == 1);
    REQUIRE(artboard->stateMachineCount() == 1);

    auto stateMachine = artboard->stateMachine("State Machine 1");
    REQUIRE(stateMachine->layerCount() == 1);
    REQUIRE(stateMachine->inputCount() == 1);

    auto abi = artboard->instance();
    rive::StateMachineInstance* stateMachineInstance =
        new rive::StateMachineInstance(stateMachine, abi.get());
    stateMachineInstance->advanceAndApply(0.1f);
    abi->advance(0.1f);

    auto trigger = stateMachine->input("Trigger 1");
    REQUIRE(trigger != nullptr);
    REQUIRE(trigger->is<rive::StateMachineTrigger>());

    auto layer = stateMachine->layer(0);
    REQUIRE(layer->stateCount() == 5);

    REQUIRE(layer->anyState() != nullptr);
    REQUIRE(layer->entryState() != nullptr);
    REQUIRE(layer->exitState() != nullptr);

    rive::AnimationState* animationState1 = nullptr;
    rive::AnimationState* animationState2 = nullptr;

    int foundAnimationStates = 0;
    for (int i = 0; i < layer->stateCount(); i++)
    {
        auto state = layer->state(i);
        if (state->is<rive::AnimationState>())
        {
            foundAnimationStates++;
            REQUIRE(state->as<rive::AnimationState>()->animation() != nullptr);
            if (foundAnimationStates == 1)
            {
                animationState1 = state->as<rive::AnimationState>();
            }
            else
            {
                animationState2 = state->as<rive::AnimationState>();
            }
        }
    }
    REQUIRE(foundAnimationStates == 2);
    REQUIRE(animationState1 != nullptr);
    REQUIRE(animationState2 != nullptr);

    auto triggerInstance = stateMachineInstance->getTrigger("Trigger 1");
    REQUIRE(triggerInstance != nullptr);

    triggerInstance->fire();
    stateMachineInstance->advanceAndApply(0.1f);
    auto currentLayerState = stateMachineInstance->layerState(0);

    REQUIRE(currentLayerState == animationState2);

    triggerInstance->fire();
    stateMachineInstance->advanceAndApply(0.1f);
    currentLayerState = stateMachineInstance->layerState(0);

    REQUIRE(currentLayerState == animationState1);

    delete stateMachineInstance;
}

TEST_CASE("Transition with list index can be compared to a number", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/transition_index_condition.riv", &silver);

    auto artboard = file->artboardDefault();
    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.0f);
    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    CHECK(silver.matches("transition_index_condition"));
}

TEST_CASE("Listeners are sorted in the right order", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/sorted_listeners.riv", &silver);

    auto artboard = file->artboardDefault();
    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);

    auto vmi = file->createViewModelInstance(artboard.get());

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.0f);
    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    // Down (Move + Down happen on the same frame, but down happens after move)
    silver.addFrame();
    stateMachine->pointerDown(Vec2D(250.0f, 250.0f), 0);
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    // Click (Up + Click on same frame, but click happens after up)
    silver.addFrame();
    stateMachine->pointerUp(Vec2D(250.0f, 250.0f), 0);
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    // Exit
    silver.addFrame();
    stateMachine->pointerMove(Vec2D(0.0f, 0.0f), 0, 0);
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    // Drag gesture
    silver.addFrame();
    stateMachine->pointerDown(Vec2D(250.0f, 250.0f), 0);
    stateMachine->pointerMove(Vec2D(251.0f, 251.0f), 0);
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    // Up
    silver.addFrame();
    stateMachine->pointerUp(Vec2D(251.0f, 251.0f), 0);
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    // Exit
    silver.addFrame();
    stateMachine->pointerMove(Vec2D(0.0f, 0.0f), 0);
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    // (Move): Enter + Move, but move happens after enter
    silver.addFrame();
    stateMachine->pointerMove(Vec2D(251.0f, 251.0f), 0);
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("sorted_listeners"));
}