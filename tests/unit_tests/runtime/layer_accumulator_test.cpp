#include "rive/animation/animation_state.hpp"
#include "rive/animation/blend_accumulator.hpp"
#include "rive/animation/blend_animation.hpp"
#include "rive/animation/blend_state.hpp"
#include "rive/animation/keyed_object.hpp"
#include "rive/animation/keyed_property.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/animation/state_machine.hpp"
#include "rive/animation/state_machine_bool.hpp"
#include "rive/animation/state_machine_input_instance.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/animation/state_machine_layer.hpp"
#include "rive/animation/state_machine_number.hpp"
#include "rive/animation/state_machine_trigger.hpp"
#include "rive/artboard.hpp"
#include "rive/event.hpp"
#include "rive/event_report.hpp"
#include "rive/file.hpp"
#include "rive/generated/core_registry.hpp"
#include "rive/layout_component.hpp"
#include "rive/node.hpp"
#include "rive/shapes/paint/solid_color.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/transform_component.hpp"
#include "rive/world_transform_component.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/viewmodel_instance_boolean.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "rive/viewmodel/viewmodel_instance_trigger.hpp"
#include "rive_file_reader.hpp"
#include <algorithm>
#include <catch.hpp>
#include <cstring>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

// A state machine whose layers key the same property sets it once a frame,
// to the value the layers' writes would have left (see
// StateMachine::layerConflicts). These pin that it is exact: the same values,
// events and frames as writing each layer's values to the objects.

using namespace rive;

