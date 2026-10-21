#include <rive/animation/linear_animation.hpp>
#include <rive/animation/state_machine.hpp>
#include <rive/artboard.hpp>
#include <rive/file.hpp>
#include "rive_file_reader.hpp"
#include <catch.hpp>

// An instance does not copy its source's animation / state machine lists --
// those objects are owned by the source and shared, so the instance reads
// them straight off artboardSource(). These cases pin the behaviour that the
// copy used to provide.

TEST_CASE(
    "an instance reports its source's animations and state machines",
    "[artboard]")
{
  auto file = ReadRiveFile("assets/multiple_state_machines.riv");

  rive::Artboard* source = file->artboard();
  REQUIRE(source != nullptr);
  REQUIRE(source->stateMachineCount() > 0);

  auto instance = file->artboardDefault();
  REQUIRE(instance != nullptr);
  REQUIRE(instance->isInstance());
  REQUIRE(instance->artboardSource() == source);

  // Counts agree, and the instance hands back the very same (shared) objects.
  REQUIRE(instance->animationCount() == source->animationCount());
  REQUIRE(instance->stateMachineCount() == source->stateMachineCount());

  for (size_t i = 0; i < source->animationCount(); i++)
  {
    REQUIRE(instance->animation(i) == source->animation(i));
    REQUIRE(instance->animationNameAt(i) == source->animationNameAt(i));
    // Resolving by name finds the same object as resolving by index.
    REQUIRE(
        instance->animation(source->animationNameAt(i)) ==
        source->animation(i));
  }
  for (size_t i = 0; i < source->stateMachineCount(); i++)
  {
    REQUIRE(instance->stateMachine(i) == source->stateMachine(i));
    REQUIRE(instance->stateMachineNameAt(i) == source->stateMachineNameAt(i));
    REQUIRE(
        instance->stateMachine(source->stateMachineNameAt(i)) ==
        source->stateMachine(i));
  }

  // Out of range still reports nothing rather than reading off the end.
  REQUIRE(instance->animation(source->animationCount()) == nullptr);
  REQUIRE(instance->stateMachine(source->stateMachineCount()) == nullptr);

  REQUIRE(
      instance->defaultStateMachineIndex() ==
      source->defaultStateMachineIndex());
}

TEST_CASE(
    "an instance of an instance still reaches the original source",
    "[artboard]")
{
  auto file = ReadRiveFile("assets/multiple_state_machines.riv");
  rive::Artboard* source = file->artboard();

  auto instance = file->artboardDefault();
  auto nested = instance->instance();
  REQUIRE(nested != nullptr);
  // instance() forwards the source rather than pointing at the instance.
  REQUIRE(nested->artboardSource() == source);
  REQUIRE(nested->stateMachineCount() == source->stateMachineCount());
  REQUIRE(nested->animationCount() == source->animationCount());
}

TEST_CASE("an instance's object list carries no spare capacity", "[artboard]")
{
  auto file = ReadRiveFile("assets/multiple_state_machines.riv");
  auto instance = file->artboardDefault();
  REQUIRE(instance != nullptr);

  // instance() reserves the exact object count up front, so the clone holds
  // no geometric-growth slack. Guards the reserve against being dropped.
  const std::vector<rive::Core*>& objects = instance->objects();
  REQUIRE(objects.size() > 1);
  REQUIRE(objects.capacity() == objects.size());
}
