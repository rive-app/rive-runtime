#include <rive/animation/state_machine.hpp>
#include <rive/animation/state_machine_instance.hpp>
#include <rive/animation/state_machine_input_instance.hpp>
#include <rive/animation/state_machine_listener.hpp>
#include <rive/assets/image_asset.hpp>
#include <rive/listener_phase.hpp>
#include <rive/shapes/image.hpp>
#include "rive_file_reader.hpp"
#include "utils/serializing_factory.hpp"
#include <catch.hpp>

using namespace rive;

// assets/rml/image_listener_targets.rml lays six 100x100 images out in a row,
// one per 100px column, each with a pointer down listener flipping the bool
// input of the same name. SerializingFactory decodes the embedded png, which
// NoOpFactory would not, and an image without a decoded image never hits.
TEST_CASE("images are pointer listener targets", "[hittest]")
{
  SerializingFactory factory;
  auto file = ReadRiveFile("assets/image_listener_targets.riv", &factory);
  auto artboard = file->artboardNamed("Images");
  REQUIRE(artboard != nullptr);
  auto stateMachine = artboard->stateMachineAt(0);
  REQUIRE(stateMachine != nullptr);
  stateMachine->advanceAndApply(0.0f);

  // One hit component per image, including the one inside Group and the
  // hidden one.
  CHECK(stateMachine->hasListeners());
  CHECK(stateMachine->hitComponentsCount() == 6);

  auto press = [&](Vec2D position) {
    stateMachine->pointerDown(position);
    stateMachine->pointerUp(position);
    stateMachine->advanceAndApply(0.0f);
  };
  auto pressed = [&](const char* name) {
    auto input = stateMachine->getBool(name);
    REQUIRE(input != nullptr);
    return input->value();
  };

  press(Vec2D(50.0f, 50.0f));
  CHECK(pressed("Direct"));

  // The listener targets the Node; the image inside it is the hit area.
  press(Vec2D(150.0f, 50.0f));
  CHECK(pressed("Group"));

  // Inside the image's box but above the mesh's lower-left triangle.
  press(Vec2D(290.0f, 10.0f));
  CHECK_FALSE(pressed("Meshed"));
  press(Vec2D(210.0f, 90.0f));
  CHECK(pressed("Meshed"));

  // The hidden center patch [325..375] is a hole.
  press(Vec2D(350.0f, 50.0f));
  CHECK_FALSE(pressed("Sliced"));
  press(Vec2D(305.0f, 5.0f));
  CHECK(pressed("Sliced"));

  press(Vec2D(450.0f, 50.0f));
  CHECK_FALSE(pressed("Hidden"));

  // Opacity doesn't matter, as with shapes.
  press(Vec2D(550.0f, 50.0f));
  CHECK(pressed("Transparent"));
}

// A capture listener on an image, or on a group holding one, runs in the
// capture phase, as on a shape, rather than as a normal listener.
TEST_CASE("image capture listeners run in the capture phase", "[hittest]")
{
  for (const char* name : {"Direct", "Group"})
  {
    SerializingFactory factory;
    auto file = ReadRiveFile("assets/image_listener_targets.riv", &factory);
    auto machine = file->artboard("Images")->stateMachine(0);
    REQUIRE(machine != nullptr);
    for (size_t i = 0; i < machine->listenerCount(); i++)
    {
      auto listener = machine->listener(i);
      if (listener->name() == name)
      {
        const_cast<StateMachineListener*>(listener)->phaseValue(
            (uint32_t)ListenerPhase::capture);
      }
    }
    auto artboard = file->artboardNamed("Images");
    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);
    stateMachine->advanceAndApply(0.0f);
    INFO(name);
    CHECK(stateMachine->hasCaptureListeners());

    const Vec2D position(std::string(name) == "Direct" ? 50.0f : 150.0f, 50.0f);
    stateMachine->pointerDown(position);
    stateMachine->pointerUp(position);
    stateMachine->advanceAndApply(0.0f);
    auto input = stateMachine->getBool(name);
    REQUIRE(input != nullptr);
    CHECK(input->value());
  }
}

TEST_CASE("images without a decoded image take no hits", "[hittest]")
{
  SerializingFactory factory;
  auto file = ReadRiveFile("assets/image_listener_targets.riv", &factory);
  auto artboard = file->artboardNamed("Images");
  REQUIRE(artboard != nullptr);
  auto stateMachine = artboard->stateMachineAt(0);
  REQUIRE(stateMachine != nullptr);
  stateMachine->advanceAndApply(0.0f);

  auto image = artboard->find<Image>("Direct");
  REQUIRE(image != nullptr);
  REQUIRE(image->imageAsset() != nullptr);
  image->imageAsset()->renderImage(nullptr);
  stateMachine->advanceAndApply(0.0f);

  stateMachine->pointerDown(Vec2D(50.0f, 50.0f));
  stateMachine->pointerUp(Vec2D(50.0f, 50.0f));
  stateMachine->advanceAndApply(0.0f);
  auto input = stateMachine->getBool("Direct");
  REQUIRE(input != nullptr);
  CHECK_FALSE(input->value());
}

// Before 7.5 images were never hit tested, and a listener on a group holding
// one must not start hitting it: the same scene stamped 7.4 keeps every image
// out of the hit lookup.
TEST_CASE("images in files before 7.5 aren't pointer targets", "[hittest]")
{
  std::vector<uint8_t> bytes = ReadFile("assets/image_listener_targets.riv");
  REQUIRE(bytes.size() > 6);
  REQUIRE(bytes[4] == 7);
  bytes[5] = 4;
  SerializingFactory factory;
  ImportResult result;
  auto file = File::import(bytes, &factory, &result);
  REQUIRE(result == ImportResult::success);
  auto artboard = file->artboardNamed("Images");
  REQUIRE(artboard != nullptr);
  auto image = artboard->find<Image>("Direct");
  REQUIRE(image != nullptr);
  CHECK_FALSE(image->isPointerTarget());

  auto stateMachine = artboard->stateMachineAt(0);
  REQUIRE(stateMachine != nullptr);
  stateMachine->advanceAndApply(0.0f);
  CHECK(stateMachine->hitComponentsCount() == 0);
  CHECK_FALSE(stateMachine->hasListeners());

  for (float x : {50.0f, 150.0f, 550.0f})
  {
    stateMachine->pointerDown(Vec2D(x, 50.0f));
    stateMachine->pointerUp(Vec2D(x, 50.0f));
  }
  stateMachine->advanceAndApply(0.0f);
  for (const char* name : {"Direct", "Group", "Transparent"})
  {
    auto input = stateMachine->getBool(name);
    REQUIRE(input != nullptr);
    CHECK_FALSE(input->value());
  }
}
