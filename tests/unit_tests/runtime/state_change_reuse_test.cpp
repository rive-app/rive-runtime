#include <rive/file.hpp>
#include <rive/animation/animation_reset.hpp>
#include <rive/animation/animation_reset_factory.hpp>
#include <rive/animation/animation_state.hpp>
#include <rive/animation/animation_state_instance.hpp>
#include <rive/animation/keyed_object.hpp>
#include <rive/animation/keyed_property.hpp>
#include <rive/animation/keyframe_color.hpp>
#include <rive/animation/keyframe_double.hpp>
#include <rive/animation/layer_state.hpp>
#include <rive/animation/linear_animation.hpp>
#include <rive/animation/state_machine.hpp>
#include <rive/animation/state_machine_instance.hpp>
#include <rive/animation/state_machine_layer.hpp>
#include <rive/generated/core_registry.hpp>
#include <rive/viewmodel/viewmodel_instance.hpp>
#include <rive/viewmodel/viewmodel_instance_boolean.hpp>
#include <rive/viewmodel/viewmodel_instance_number.hpp>
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <algorithm>
#include <filesystem>
#include <set>
#include <string>
#include <utility>
#include <vector>

// A state change neither builds its animation reset with scratch containers
// of its own nor makes a new instance of a state the layer just left: the
// reset is written with buffers pooled with it, and the instance left
// behind starts over (StateInstance::restart). Both have to come out as
// they did when made anew.

using namespace rive;

namespace
{
// Assets with a default-constructible file, artboards and all. Some are
// malformed on purpose.
std::vector<std::string> listAssets()
{
  std::vector<std::string> out;
  for (const auto& entry :
       std::filesystem::recursive_directory_iterator("assets"))
  {
    if (entry.is_regular_file() && entry.path().extension() == ".riv")
    {
      out.push_back(entry.path().generic_string());
    }
  }
  std::sort(out.begin(), out.end());
  return out;
}

rcp<File> importAsset(const std::string& path)
{
  std::vector<uint8_t> bytes = ReadFile(path.c_str());
  ImportResult result;
  rcp<File> file = File::import(bytes, &gNoOpFactory, &result);
  return result == ImportResult::success ? file : nullptr;
}

// The reset as AnimationResetFactory wrote it before it built into the
// pooled reset: a set and a vector per keyed object, made for each reset.
void writeTheOldWay(
    AnimationReset* reset,
    const std::vector<const LinearAnimation*>& animations,
    ArtboardInstance* artboard,
    bool useFirstAsBaseline)
{
  struct Property
  {
    const KeyedProperty* keyedProperty;
    bool isBaseline;
  };
  struct Object
  {
    uint32_t objectId;
    std::vector<Property> properties;
    std::set<int> keys;
  };
  std::vector<Object> objects;
  bool isBaseline = useFirstAsBaseline;
  for (auto animation : animations)
  {
    for (size_t o = 0; o < animation->numKeyedObjects(); o++)
    {
      auto keyedObject = animation->getObject(o);
      auto found = std::find_if(
          objects.begin(),
          objects.end(),
          [&](const Object& object) {
            return object.objectId == keyedObject->objectId();
          });
      if (found == objects.end())
      {
        objects.push_back({keyedObject->objectId(), {}, {}});
        found = objects.end() - 1;
      }
      for (size_t k = 0; k < keyedObject->numKeyedProperties(); k++)
      {
        auto keyedProperty = keyedObject->getProperty(k);
        int key = keyedProperty->propertyKey();
        if (found->keys.count(key) != 0)
        {
          continue;
        }
        switch (CoreRegistry::propertyFieldId(key))
        {
          case CoreDoubleType::id:
          case CoreColorType::id:
            found->keys.insert(key);
            found->properties.push_back({keyedProperty, isBaseline});
            break;
        }
      }
    }
    isBaseline = false;
  }
  for (const auto& object : objects)
  {
    auto core = artboard->resolve(object.objectId);
    if (core == nullptr || object.properties.empty())
    {
      continue;
    }
    auto component = core->as<Component>();
    reset->writeObjectId(object.objectId);
    reset->writeTotalProperties((uint32_t)object.properties.size());
    for (const auto& property : object.properties)
    {
      auto key = property.keyedProperty->propertyKey();
      auto first = property.keyedProperty->first();
      reset->writePropertyKey(key);
      if (CoreRegistry::propertyFieldId(key) == CoreDoubleType::id)
      {
        if (!property.isBaseline)
        {
          reset->writePropertyValue(CoreRegistry::getDouble(component, key));
        }
        else if (first != nullptr)
        {
          reset->writePropertyValue(first->as<KeyFrameDouble>()->value());
        }
      }
      else
      {
        if (!property.isBaseline)
        {
          reset->writePropertyValue(
              (float)CoreRegistry::getColor(component, key));
        }
        else if (first != nullptr)
        {
          reset->writePropertyValue((float)first->as<KeyFrameColor>()->value());
        }
      }
    }
  }
  reset->complete();
}

std::vector<uint8_t> bytesTheOldWay(
    const std::vector<const LinearAnimation*>& animations,
    ArtboardInstance* artboard,
    bool useFirstAsBaseline)
{
  AnimationReset reset;
  writeTheOldWay(&reset, animations, artboard, useFirstAsBaseline);
  return reset.bytes();
}

std::vector<uint8_t> bytesBuilt(
    std::vector<const LinearAnimation*> animations,
    ArtboardInstance* artboard,
    bool useFirstAsBaseline)
{
  auto reset = AnimationResetFactory::fromAnimations(
      animations,
      artboard,
      useFirstAsBaseline);
  auto bytes = reset->bytes();
  AnimationResetFactory::release(std::move(reset));
  return bytes;
}
} // namespace

