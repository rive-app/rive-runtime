#include "rive/file.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/data_bind/converters/data_converter_group.hpp"
#include "rive/data_bind/converters/data_converter_group_item.hpp"
#include "rive/data_bind/converters/data_converter_interpolator.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/data_bind/data_values/data_value_string.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/viewmodel/viewmodel_instance_color.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "rive/viewmodel/viewmodel_instance_trigger.hpp"
#include "rive/viewmodel/viewmodel_instance_list.hpp"
#include "utils/serializing_factory.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <cstdio>
#include <cstring>

using namespace rive;

TEST_CASE("list to length converter", "[silver]")
{
  SerializingFactory silver;
  auto file = ReadRiveFile("assets/list_to_length_test.riv", &silver);

  auto artboard = file->artboardDefault();

  silver.frameSize(artboard->width(), artboard->height());

  REQUIRE(artboard != nullptr);
  auto stateMachine = artboard->stateMachineAt(0);
  int viewModelId = artboard.get()->viewModelId();

  auto vmi = viewModelId == -1 ? file->createViewModelInstance(artboard.get())
                               : file->createViewModelInstance(viewModelId, 0);

  stateMachine->bindViewModelInstance(vmi);
  stateMachine->advanceAndApply(0.1f);

  auto renderer = silver.makeRenderer();
  artboard->draw(renderer.get());

  auto list = vmi->propertyValue("lis");
  auto childVM = file->viewModel("child");
  int cnt = 0;
  while (cnt++ < 4)
  {
    silver.addFrame();
    auto childVMI = file->createDefaultViewModelInstance(childVM);
    if (childVMI != nullptr)
    {
      auto listItem = make_rcp<ViewModelInstanceListItem>();
      listItem->viewModelInstance(childVMI);
      list->as<ViewModelInstanceList>()->addItem(listItem);
    }
    // first advance to set view model value
    stateMachine->advanceAndApply(0.1f);
    // second advance to measure
    stateMachine->advanceAndApply(0.1f);

    artboard->draw(renderer.get());
  }

  CHECK(silver.matches("list_to_length_test"));
}

TEST_CASE("data converter interpolator resets on binding", "[silver]")
{
  SerializingFactory silver;
  auto file = ReadRiveFile(
      "assets/data_converter_interpolator_reset.riv",
      &silver);

  auto artboard = file->artboardDefault();

  silver.frameSize(artboard->width(), artboard->height());

  REQUIRE(artboard != nullptr);
  auto stateMachine = artboard->stateMachineAt(0);
  auto renderer = silver.makeRenderer();
  int viewModelId = artboard.get()->viewModelId();
  {
    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);
    auto numProp = vmi->propertyValue("xPos")->as<ViewModelInstanceNumber>();
    numProp->propertyValue(250);
    auto colProp = vmi->propertyValue("col")->as<ViewModelInstanceColor>();
    auto redColor = (255 << 24) | (255 << 16);
    colProp->propertyValue(redColor);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);

    artboard->draw(renderer.get());

    auto greenColor = (255 << 24) | (255 << 8);
    colProp->propertyValue(greenColor);
    numProp->propertyValue(500);

    int frames = (int)(1.0f / 0.016f);
    for (int i = 0; i < frames; i++)
    {
      silver.addFrame();
      stateMachine->advanceAndApply(0.016f);
      artboard->draw(renderer.get());
    }
  }
  // When a new binding is applied, interpolators are reset and the initial
  // value is not interpolated
  {
    silver.addFrame();
    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);
    auto numProp = vmi->propertyValue("xPos")->as<ViewModelInstanceNumber>();
    numProp->propertyValue(250);
    auto colProp = vmi->propertyValue("col")->as<ViewModelInstanceColor>();
    auto redColor = (255 << 24) | (255 << 16);
    colProp->propertyValue(redColor);
    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);

    artboard->draw(renderer.get());

    auto blueColor = (255 << 24) | 255;
    colProp->propertyValue(blueColor);
    numProp->propertyValue(0);

    int frames = (int)(1.0f / 0.016f);
    for (int i = 0; i < frames; i++)
    {
      silver.addFrame();
      stateMachine->advanceAndApply(0.016f);
      artboard->draw(renderer.get());
    }
  }

  CHECK(silver.matches("data_converter_interpolator_reset"));
}

