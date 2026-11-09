#include <rive/file.hpp>
#include <rive/animation/state_machine.hpp>
#include <rive/animation/state_machine_instance.hpp>
#include <rive/artboard_component_list.hpp>
#include <rive/data_bind/bindable_property.hpp>
#include <rive/data_bind/data_bind.hpp>
#include <rive/generated/core_registry.hpp>
#include <rive/viewmodel/viewmodel_instance.hpp>
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

// Conditions that read one value share one bindable per instance (see
// StateMachine::bindableTwins), a layer whose held pose a bind change
// unsettled searches before it re-applies the pose, and a list row whose
// machine leaves it quiet goes quiet before its artboard advances. Editor
// builds don't share (their conditions change after import).

#ifndef WITH_RIVE_EDITOR
using namespace rive;

namespace
{
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

// Every double and color property the artboard's animations key, read off
// the artboard instance. As doubles, which hold every 32-bit color exactly.
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
            values.push_back((double)CoreRegistry::getColor(object, key));
            break;
        }
      }
    }
  }
  return values;
}

// The three switches, on or off for a scope, then back on.
struct Switches
{
  explicit Switches(bool on)
  {
    StateMachineInstance::sm_shareBindables = on;
    StateMachineInstance::sm_searchBeforeReapply = on;
    ArtboardComponentList::sm_requietAfterMachine = on;
  }
  ~Switches()
  {
    StateMachineInstance::sm_shareBindables = true;
    StateMachineInstance::sm_searchBeforeReapply = true;
    ArtboardComponentList::sm_requietAfterMachine = true;
  }
};

using Frames = std::vector<std::vector<double>>;

// A test asset artboard's first machine, bound to the artboard's default
// view model instance when it has one, played for 90 frames.
Frames playAsset(const std::string& path, size_t artboardIndex, bool on)
{
  Switches switches(on);
  Frames frames;
  auto file = importAsset(path);
  auto artboard = file->artboardAt(artboardIndex);
  auto machine = artboard->stateMachineAt(0);
  // Bound, this one trips verifyFrozenPose on master too: a frozen layer
  // holds a pose keying ScrollConstraint.scrollIndex, which the scroll
  // constraint then changes without the artboard's change serial moving.
  // Reported, not fixed here.
  bool knownFrozenPoseBug = path == "assets/component_list_child_origin.riv";
  if (auto viewModelInstance = knownFrozenPoseBug
                                   ? nullptr
                                   : file->createDefaultViewModelInstance(
                                         artboard.get()))
  {
    machine->bindViewModelInstance(viewModelInstance);
  }
  for (int frame = 0; frame < 90; frame++)
  {
    machine->advanceAndApply(1.0f / 60.0f);
    frames.push_back(keyedValues(artboard.get()));
  }
  return frames;
}
} // namespace

TEST_CASE("Conditions reading one value share its bindable", "[data binding]")
{
  size_t shared = 0;
  for (const std::string& path : listAssets())
  {
    auto file = importAsset(path);
    if (file == nullptr)
    {
      continue;
    }
    for (size_t a = 0; a < file->artboardCount(); a++)
    {
      auto source = file->artboard(a);
      auto instance = file->artboardAt(a);
      if (source == nullptr || instance == nullptr)
      {
        continue;
      }
      for (size_t m = 0; m < source->stateMachineCount(); m++)
      {
        auto machine = source->stateMachine(m);
        auto machineInstance = instance->stateMachineAt(m);
        REQUIRE(machineInstance != nullptr);
        size_t unshared;
        {
          Switches off(false);
          unshared = instance->stateMachineAt(m)->dataBinds().size();
        }
        const auto& twins = machine->bindableTwins();
        size_t twinCount = 0;
        for (size_t i = 0; i < machine->dataBindCount(); i++)
        {
          auto dataBind = machine->dataBind(i);
          if (dataBind->target() == nullptr)
          {
            continue;
          }
          if (twins.empty() || twins[i] < 0)
          {
            continue;
          }
          twinCount++;
          INFO(path << " " << machine->name() << " bind " << i);
          auto bindable = dataBind->target()->as<BindableProperty>();
          auto twin = machine->dataBind(twins[i])
                          ->target()
                          ->as<BindableProperty>();
          // Two bindables in the file, one in the instance.
          CHECK(bindable != twin);
          CHECK(machineInstance->bindablePropertyInstance(bindable) != nullptr);
          CHECK(
              machineInstance->bindablePropertyInstance(bindable) ==
              machineInstance->bindablePropertyInstance(twin));
        }
        INFO(path << " " << machine->name());
        // A twin's bind isn't made; every other one still is.
        CHECK(machineInstance->dataBinds().size() == unshared - twinCount);
        shared += twinCount;
      }
    }
  }
  // Enough of the test assets read a value twice for this to say something.
  CHECK(shared > 50);
}

TEST_CASE(
    "Sharing a bindable leaves it to itself when switched off",
    "[data binding]")
{
  Switches off(false);
  auto file = ReadRiveFile("assets/data_binding_test.riv");
  size_t checked = 0;
  for (size_t a = 0; a < file->artboardCount(); a++)
  {
    auto source = file->artboard(a);
    auto instance = source->instance();
    for (size_t m = 0; m < source->stateMachineCount(); m++)
    {
      auto machine = source->stateMachine(m);
      auto machineInstance = instance->stateMachineAt(m);
      // Every bindable its own clone.
      for (size_t i = 0; i < machine->dataBindCount(); i++)
      {
        auto target = machine->dataBind(i)->target();
        if (target == nullptr || !target->is<BindableProperty>())
        {
          continue;
        }
        auto bindable = target->as<BindableProperty>();
        for (size_t j = 0; j < i; j++)
        {
          auto other = machine->dataBind(j)->target();
          if (other != nullptr && other != target &&
              other->is<BindableProperty>())
          {
            CHECK(
                machineInstance->bindablePropertyInstance(bindable) !=
                machineInstance->bindablePropertyInstance(
                    other->as<BindableProperty>()));
          }
        }
        checked++;
      }
    }
  }
  CHECK(checked > 0);
}

// Every test asset artboard's first machine plays the same with all three
// on as with all three off.
TEST_CASE(
    "Test asset machines play the same sharing condition bindables",
    "[data binding]")
{
  bool deterministic = File::deterministicMode;
  File::deterministicMode = true;
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
}
#endif