// Every pair of a test asset artboard's first animations, in either order,
// with and without the first as baseline, and its first three together: the
// pooled build writes the bytes the old builder did. The pool is reused
// throughout, so stale scratch from a bigger reset would show.
TEST_CASE(
    "An animation reset is written as it was before it was pooled",
    "[animation reset]")
{
  int compared = 0;
  for (const std::string& path : listAssets())
  {
    auto file = importAsset(path);
    if (file == nullptr)
    {
      continue;
    }
    for (size_t a = 0; a < file->artboardCount(); a++)
    {
      auto artboard = file->artboardAt(a);
      if (artboard == nullptr)
      {
        continue;
      }
      std::vector<const LinearAnimation*> animations;
      for (size_t i = 0; i < artboard->animationCount() && i < 6; i++)
      {
        animations.push_back(artboard->animation(i));
      }
      for (auto first : animations)
      {
        for (auto second : animations)
        {
          for (bool baseline : {false, true})
          {
            std::vector<const LinearAnimation*> pair = {first, second};
            INFO(
                path << " " << first->name() << ", " << second->name()
                     << (baseline ? " (baseline)" : ""));
            REQUIRE(
                bytesBuilt(pair, artboard.get(), baseline) ==
                bytesTheOldWay(pair, artboard.get(), baseline));
            compared++;
          }
        }
      }
      if (animations.size() >= 3)
      {
        animations.resize(3);
        INFO(path << " first three");
        REQUIRE(
            bytesBuilt(animations, artboard.get(), true) ==
            bytesTheOldWay(animations, artboard.get(), true));
        compared++;
      }
    }
  }
  // The walk has to reach some, or matching proves nothing.
  CHECK(compared > 1000);
}

namespace
{
struct Playback
{
  float time;
  float totalTime;
  float lastTotalTime;
  float spilledTime;
  float direction;
  bool didLoop;
  int loopValue;
  bool keepGoing;
  bool operator==(const Playback& other) const
  {
    return time == other.time && totalTime == other.totalTime &&
           lastTotalTime == other.lastTotalTime &&
           spilledTime == other.spilledTime && direction == other.direction &&
           didLoop == other.didLoop && loopValue == other.loopValue &&
           keepGoing == other.keepGoing;
  }
};

Playback playback(const AnimationStateInstance& state)
{
  auto animation = state.animationInstance();
  return {
      animation->time(),
      animation->totalTime(),
      animation->lastTotalTime(),
      animation->spilledTime(),
      animation->direction(),
      animation->didLoop(),
      animation->loopValue(),
      state.keepGoing()};
}
} // namespace