namespace
{
// One frame as the runtime runs it: advance the interpolator, then convert the
// bound value through it.
float stepInterpolator(DataConverterInterpolator& converter, float target)
{
  converter.advance(1.0f / 60.0f);
  DataValueNumber input(target);
  return converter.convert(&input, nullptr)->as<DataValueNumber>()->value();
}

// Runs frames until the value lands on target, checking that every frame moves
// the value. Returns the number of frames taken.
int tweenTo(DataConverterInterpolator& converter, float target)
{
  float previous = stepInterpolator(converter, target);
  int frames = 0;
  while (previous != target && frames < 600)
  {
    float value = stepInterpolator(converter, target);
    INFO("frame " << frames << " value " << value);
    CHECK(value != previous);
    previous = value;
    frames++;
  }
  REQUIRE(previous == target);
  return frames;
}
} // namespace

TEST_CASE(
    "data converter interpolator restarts without holding a frame",
    "[data binding]")
{
  DataConverterInterpolator converter;
  converter.duration(0.375f);
  DataValueNumber initial(0);
  converter.convert(&initial, nullptr);
  // Get past the first run, which snaps to the value instead of tweening.
  stepInterpolator(converter, 0);
  stepInterpolator(converter, 0);

  int firstTweenFrames = tweenTo(converter, 300);

  // A new target after the previous tween landed is a fresh tween, not a
  // smoothing from the finished one.
  CHECK(tweenTo(converter, 0) == firstTweenFrames);
  CHECK(tweenTo(converter, 300) == firstTweenFrames);

  // A new target mid tween smooths from the old one. Its value must keep
  // moving on the frame the old tween finishes.
  for (int i = 0; i < 12; i++)
  {
    stepInterpolator(converter, 0);
  }
  tweenTo(converter, 300);
}

namespace
{
// One frame as stepInterpolator runs it, checking that a converter that said
// it was quiet does nothing in it: its advance reports no work and the value
// stays on its target. Returns whether it was quiet.
bool stepChecked(
    DataConverter& converter,
    float target,
    AdvancingComponent::QuietState& state)
{
  state = converter.advanceQuietState();
  DataValueNumber input(target);
  bool advanced = converter.advance(1.0f / 60.0f);
  float value = converter.convert(&input, nullptr)
                    ->as<DataValueNumber>()
                    ->value();
  if (state == AdvancingComponent::QuietState::quiet)
  {
    CHECK_FALSE(advanced);
    CHECK(value == target);
  }
  return state == AdvancingComponent::QuietState::quiet;
}
} // namespace

// A list row skips its frame while every converter its binds use would do
// nothing (see quiet_rows_test.cpp). An interpolator would do nothing once its
// value has landed, until convert gives it a new one, which only happens for a
// dirty bind, and that wakes the row.
TEST_CASE(
    "data converter interpolator is quiet once its value lands",
    "[data binding]")
{
  using QuietState = AdvancingComponent::QuietState;
  DataConverterInterpolator converter;
  converter.duration(0.375f);
  // Nothing to animate yet, and advance reports work while it has nothing.
  CHECK(converter.advanceQuietState() == QuietState::busy);
  DataValueNumber initial(0);
  converter.convert(&initial, nullptr);
  // The first run snaps to the value, and its advances count the frames that
  // decide that.
  CHECK(converter.advanceQuietState() == QuietState::busy);
  stepInterpolator(converter, 0);
  CHECK(converter.advanceQuietState() == QuietState::busy);
  stepInterpolator(converter, 0);
  CHECK(converter.advanceQuietState() == QuietState::quiet);

  auto state = QuietState::never;
  // A new value is in flight until it lands.
  DataValueNumber target(300);
  converter.convert(&target, nullptr);
  CHECK(converter.advanceQuietState() == QuietState::busy);
  int frames = 0;
  while (!stepChecked(converter, 300, state) && frames < 600)
  {
    CHECK(state == QuietState::busy);
    frames++;
  }
  CHECK(frames > 1);

  // A new value mid tween smooths from the old one, and stays in flight until
  // the smoothed one lands.
  for (int i = 0; i < 12; i++)
  {
    stepInterpolator(converter, 0);
  }
  CHECK(converter.advanceQuietState() == QuietState::busy);
  DataValueNumber retarget(300);
  converter.convert(&retarget, nullptr);
  frames = 0;
  while (!stepChecked(converter, 300, state) && frames < 600)
  {
    CHECK(state == QuietState::busy);
    frames++;
  }
  CHECK(frames > 12);
  CHECK(converter.advanceQuietState() == QuietState::quiet);
}

