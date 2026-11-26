#include <rive/node.hpp>
#include <rive/nested_artboard.hpp>
#include <rive/animation/state_machine_instance.hpp>
#include <rive/viewmodel/viewmodel_instance_number.hpp>
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include <catch.hpp>

// assets/bind_walk_skip.rml: root hosts child, which hosts grandchild. Each
// nested level binds its node "dot".x to the root view model's `a`; only the
// grandchild binds its dot's y, to `b`.
//
// A nested artboard whose instance has no bind work below skips the walk
// into it (NestedArtboard::updateDataBinds). TESTING builds also check, on
// every skip, that nothing below had work.
TEST_CASE(
    "a nested artboard skips the bind walk until work is queued",
    "[bind_walk]")
{
  auto file = ReadRiveFile("assets/bind_walk_skip.riv");
  auto artboard = file->artboard("root")->instance();
  auto viewModelInstance = file->createDefaultViewModelInstance(artboard.get());
  auto machine = artboard->defaultStateMachine();
  machine->bindViewModelInstance(viewModelInstance);
  machine->advanceAndApply(0.0f);

  auto child = artboard->find<rive::NestedArtboard>("child")
                   ->artboardInstance();
  auto grandchild = child->find<rive::NestedArtboard>("grandchild")
                        ->artboardInstance();
  auto childDot = child->find<rive::Node>("dot");
  auto grandchildDot = grandchild->find<rive::Node>("dot");
  REQUIRE(childDot != nullptr);
  REQUIRE(grandchildDot != nullptr);
  CHECK(childDot->x() == Approx(3.0f));
  CHECK(grandchildDot->x() == Approx(3.0f));

  // Settled: nothing is queued below the root, so the walk into child is
  // skipped.
  machine->advanceAndApply(1.0f / 60.0f);
  CHECK(!child->hasBindWorkBelow());
  CHECK(!grandchild->hasBindWorkBelow());
  auto skips = rive::NestedArtboard::sm_bindWalkSkips.load();
  machine->advanceAndApply(1.0f / 60.0f);
  CHECK(rive::NestedArtboard::sm_bindWalkSkips.load() > skips);

  // A change marks the chain the binds it dirties sit on, and both levels
  // apply it in the same frame.
  auto a = viewModelInstance->propertyValue("a")
               ->as<rive::ViewModelInstanceNumber>();
  a->propertyValue(7.0f);
  CHECK(child->hasBindWorkBelow());
  CHECK(grandchild->hasBindWorkBelow());
  machine->advanceAndApply(1.0f / 60.0f);
  CHECK(childDot->x() == Approx(7.0f));
  CHECK(grandchildDot->x() == Approx(7.0f));
  CHECK(!child->hasBindWorkBelow());
  CHECK(!grandchild->hasBindWorkBelow());

  // A change only the grandchild reads queues nothing on child, so only the
  // mark passed up from the grandchild keeps the walk going into child, and
  // through it to the grandchild, in the same frame.
  CHECK(grandchildDot->y() == Approx(5.0f));
  auto b = viewModelInstance->propertyValue("b")
               ->as<rive::ViewModelInstanceNumber>();
  b->propertyValue(11.0f);
  CHECK(!child->hasDataBindWork());
  CHECK(grandchild->hasDataBindWork());
  CHECK(child->hasBindWorkBelow());
  CHECK(grandchild->hasBindWorkBelow());
  machine->advanceAndApply(1.0f / 60.0f);
  CHECK(grandchildDot->y() == Approx(11.0f));
  CHECK(!child->hasBindWorkBelow());
}