// Every animation state in the test assets' machines, played on through a
// few loops and restarted, matches a new instance of it, then and frame by
// frame after: one shots, loops and ping pongs, forwards and backwards.
TEST_CASE(
    "A restarted animation state starts over as a new one would",
    "[animation reset]")
{
  int restarted = 0;
  for (const std::string& path : listAssets())
  {
    auto file = importAsset(path);
    if (file == nullptr)
    {
      continue;
    }
    for (size_t a = 0; a < file->artboardCount(); a++)
    {
      auto artboard = file->artboardAt(a);
      if (artboard == nullptr || artboard->stateMachineCount() == 0)
      {
        continue;
      }
      auto machine = artboard->stateMachineAt(0);
      auto definition = artboard->stateMachine(0);
      for (size_t l = 0; l < definition->layerCount(); l++)
      {
        auto layer = definition->layer(l);
        for (size_t s = 0; s < layer->stateCount(); s++)
        {
          auto state = layer->state(s);
          if (!state->is<AnimationState>() ||
              state->as<AnimationState>()->animation() == nullptr)
          {
            continue;
          }
          auto fresh = state->makeInstance(artboard.get());
          auto used = state->makeInstance(artboard.get());
          auto* freshState = static_cast<AnimationStateInstance*>(fresh.get());
          auto* usedState = static_cast<AnimationStateInstance*>(used.get());
          for (int i = 0; i < 9; i++)
          {
            usedState->advance(0.37f, machine.get());
          }
          if (!usedState->restart())
          {
            continue;
          }
          INFO(path << " " << state->as<AnimationState>()->animation()->name());
          REQUIRE(playback(*usedState) == playback(*freshState));
          for (int i = 0; i < 12; i++)
          {
            freshState->advance(0.21f, machine.get());
            usedState->advance(0.21f, machine.get());
            REQUIRE(playback(*usedState) == playback(*freshState));
          }
          restarted++;
        }
      }
    }
  }
  CHECK(restarted > 100);
}

