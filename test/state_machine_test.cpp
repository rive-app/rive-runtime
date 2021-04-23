#include "catch.hpp"
#include "core/binary_reader.hpp"
#include "file.hpp"
#include "animation/state_machine_bool.hpp"
#include "animation/state_machine_layer.hpp"
#include "animation/animation_state.hpp"
#include "animation/entry_state.hpp"
#include "animation/state_transition.hpp"
#include "animation/state_machine_instance.hpp"
#include "animation/state_machine_input_instance.hpp"
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