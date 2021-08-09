#include <rive/core/binary_reader.hpp>
#include <rive/file.hpp>
#include <rive/animation/state_machine_bool.hpp>
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
#include "catch.hpp"
#include <cstdio>

TEST_CASE("file with state machine be read", "[file]")
{
	FILE* fp = fopen("../../test/assets/rocket.riv", "r");
	REQUIRE(fp != nullptr);

	fseek(fp, 0, SEEK_END);
	auto length = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	uint8_t* bytes = new uint8_t[length];
	REQUIRE(fread(bytes, 1, length, fp) == length);
	auto reader = rive::BinaryReader(bytes, length);
	rive::File* file = nullptr;
	auto result = rive::File::import(reader, &file);

	REQUIRE(result == rive::ImportResult::success);
	REQUIRE(file != nullptr);
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

	rive::StateMachineInstance smi(artboard->stateMachine("Button"));
	REQUIRE(smi.getBool("Hover")->name() == "Hover");
	REQUIRE(smi.getBool("Press")->name() == "Press");
	REQUIRE(smi.getBool("Hover") != nullptr);
	REQUIRE(smi.getBool("Press") != nullptr);
	REQUIRE(smi.stateChangedCount() == 0);
	REQUIRE(smi.currentAnimationCount() == 0);

	delete file;
	delete[] bytes;
}

TEST_CASE("file with blend states loads correctly", "[file]")
{
	FILE* fp = fopen("../../test/assets/blend_test.riv", "r");
	REQUIRE(fp != nullptr);

	fseek(fp, 0, SEEK_END);
	auto length = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	uint8_t* bytes = new uint8_t[length];
	REQUIRE(fread(bytes, 1, length, fp) == length);
	auto reader = rive::BinaryReader(bytes, length);
	rive::File* file = nullptr;
	auto result = rive::File::import(reader, &file);

	REQUIRE(result == rive::ImportResult::success);
	REQUIRE(file != nullptr);
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

	delete file;
	delete[] bytes;
}

TEST_CASE("animation state with no animation doesn't crash", "[file]")
{
	FILE* fp = fopen("../../test/assets/multiple_state_machines.riv", "r");
	REQUIRE(fp != nullptr);

	fseek(fp, 0, SEEK_END);
	auto length = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	uint8_t* bytes = new uint8_t[length];
	REQUIRE(fread(bytes, 1, length, fp) == length);
	auto reader = rive::BinaryReader(bytes, length);
	rive::File* file = nullptr;
	auto result = rive::File::import(reader, &file);

	REQUIRE(result == rive::ImportResult::success);
	REQUIRE(file != nullptr);
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

	rive::StateMachineInstance smi(stateMachine);
	smi.advance(artboard, 0.0f);

	delete file;
	delete[] bytes;
}