namespace
{
// Every double and color property the artboard's animations key, read off
// the artboard instance. A double holds a color's 32 bits exactly.
std::vector<double> keyedValues(ArtboardInstance* artboard)
{
  std::vector<double> values;
  for (size_t i = 0; i < artboard->animationCount(); i++)
  {
    auto animation = artboard->animation(i);
    for (size_t o = 0; o < animation->numKeyedObjects(); o++)
    {
      auto keyedObject = animation->getObject(o);
      auto object = artboard->resolve(keyedObject->objectId());
      if (object == nullptr)
      {
        continue;
      }
      for (size_t k = 0; k < keyedObject->numKeyedProperties(); k++)
      {
        auto key = keyedObject->getProperty(k)->propertyKey();
        switch (CoreRegistry::propertyFieldId(key))
        {
          case CoreDoubleType::id:
            values.push_back(CoreRegistry::getDouble(object, key));
            break;
          case CoreColorType::id:
            values.push_back(
                (double)(uint32_t)CoreRegistry::getColor(object, key));
            break;
        }
      }
    }
  }
  return values;
}

using Frames = std::vector<std::vector<double>>;

// state_return_reuse.riv's Main, driven frame by frame. A layer takes no
// other transition until a mix ends, unless the mix can be cut short
// (enableEarlyExit, on Left to Right and A to B only), so some flips wait or
// cancel out:
// - Side goes to Right at 10, back to Left mid mix at 15, to Right at 33
//   (where the mix to Left ends, 18 having set right again), to Left at 60,
//   stays (61 and 62 cancel out by 78), then Right at 120 and Left at 150.
// - Cycle goes A, B (20), C (45), A (70, not a return: it left B), B (95),
//   back to A mid mix at 100, B at 130 and A mid mix at 131.
// - Bound goes Q at 32 and back to P at 34 while Mark eases to a target set
//   at 30; back to Q at 50, level (which Q binds once) set at 40 in
//   between; P at 54; Q at 103 and P at 104 while Mark eases to a target
//   set at 100.
// - Input goes V at 160 and back to U at 164, the view model unbound at 162
//   in between.
Frames playReturns(bool reuse)
{
  StateMachineInstance::sm_reuseStateInstances = reuse;
  auto file = ReadRiveFile("assets/state_return_reuse.riv");
  auto artboard = file->artboardNamed("Main");
  auto viewModelInstance = file->createViewModelInstance(
      artboard->viewModelId(),
      0);
  auto machine = artboard->stateMachineAt(0);
  machine->bindViewModelInstance(viewModelInstance);
  auto right = viewModelInstance->propertyValue("right")
                   ->as<ViewModelInstanceBoolean>();
  auto step = viewModelInstance->propertyValue("step")
                  ->as<ViewModelInstanceNumber>();
  auto target = viewModelInstance->propertyValue("target")
                    ->as<ViewModelInstanceNumber>();
  auto flip = viewModelInstance->propertyValue("flip")
                  ->as<ViewModelInstanceBoolean>();
  auto level = viewModelInstance->propertyValue("level")
                   ->as<ViewModelInstanceNumber>();
  auto go = machine->getBool("go");
  const std::vector<std::pair<int, bool>> rights = {
      {10, true},
      {15, false},
      {18, true},
      {60, false},
      {61, true},
      {62, false},
      {120, true},
      {150, false}};
  const std::vector<std::pair<int, float>> steps =
      {{20, 1}, {45, 2}, {70, 0}, {95, 1}, {100, 0}, {130, 1}, {131, 0}};
  const std::vector<std::pair<int, float>> targets = {{30, 200}, {100, 60}};
  const std::vector<std::pair<int, bool>> flips = {
      {32, true},
      {34, false},
      {50, true},
      {54, false},
      {103, true},
      {104, false}};
  Frames frames;
  for (int frame = 0; frame < 200; frame++)
  {
    for (auto& change : rights)
    {
      if (change.first == frame)
      {
        right->propertyValue(change.second);
      }
    }
    for (auto& change : steps)
    {
      if (change.first == frame)
      {
        step->propertyValue(change.second);
      }
    }
    for (auto& change : targets)
    {
      if (change.first == frame)
      {
        target->propertyValue(change.second);
      }
    }
    for (auto& change : flips)
    {
      if (change.first == frame)
      {
        flip->propertyValue(change.second);
      }
    }
    if (frame == 40)
    {
      level->propertyValue(220);
    }
    if (frame == 160 || frame == 164)
    {
      go->value(frame == 160);
    }
    if (frame == 162)
    {
      machine->bindViewModelInstance(nullptr);
    }
    machine->advanceAndApply(1.0f / 60.0f);
    frames.push_back(keyedValues(artboard.get()));
  }
  StateMachineInstance::sm_reuseStateInstances = true;
  return frames;
}

// state_return_reuse.riv's Main with Side's Left state given the A
// animation at frame 12, the way the editor reassigns one: Side leaves Left
// at 10 and goes back to it mid mix at 15.
Frames playReassigned(bool reuse)
{
  StateMachineInstance::sm_reuseStateInstances = reuse;
  auto file = ReadRiveFile("assets/state_return_reuse.riv");
  auto artboard = file->artboardNamed("Main");
  auto viewModelInstance = file->createViewModelInstance(
      artboard->viewModelId(),
      0);
  auto machine = artboard->stateMachineAt(0);
  machine->bindViewModelInstance(viewModelInstance);
  auto right = viewModelInstance->propertyValue("right")
                   ->as<ViewModelInstanceBoolean>();
  AnimationState* left = nullptr;
  auto side = artboard->stateMachine(0)->layer(0);
  for (size_t i = 0; i < side->stateCount(); i++)
  {
    auto state = side->state(i);
    if (state->is<AnimationState>() &&
        state->as<AnimationState>()->animation()->name() == "Left")
    {
      left = state->as<AnimationState>();
    }
  }
  REQUIRE(left != nullptr);
  Frames frames;
  for (int frame = 0; frame < 60; frame++)
  {
    if (frame == 10 || frame == 15)
    {
      right->propertyValue(frame == 10);
    }
    if (frame == 12)
    {
      left->animation(artboard->animation("A"));
    }
    machine->advanceAndApply(1.0f / 60.0f);
    frames.push_back(keyedValues(artboard.get()));
  }
  StateMachineInstance::sm_reuseStateInstances = true;
  return frames;
}

// A test asset artboard's first machine, played for 90 frames.
Frames playAsset(const std::string& path, size_t artboardIndex, bool reuse)
{
  StateMachineInstance::sm_reuseStateInstances = reuse;
  Frames frames;
  auto file = importAsset(path);
  auto artboard = file->artboardAt(artboardIndex);
  auto machine = artboard->stateMachineAt(0);
  for (int frame = 0; frame < 90; frame++)
  {
    machine->advanceAndApply(1.0f / 60.0f);
    frames.push_back(keyedValues(artboard.get()));
  }
  StateMachineInstance::sm_reuseStateInstances = true;
  return frames;
}
} // namespace