// A group is as quiet as its items' converters.
TEST_CASE(
    "data converter group is quiet once its interpolator lands",
    "[data binding]")
{
  using QuietState = AdvancingComponent::QuietState;
  DataConverterGroup group;
  auto interpolator = new DataConverterInterpolator();
  interpolator->duration(0.375f);
  auto item = new DataConverterGroupItem();
  item->converter(interpolator);
  item->ownsConverter(true);
  group.addItem(item);

  DataValueNumber initial(0);
  group.convert(&initial, nullptr);
  CHECK(group.advanceQuietState() == QuietState::busy);
  auto state = QuietState::never;
  int frames = 0;
  while (!stepChecked(group, 0, state) && frames < 600)
  {
    frames++;
  }
  CHECK(frames == 2);
  DataValueNumber target(300);
  group.convert(&target, nullptr);
  CHECK(group.advanceQuietState() == QuietState::busy);
  frames = 0;
  while (!stepChecked(group, 300, state) && frames < 600)
  {
    frames++;
  }
  CHECK(frames > 1);
  CHECK(interpolator->advanceQuietState() == QuietState::quiet);
}

// An interpolator given a value it can't ease, such as a group's string, never
// sets up anything to animate, and its advance reports work on every frame.
// It can't go quiet, so it says never, which a list caches for the row,
// instead of busy, which would have the row asked again every frame.
TEST_CASE(
    "data converter interpolator that can't ease its value is never quiet",
    "[data binding]")
{
  using QuietState = AdvancingComponent::QuietState;
  DataConverterInterpolator converter;
  converter.duration(0.375f);
  // Before its first value: one is about to arrive.
  CHECK(converter.advanceQuietState() == QuietState::busy);
  DataValueString text("label");
  CHECK(converter.convert(&text, nullptr) == &text);
  CHECK(converter.advanceQuietState() == QuietState::never);
  for (int i = 0; i < 4; i++)
  {
    CHECK(converter.advance(1.0f / 60.0f));
    converter.convert(&text, nullptr);
    CHECK(converter.advanceQuietState() == QuietState::never);
  }

  // A rebind resets it, and a number it can ease lets it go quiet again.
  converter.reset();
  CHECK(converter.advanceQuietState() == QuietState::busy);
  DataValueNumber initial(5);
  converter.convert(&initial, nullptr);
  stepInterpolator(converter, 5);
  stepInterpolator(converter, 5);
  CHECK(converter.advanceQuietState() == QuietState::quiet);
}

TEST_CASE(
    "Interpolations that change duration to zero work correctly",
    "[silver]")
{
  SerializingFactory silver;
  auto file = ReadRiveFile("assets/interpolation_zero_duration.riv", &silver);

  auto artboard = file->artboardDefault();
  REQUIRE(artboard != nullptr);

  silver.frameSize(artboard->width(), artboard->height());

  auto stateMachine = artboard->stateMachineAt(0);
  int viewModelId = artboard.get()->viewModelId();

  auto vmi = viewModelId == -1 ? file->createViewModelInstance(artboard.get())
                               : file->createViewModelInstance(viewModelId, 0);
  auto objectX = vmi->propertyValue("objectX")->as<ViewModelInstanceNumber>();
  auto interpValue = vmi->propertyValue("interpValue")
                         ->as<ViewModelInstanceNumber>();

  stateMachine->bindViewModelInstance(vmi);
  stateMachine->advanceAndApply(0.1f);
  auto renderer = silver.makeRenderer();
  artboard->draw(renderer.get());

  objectX->propertyValue(200);

  int frames = (int)(1.5f / 0.1f);
  for (int i = 0; i < frames; i++)
  {
    silver.addFrame();
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());
  }

  interpValue->propertyValue(0);
  stateMachine->advanceAndApply(0.016f);
  objectX->propertyValue(400);
  stateMachine->advanceAndApply(0.016f);
  for (int i = 0; i < frames; i++)
  {
    silver.addFrame();
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());
  }

  interpValue->propertyValue(1);
  stateMachine->advanceAndApply(0.016f);
  objectX->propertyValue(200);
  stateMachine->advanceAndApply(0.016f);
  for (int i = 0; i < frames; i++)
  {
    silver.addFrame();
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());
  }

  CHECK(silver.matches("interpolation_zero_duration"));
}