namespace
{
// Turns the layer accumulator on or off for the instances made in scope.
struct LayerAccumulatorScope
{
  bool previous = StateMachineInstance::sm_layerAccumulatorEnabled;
  explicit LayerAccumulatorScope(bool enabled)
  {
    StateMachineInstance::sm_layerAccumulatorEnabled = enabled;
  }
  ~LayerAccumulatorScope()
  {
    StateMachineInstance::sm_layerAccumulatorEnabled = previous;
  }
};

void addKeyed(
    const LinearAnimation* animation,
    std::set<std::pair<uint32_t, int>>& keyed)
{
  if (animation == nullptr)
  {
    return;
  }
  for (size_t o = 0; o < animation->numKeyedObjects(); o++)
  {
    const KeyedObject* object = animation->getObject(o);
    for (size_t p = 0; p < object->numKeyedProperties(); p++)
    {
      int key = object->getProperty(p)->propertyKey();
      if (!CoreRegistry::isCallback(key))
      {
        keyed.insert({object->objectId(), key});
      }
    }
  }
}

// Every (object, property) any state of the machine keys.
std::vector<std::pair<uint32_t, int>> keyedBy(const StateMachine* machine)
{
  std::set<std::pair<uint32_t, int>> keyed;
  for (size_t l = 0; l < machine->layerCount(); l++)
  {
    const StateMachineLayer* layer = machine->layer(l);
    for (size_t s = 0; s < layer->stateCount(); s++)
    {
      const LayerState* state = layer->state(s);
      if (state->is<AnimationState>())
      {
        addKeyed(state->as<AnimationState>()->animation(), keyed);
      }
      else if (state->is<BlendState>())
      {
        for (auto* blend : state->as<BlendState>()->animations())
        {
          addKeyed(blend->animation(), keyed);
        }
      }
    }
  }
  return {keyed.begin(), keyed.end()};
}

void appendBits(std::vector<uint32_t>& out, float value)
{
  uint32_t bits;
  memcpy(&bits, &value, sizeof(bits));
  out.push_back(bits);
}

// What a frame left: every keyed value, every component's collapse, world
// transform and render opacity, and the events reported.
std::vector<uint32_t> snapshot(
    ArtboardInstance* artboard,
    StateMachineInstance* machine,
    const std::vector<std::pair<uint32_t, int>>& keyed)
{
  std::vector<uint32_t> out;
  for (const auto& entry : keyed)
  {
    Core* object = artboard->resolve(entry.first);
    if (object == nullptr)
    {
      continue;
    }
    switch (CoreRegistry::propertyFieldId(entry.second))
    {
      case CoreDoubleType::id:
        appendBits(out, CoreRegistry::getDouble(object, entry.second));
        break;
      case CoreColorType::id:
        out.push_back((uint32_t)CoreRegistry::getColor(object, entry.second));
        break;
      // Ids share the uint field; read both ways.
      case CoreUintType::id:
        out.push_back(CoreRegistry::getUint(object, entry.second));
        out.push_back(CoreRegistry::getId(object, entry.second));
        break;
      case CoreBoolType::id:
        out.push_back(CoreRegistry::getBool(object, entry.second));
        break;
      case CoreStringType::id:
        out.push_back((uint32_t)std::hash<std::string>()(
            CoreRegistry::getString(object, entry.second)));
        break;
    }
  }
  for (Core* object : artboard->objects())
  {
    if (object != nullptr && object->is<Component>())
    {
      out.push_back(object->as<Component>()->isCollapsed());
    }
    if (object == nullptr || !object->is<WorldTransformComponent>())
    {
      continue;
    }
    auto component = object->as<WorldTransformComponent>();
    const Mat2D& world = component->worldTransform();
    for (int i = 0; i < 6; i++)
    {
      appendBits(out, world[i]);
    }
    appendBits(out, component->childOpacity());
  }
  out.push_back((uint32_t)machine->reportedEventCount());
  for (size_t i = 0; i < machine->reportedEventCount(); i++)
  {
    out.push_back((uint32_t)std::hash<std::string>()(
        machine->reportedEventAt(i).event()->name()));
  }
  return out;
}

// Pokes every input and view model value the same way at the same frames.
void perturb(
    StateMachineInstance* machine,
    ViewModelInstance* viewModelInstance,
    int frame)
{
  if (frame != 10 && frame != 25 && frame != 40 && frame != 55)
  {
    return;
  }
  for (size_t i = 0; i < machine->inputCount(); i++)
  {
    SMIInput* input = machine->input(i);
    if (input == nullptr)
    {
      continue;
    }
    if (input->input()->is<StateMachineBool>())
    {
      auto boolean = static_cast<SMIBool*>(input);
      boolean->value(!boolean->value());
    }
    else if (input->input()->is<StateMachineNumber>())
    {
      auto number = static_cast<SMINumber*>(input);
      number->value(number->value() + (float)(frame % 7) * 0.5f);
    }
    else if (input->input()->is<StateMachineTrigger>())
    {
      static_cast<SMITrigger*>(input)->fire();
    }
  }
  if (viewModelInstance == nullptr)
  {
    return;
  }
  for (auto& value : viewModelInstance->propertyValues())
  {
    if (value->is<ViewModelInstanceBoolean>())
    {
      auto boolean = value->as<ViewModelInstanceBoolean>();
      boolean->propertyValue(!boolean->propertyValue());
    }
    else if (value->is<ViewModelInstanceNumber>())
    {
      auto number = value->as<ViewModelInstanceNumber>();
      number->propertyValue(number->propertyValue() + (float)(frame % 7));
    }
    else if (value->is<ViewModelInstanceTrigger>())
    {
      value->as<ViewModelInstanceTrigger>()->trigger();
    }
  }
}

struct Run
{
  bool accumulated = false;
  std::vector<std::vector<uint32_t>> frames;
  // What each frame's advance returned.
  std::vector<bool> keepGoing;
};

// How a host drives the machine: advanceAndApply, whose settle loop searches
// again after the layers, or advance and then the artboard, which doesn't.
enum class Host
{
  advanceAndApply,
  advanceThenArtboard,
};

// Plays [machineIndex] of [artboardIndex] for 70 frames, poking it as above.
Run play(
    const rcp<File>& file,
    size_t artboardIndex,
    size_t machineIndex,
    bool accumulate,
    Host host)
{
  LayerAccumulatorScope scope(accumulate);
  Run run;
  auto artboard = file->artboardAt(artboardIndex);
  auto machine = artboard->stateMachineAt(machineIndex);
  run.accumulated = machine->hasLayerAccumulator();
  auto viewModelInstance = file->createDefaultViewModelInstance(artboard.get());
  if (viewModelInstance != nullptr)
  {
    machine->bindViewModelInstance(viewModelInstance);
  }
  auto keyed = keyedBy(artboard->stateMachine(machineIndex));
  for (int frame = 0; frame < 70; frame++)
  {
    perturb(machine.get(), viewModelInstance.get(), frame);
    float seconds = frame == 0 ? 0.0f : 1.0f / 60.0f;
    bool keepGoing;
    if (host == Host::advanceAndApply)
    {
      keepGoing = machine->advanceAndApply(seconds);
    }
    else
    {
      keepGoing = machine->advance(seconds);
      artboard->advance(seconds);
    }
    run.frames.push_back(snapshot(artboard.get(), machine.get(), keyed));
    run.keepGoing.push_back(keepGoing);
  }
  return run;
}

std::vector<std::string> listAssets(const std::string& dir)
{
  std::vector<std::string> out;
  for (const auto& entry : std::filesystem::recursive_directory_iterator(dir))
  {
    if (entry.is_regular_file() && entry.path().extension() == ".riv")
    {
      out.push_back(entry.path().generic_string());
    }
  }
  std::sort(out.begin(), out.end());
  return out;
}
} // namespace