// Going back to a state reuses its instance, mid mix or not, and every
// frame comes out as it does with a new instance each time, bound keyframes
// included: a reused P starts Mark's ease over, a reused Q takes level as it
// is, and a reused U, the view model gone, has no bound value for Pin.
TEST_CASE(
    "A layer going back to a state reuses its instance",
    "[animation reset]")
{
  auto before = StateMachineInstance::sm_reusedStateInstances;
  auto reused = playReturns(true);
  auto reuses = StateMachineInstance::sm_reusedStateInstances - before;
  auto made = playReturns(false);
  CHECK(StateMachineInstance::sm_reusedStateInstances - before == reuses);
  // Side returns at 15, 33, 60, 120 and 150; Cycle at 100, 130 and 131;
  // Bound at 34, 50, 54, 103 and 104; Input at 164.
  CHECK(reuses == 14);
  REQUIRE(reused.size() == made.size());
  for (size_t frame = 0; frame < reused.size(); frame++)
  {
    INFO("frame " << frame);
    REQUIRE(reused[frame] == made[frame]);
  }
}

// A state given another animation since the layer left it plays the new
// one when the layer goes back, as a new instance of it would: the old
// instance isn't reused.
TEST_CASE(
    "A layer going back to a reassigned state plays its new animation",
    "[animation reset]")
{
  auto before = StateMachineInstance::sm_reusedStateInstances;
  auto reused = playReassigned(true);
  CHECK(StateMachineInstance::sm_reusedStateInstances == before);
  auto made = playReassigned(false);
  REQUIRE(reused.size() == made.size());
  for (size_t frame = 0; frame < reused.size(); frame++)
  {
    INFO("frame " << frame);
    REQUIRE(reused[frame] == made[frame]);
  }
}

// Every test asset artboard's first machine plays the same with and without
// reusing the states its layers go back to.
TEST_CASE(
    "Test asset machines play the same reusing states",
    "[animation reset]")
{
  bool deterministic = File::deterministicMode;
  File::deterministicMode = true;
  auto before = StateMachineInstance::sm_reusedStateInstances;
  int machines = 0;
  for (const std::string& path : listAssets())
  {
    auto file = importAsset(path);
    if (file == nullptr)
    {
      continue;
    }
    for (size_t a = 0; a < file->artboardCount(); a++)
    {
      auto artboard = file->artboardAt(a);
      if (artboard == nullptr || artboard->stateMachineCount() == 0)
      {
        continue;
      }
      INFO(path << " artboard " << a);
      REQUIRE(playAsset(path, a, true) == playAsset(path, a, false));
      machines++;
    }
  }
  File::deterministicMode = deterministic;
  CHECK(machines > 100);
  // Some have to go back to a state, or matching proves nothing.
  CHECK(StateMachineInstance::sm_reusedStateInstances > before);
}
