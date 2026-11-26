#include <rive/node.hpp>
#include <rive/nested_artboard.hpp>
#include <rive/animation/state_machine_instance.hpp>
#include <rive/viewmodel/viewmodel_instance_number.hpp>
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include <catch.hpp>

// A nested remap animation's time can be written after the frame's advance
// already applied it: by a data bind or a joystick in the update pass, or by a
// host's animation that one of those applied. The new time still shows in that
// frame, with the host's animations still composed in order. Source:
// assets/nested_remap_time_lag.rml.

namespace
{
// A host artboard whose nested artboard "leaf" has a remap bound to the view
// model's "t", next to other nested animations on the same leaf.
struct BoundLeaf
{
  rive::rcp<rive::File> file;
  std::unique_ptr<rive::ArtboardInstance> artboard;
  std::unique_ptr<rive::StateMachineInstance> machine;
  rive::rcp<rive::ViewModelInstance> viewModelInstance;
  rive::Node* dot = nullptr;
  rive::ViewModelInstanceNumber* time = nullptr;
};

BoundLeaf loadBoundLeaf(const char* artboardName)
{
  BoundLeaf rig;
  rig.file = ReadRiveFile("assets/nested_remap_time_lag.riv");
  rig.artboard = rig.file->artboard(artboardName)->instance();
  rig.viewModelInstance = rig.file->createDefaultViewModelInstance(
      rig.artboard.get());
  rig.machine = rig.artboard->defaultStateMachine();
  rig.machine->bindViewModelInstance(rig.viewModelInstance);
  rig.machine->advanceAndApply(0.0f);
  rig.dot = rig.artboard->find<rive::NestedArtboard>("leaf")
                ->artboardInstance()
                ->find<rive::Node>("dot");
  rig.time = rig.viewModelInstance->propertyValue("t")
                 ->as<rive::ViewModelInstanceNumber>();
  return rig;
}
} // namespace

TEST_CASE("a bound nested remap time shows the same frame", "[nested_remap]")
{
  auto file = ReadRiveFile("assets/nested_remap_time_lag.riv");
  auto artboard = file->artboard("bound_remap")->instance();
  auto viewModelInstance = file->createDefaultViewModelInstance(artboard.get());
  auto machine = artboard->defaultStateMachine();
  machine->bindViewModelInstance(viewModelInstance);
  machine->advanceAndApply(0.0f);

  auto leaf = artboard->find<rive::NestedArtboard>("leaf")->artboardInstance();
  auto dot = leaf->find<rive::Node>("dot");
  REQUIRE(dot != nullptr);
  auto time = viewModelInstance->propertyValue("t")
                  ->as<rive::ViewModelInstanceNumber>();
  for (int i = 1; i <= 4; i++)
  {
    time->propertyValue(0.25f * i);
    machine->advanceAndApply(1.0f / 60.0f);
    CHECK(dot->x() == Approx(25.0f * i));
  }
}

TEST_CASE(
    "a remap time a joystick-driven host remap keys shows the same frame",
    "[nested_remap]")
{
  // The joystick keys the middle artboard's remap time and re-applies that
  // remap right away. The middle artboard's animation then keys the leaf's
  // remap time, after the leaf already advanced this frame.
  auto file = ReadRiveFile("assets/nested_remap_time_lag.riv");
  auto artboard = file->artboard("joystick_chain")->instance();
  auto viewModelInstance = file->createDefaultViewModelInstance(artboard.get());
  auto machine = artboard->defaultStateMachine();
  machine->bindViewModelInstance(viewModelInstance);
  machine->advanceAndApply(0.0f);

  auto middle = artboard->find<rive::NestedArtboard>("middle")
                    ->artboardInstance();
  auto leaf = middle->find<rive::NestedArtboard>("leaf")->artboardInstance();
  auto dot = leaf->find<rive::Node>("dot");
  REQUIRE(dot != nullptr);
  auto x = viewModelInstance->propertyValue("x")
               ->as<rive::ViewModelInstanceNumber>();
  for (float value : {-0.5f, 0.0f, 0.5f, 1.0f})
  {
    x->propertyValue(value);
    machine->advanceAndApply(1.0f / 60.0f);
    // Joystick x -1..1 maps to 0..1 of each timeline down the chain.
    CHECK(dot->x() == Approx((value + 1.0f) * 50.0f));
  }
}

TEST_CASE(
    "a later remap still wins after an earlier one's bound time changes",
    "[nested_remap]")
{
  // "bound" keys the dot's x from t, then "fixed" keys it at time 0.5.
  auto rig = loadBoundLeaf("overlapping_remaps");
  REQUIRE(rig.dot != nullptr);
  for (int i = 1; i <= 4; i++)
  {
    rig.time->propertyValue(0.25f * i);
    rig.machine->advanceAndApply(1.0f / 60.0f);
    CHECK(rig.dot->x() == Approx(50.0f));
  }
}

TEST_CASE(
    "a later simple animation still wins after a remap's bound time "
    "changes",
    "[nested_remap]")
{
  // The simple animation holds the dot's x at 75 after the bound remap.
  auto rig = loadBoundLeaf("remap_then_simple");
  REQUIRE(rig.dot != nullptr);
  for (int i = 1; i <= 4; i++)
  {
    rig.time->propertyValue(0.25f * i);
    rig.machine->advanceAndApply(1.0f / 60.0f);
    CHECK(rig.dot->x() == Approx(75.0f));
  }
}

TEST_CASE(
    "a remap before a nested state machine waits for the next frame",
    "[nested_remap]")
{
  // The state machine can't apply again without advancing, so the remap's new
  // time isn't applied in the settle pass: the machine's x of 75 stays on top.
  auto rig = loadBoundLeaf("remap_then_state_machine");
  REQUIRE(rig.dot != nullptr);
  for (int i = 1; i <= 4; i++)
  {
    rig.time->propertyValue(0.25f * i);
    rig.machine->advanceAndApply(1.0f / 60.0f);
    CHECK(rig.dot->x() == Approx(75.0f));
  }
}

TEST_CASE(
    "a remap after a nested state machine shows its bound time that frame",
    "[nested_remap]")
{
  auto rig = loadBoundLeaf("state_machine_then_remap");
  REQUIRE(rig.dot != nullptr);
  for (int i = 1; i <= 4; i++)
  {
    rig.time->propertyValue(0.25f * i);
    rig.machine->advanceAndApply(1.0f / 60.0f);
    CHECK(rig.dot->x() == Approx(25.0f * i));
  }
}

TEST_CASE(
    "a remap at a partial mix is not blended twice in a frame",
    "[nested_remap]")
{
  // At mix 0.5 each apply blends halfway from the current x. Applying again in
  // the settle pass would blend a second time, so the new time waits for the
  // next frame's advance, as it did before remaps applied in settle passes.
  auto rig = loadBoundLeaf("half_mix_remap");
  REQUIRE(rig.dot != nullptr);
  REQUIRE(rig.dot->x() == Approx(0.0f));
  rig.time->propertyValue(1.0f);
  rig.machine->advanceAndApply(1.0f / 60.0f);
  CHECK(rig.dot->x() == Approx(0.0f));
  rig.machine->advanceAndApply(1.0f / 60.0f);
  CHECK(rig.dot->x() == Approx(50.0f));
  rig.machine->advanceAndApply(1.0f / 60.0f);
  CHECK(rig.dot->x() == Approx(75.0f));
}