TEST_CASE(
    "A machine's shared layer writes land as written one by one",
    "[state machine]")
{
  bool deterministic = File::deterministicMode;
  File::deterministicMode = true;
  std::vector<std::string> assets = listAssets("assets");
  int machines = 0;
  int frames = 0;
  for (const std::string& path : assets)
  {
    // Some assets are malformed on purpose.
    std::vector<uint8_t> bytes = ReadFile(path.c_str());
    ImportResult result;
    rcp<File> file = File::import(bytes, &gNoOpFactory, &result);
    if (result != ImportResult::success || file == nullptr)
    {
      continue;
    }
    for (size_t a = 0; a < file->artboardCount(); a++)
    {
      Artboard* definition = file->artboard(a);
      for (size_t m = 0; m < definition->stateMachineCount(); m++)
      {
        if (definition->stateMachine(m)->layerConflicts().empty())
        {
          continue;
        }
        for (Host host : {Host::advanceAndApply, Host::advanceThenArtboard})
        {
          Run direct = play(file, a, m, false, host);
          Run accumulated = play(file, a, m, true, host);
          CHECK_FALSE(direct.accumulated);
          if (!accumulated.accumulated)
          {
            // An artboard that uses scripts writes straight through.
            continue;
          }
          machines++;
          INFO(
              path << " artboard " << a << " machine " << m << " host "
                   << (int)host);
          REQUIRE(direct.frames.size() == accumulated.frames.size());
          for (size_t f = 0; f < direct.frames.size(); f++)
          {
            INFO("frame " << f);
            CHECK(direct.frames[f] == accumulated.frames[f]);
            // Writing only final values can let a machine settle where the
            // intermediate ones kept dirtying the artboard, never the other
            // way round.
            CHECK((direct.keepGoing[f] || !accumulated.keepGoing[f]));
            frames++;
          }
        }
      }
    }
  }
  File::deterministicMode = deterministic;
  // The assets do have machines like that, played both ways.
  CHECK(machines >= 40);
  CHECK(frames == machines * 70);
}

// layer_writes.riv's Pill: a Base layer rests a layout's height at 0, a
// Selected layer holds it at 40, and both apply every frame. Written to the
// layout one after the other, the height went 40, 0, 40 each frame, and every
// change marked the layout dirty, so an idle frame ran a full layout solve.
TEST_CASE(
    "Two layers holding a layout's height leave an idle frame clean",
    "[state machine]")
{
  auto file = ReadRiveFile("assets/layer_writes.riv");
  for (bool accumulate : {false, true})
  {
    LayerAccumulatorScope scope(accumulate);
    auto artboard = file->artboardNamed("Pill");
    REQUIRE(artboard != nullptr);
    auto machine = artboard->defaultStateMachine();
    REQUIRE(machine != nullptr);
    CHECK(machine->hasLayerAccumulator() == accumulate);
    for (int i = 0; i < 10; i++)
    {
      machine->advanceAndApply(i == 0 ? 0.0f : 1.0f / 60.0f);
    }
    // The artboard is a layout named Pill too.
    LayoutComponent* pill = nullptr;
    for (auto* layout : artboard->find<LayoutComponent>())
    {
      if (!layout->is<Artboard>())
      {
        pill = layout;
      }
    }
    REQUIRE(pill != nullptr);
    CHECK(pill->height() == 40.0f);

    const uint64_t solves = Artboard::layoutPassCount();
    const uint64_t dirt = Artboard::sm_dirtNotifications;
    for (int i = 0; i < 30; i++)
    {
      machine->advanceAndApply(1.0f / 60.0f);
    }
    CHECK(pill->height() == 40.0f);
    if (accumulate)
    {
      CHECK(Artboard::layoutPassCount() == solves);
      CHECK(Artboard::sm_dirtNotifications == dirt);
      // So it can settle: a host that stops ticking a settled machine may.
      CHECK_FALSE(machine->advanceAndApply(1.0f / 60.0f));
    }
    else
    {
      CHECK(Artboard::layoutPassCount() >= solves + 30);
      CHECK(Artboard::sm_dirtNotifications > dirt);
      CHECK(machine->advanceAndApply(1.0f / 60.0f));
    }
  }
}

// layer_writes.riv's Discrete: a Base layer shows a Solo's first child and
// keeps a layout displayed, an Over layer shows the second and hides it, and
// both apply every frame. Written one after the other, the Solo collapsed and
// uncollapsed its children and the layout hid and showed every frame.
TEST_CASE(
    "Two layers keying discrete values leave an idle frame clean",
    "[state machine]")
{
  auto file = ReadRiveFile("assets/layer_writes.riv");
  for (bool accumulate : {false, true})
  {
    LayerAccumulatorScope scope(accumulate);
    auto artboard = file->artboardNamed("Discrete");
    REQUIRE(artboard != nullptr);
    auto machine = artboard->defaultStateMachine();
    REQUIRE(machine != nullptr);
    CHECK(machine->hasLayerAccumulator() == accumulate);
    for (int i = 0; i < 10; i++)
    {
      machine->advanceAndApply(i == 0 ? 0.0f : 1.0f / 60.0f);
    }
    auto first = artboard->find<Shape>("First");
    auto second = artboard->find<Shape>("Second");
    auto panel = artboard->find<LayoutComponent>("Panel");
    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);
    REQUIRE(panel != nullptr);
    // The Over layer's values, applied last.
    CHECK(first->isCollapsed());
    CHECK_FALSE(second->isCollapsed());
    CHECK(panel->isHidden());

    const uint64_t dirt = Artboard::sm_dirtNotifications;
    for (int i = 0; i < 30; i++)
    {
      machine->advanceAndApply(1.0f / 60.0f);
    }
    CHECK(first->isCollapsed());
    CHECK_FALSE(second->isCollapsed());
    CHECK(panel->isHidden());
    if (accumulate)
    {
      CHECK(Artboard::sm_dirtNotifications == dirt);
      CHECK_FALSE(machine->advanceAndApply(1.0f / 60.0f));
    }
    else
    {
      CHECK(Artboard::sm_dirtNotifications > dirt);
    }
  }
}

// layer_writes.riv's Alias, ScaleTypes and Direction: two layers key a
// property that a third layer's writes are tied to. Alias's third layer keys
// the same x through its xArtboard key, so x is collected under one key and
// the last write wins whichever key it came through. ScaleTypes' scale type
// change sets the intrinsicallySizedValue a third layer keys, and
// Direction's direction change clears the layout animation a third layer's
// width mix reads, so neither is collected.
TEST_CASE(
    "A property tied to another keyed property keeps its write order",
    "[state machine]")
{
  auto file = ReadRiveFile("assets/layer_writes.riv");
  auto conflicts =
      [&](const char* name) -> std::vector<StateMachine::LayerConflict> {
    auto artboard = file->artboard(name);
    REQUIRE(artboard != nullptr);
    REQUIRE(artboard->stateMachineCount() == 1);
    return artboard->stateMachine(0)->layerConflicts();
  };
  auto alias = conflicts("Alias");
  REQUIRE(alias.size() == 1);
  // A copy: CHECK takes its operands by reference.
  const uint16_t xKey = NodeBase::xPropertyKey;
  CHECK(alias[0].propertyKey == xKey);
  CHECK(conflicts("ScaleTypes").empty());
  CHECK(conflicts("Direction").empty());

  for (bool accumulate : {false, true})
  {
    LayerAccumulatorScope scope(accumulate);
    auto artboard = file->artboardNamed("Alias");
    REQUIRE(artboard != nullptr);
    auto machine = artboard->defaultStateMachine();
    REQUIRE(machine != nullptr);
    CHECK(machine->hasLayerAccumulator() == accumulate);
    for (int i = 0; i < 10; i++)
    {
      machine->advanceAndApply(i == 0 ? 0.0f : 1.0f / 60.0f);
    }
    auto box = artboard->find<Shape>("Box");
    REQUIRE(box != nullptr);
    // The Alias layer's x, written last through key 9.
    CHECK(box->x() == 30.0f);
  }
}

// layer_writes.riv's BoundFlag: a Base layer keys the panel's width scale
// type to fill and a Fixed layer keys it back to fixed, so each frame's
// change sets intrinsicallySizedValue from the final types, over what a view
// model bool bound to it set. Set once to its final type, which doesn't
// change, the scale type would leave the bound value standing.
TEST_CASE(
    "Two layers keying a scale type still reset the flag a bind sets",
    "[state machine]")
{
  auto file = ReadRiveFile("assets/layer_writes.riv");
  REQUIRE(file->artboard("BoundFlag") != nullptr);
  CHECK(file->artboard("BoundFlag")->stateMachine(0)->layerConflicts().empty());
  for (bool accumulate : {false, true})
  {
    LayerAccumulatorScope scope(accumulate);
    auto artboard = file->artboardNamed("BoundFlag");
    REQUIRE(artboard != nullptr);
    auto machine = artboard->defaultStateMachine();
    REQUIRE(machine != nullptr);
    auto viewModelInstance = file->createDefaultViewModelInstance(
        artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    machine->bindViewModelInstance(viewModelInstance);
    for (int i = 0; i < 10; i++)
    {
      machine->advanceAndApply(i == 0 ? 0.0f : 1.0f / 60.0f);
    }
    auto panel = artboard->find<LayoutComponent>("Panel");
    REQUIRE(panel != nullptr);
    REQUIRE(panel->style() != nullptr);
    CHECK_FALSE(panel->style()->intrinsicallySized());

    auto hugs = viewModelInstance->propertyValue("hugs");
    REQUIRE(hugs != nullptr);
    hugs->as<ViewModelInstanceBoolean>()->propertyValue(true);
    machine->advanceAndApply(1.0f / 60.0f);
    // The bind applies after the layers.
    CHECK(panel->style()->intrinsicallySized());
    machine->advanceAndApply(1.0f / 60.0f);
    // The next frame's scale type change resets it.
    CHECK_FALSE(panel->style()->intrinsicallySized());
  }
}
