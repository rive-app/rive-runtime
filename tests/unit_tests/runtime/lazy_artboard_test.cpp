/*
 * Copyright 2026 Rive
 */

// Deferred artboards (FileImportOptions::deferDecoding): import reads each
// artboard's record but skips the rest of its run, and decodes it the first
// time the artboard is used.
// These tests hold the lazy import to the eager one: the same import result,
// the same artboards once decoded, the same frames drawn, bodies left alone
// until something needs them, and one decode per body however many threads
// first use it.
//
// RIVE_LAZY_DECODING=1 turns deferral on for every other test in the binary
// too, so the whole suite, silvers included, can run lazily against its
// unchanged baselines.

#include "rive/animation/linear_animation_instance.hpp"
#include "rive/animation/state_machine.hpp"
#include "rive/assets/file_asset.hpp"
#include "rive/assets/script_asset.hpp"
#include "rive/core/vector_binary_writer.hpp"
#include "rive/generated/animation/animation_base.hpp"
#include "rive/generated/animation/keyed_object_base.hpp"
#include "rive/generated/animation/keyed_property_base.hpp"
#include "rive/generated/animation/keyframe_base.hpp"
#include "rive/generated/animation/keyframe_double_base.hpp"
#include "rive/generated/animation/linear_animation_base.hpp"
#include "rive/generated/artboard_base.hpp"
#include "rive/generated/backboard_base.hpp"
#include "rive/generated/component_base.hpp"
#include "rive/generated/layout_component_base.hpp"
#include "rive/generated/nested_artboard_base.hpp"
#include "rive/generated/shapes/image_base.hpp"
#include "rive/generated/shapes/mesh_base.hpp"
#include "rive/artboard.hpp"
#include "rive/deferred_artboard_body.hpp"
#include "rive/file.hpp"
#include "rive/nested_artboard.hpp"
#include "rive/scripted/scripted_interpolator.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include "utils/serializing_factory.hpp"
#include "utils/svg_factory.hpp"
#include "utils/svg_renderer.hpp"
#include "utils/serialized_replay.hpp"
#include "lazy_decode_test_helpers.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <thread>
#include <vector>

using namespace rive;
using namespace lazy_decode_test;

static bool g_lazyDecodingFromEnvironment = [] {
  const char* value = getenv("RIVE_LAZY_DECODING");
  if (value != nullptr && value[0] == '1')
  {
    File::testing_defaultImportOptions.deferDecoding = true;
  }
  return true;
}();

namespace
{
// Deferring artboards or not, the default one too; animations and view
// model instances as the testing default says (RIVE_LAZY_DECODING), unless
// [lazyAnimations] is given.
rcp<File> import(
    const std::vector<uint8_t>& bytes,
    bool lazy,
    ImportResult* result,
    Factory* factory = &gNoOpFactory,
    int lazyAnimations = -1)
{
  FileImportOptions options = deferring(
      lazyAnimations == -1 ? deferringByDefault() : lazyAnimations == 1,
      lazy,
      deferringByDefault(),
      true);
  return File::import(
      bytes,
      factory,
      result,
      (FileAssetLoader*)nullptr,
      nullptr,
      false,
      options);
}

size_t pendingCount(const File* file)
{
  size_t count = 0;
  for (size_t i = 0; i < file->artboardCount(); i++)
  {
    count += file->artboardHeader(i)->hasDeferredBody() ? 1 : 0;
  }
  return count;
}

uint32_t outcomeCount(const File* file)
{
  uint32_t count = 0;
  for (size_t i = 0; i < kArtboardBodySkimCount; i++)
  {
    count += file->deferredArtboardStats().outcomes[i];
  }
  return count;
}

uint32_t deferredCount(const File* file)
{
  return file->deferredArtboardStats()
      .outcomes[(size_t)ArtboardBodySkim::deferred];
}

// Requires the same artboard, decoding [l] if the import deferred it.
void requireSameArtboard(Artboard* e, Artboard* l)
{
  CHECK(l->name() == e->name());
  CHECK(l->width() == e->width());
  CHECK(l->height() == e->height());
  l->ensureDecoded();
  CHECK_FALSE(l->hasDeferredBody());
  CHECK_FALSE(l->failedToDecode());
  REQUIRE(l->objects().size() == e->objects().size());
  for (size_t i = 0; i < e->objects().size(); i++)
  {
    Core* eo = e->objects()[i];
    Core* lo = l->objects()[i];
    REQUIRE((lo == nullptr) == (eo == nullptr));
    if (eo != nullptr)
    {
      CHECK(lo->coreType() == eo->coreType());
    }
  }
  CHECK(l->dataBinds().size() == e->dataBinds().size());
  CHECK(l->nestedArtboards().size() == e->nestedArtboards().size());
  for (size_t i = 0; i < e->nestedArtboards().size(); i++)
  {
    // The same references, cut or not: both point at artboards of their own
    // file, by the same index.
    auto eSource = e->nestedArtboards()[i]->sourceArtboard();
    auto lSource = l->nestedArtboards()[i]->sourceArtboard();
    CHECK((lSource == nullptr) == (eSource == nullptr));
    if (eSource != nullptr && lSource != nullptr)
    {
      CHECK(lSource->name() == eSource->name());
    }
  }
  REQUIRE(l->animationCount() == e->animationCount());
  for (size_t i = 0; i < e->animationCount(); i++)
  {
    requireSameAnimation(e->animation(i), l->animation(i));
  }
  REQUIRE(l->stateMachineCount() == e->stateMachineCount());
  for (size_t i = 0; i < e->stateMachineCount(); i++)
  {
    StateMachine* es = e->stateMachine(i);
    StateMachine* ls = l->stateMachine(i);
    CHECK(ls->name() == es->name());
    CHECK(ls->layerCount() == es->layerCount());
    CHECK(ls->inputCount() == es->inputCount());
    CHECK(ls->listenerCount() == es->listenerCount());
    CHECK(ls->dataBindCount() == es->dataBindCount());
  }
  CHECK(l->defaultStateMachineIndex() == e->defaultStateMachineIndex());
}
} // namespace

TEST_CASE(
    "lazy artboard bodies decode to what an eager import reads",
    "[lazy_artboard]")
{
  for (const std::string& path : allAssets())
  {
    INFO(path);
    std::vector<uint8_t> bytes = ReadFile(path.c_str());
    ImportResult eagerResult;
    ImportResult lazyResult;
    auto eager = import(bytes, false, &eagerResult);
    int decodes = File::deferredArtboardDecodeCount;
    auto lazy = import(bytes, true, &lazyResult);
    // Bodies scripts reach as they register (an artboard input), once the
    // import is done.
    int decodedByImport = File::deferredArtboardDecodeCount - decodes;
    if (lazyResult != eagerResult)
    {
      // A body that would fail the import whole only fails once decoded,
      // and leaves its artboard (or what stands in for it) empty.
      REQUIRE(eagerResult != ImportResult::success);
      REQUIRE(lazyResult == ImportResult::success);
      for (size_t i = 0; i < lazy->artboardCount(); i++)
      {
        CHECK(lazy->artboardAt(i) != nullptr);
      }
      CHECK(lazy->deferredArtboardStats().decodeFailures > 0);
      continue;
    }
    if (eager == nullptr)
    {
      continue;
    }
    REQUIRE(lazy != nullptr);
    REQUIRE(lazy->artboardCount() == eager->artboardCount());

    // Every artboard was either deferred or decoded for a reason, and the
    // deferred ones are all still pending.
    CHECK(outcomeCount(lazy.get()) == lazy->artboardCount());
    size_t pending = pendingCount(lazy.get());
    CHECK(pending + decodedByImport == deferredCount(lazy.get()));
    // Names come from the records.
    for (size_t i = 0; i < lazy->artboardCount(); i++)
    {
      CHECK(lazy->artboardNameAt(i) == eager->artboardNameAt(i));
    }
    CHECK(pendingCount(lazy.get()) == pending);

    // View models list only file level instances, so a deferred artboard
    // holding some (a stateful component's) changes no list.
    REQUIRE(lazy->viewModelCount() == eager->viewModelCount());
    for (size_t i = 0; i < eager->viewModelCount(); i++)
    {
      CHECK(
          lazy->viewModel(i)->instanceCount() ==
          eager->viewModel(i)->instanceCount());
    }

    for (size_t i = 0; i < eager->artboardCount(); i++)
    {
      INFO("artboard " << i);
      requireSameArtboard(eager->artboard(i), lazy->artboard(i));
    }
    CHECK(pendingCount(lazy.get()) == 0);
    CHECK(lazy->deferredArtboardStats().decodeFailures == 0);
    // And still does once they've all decoded.
    for (size_t i = 0; i < eager->viewModelCount(); i++)
    {
      CHECK(
          lazy->viewModel(i)->instanceCount() ==
          eager->viewModel(i)->instanceCount());
    }
  }
}

static const char* const kDrawnPaths[] = {
    "assets/car_widgets_v01.riv",
    "assets/hunter_x_demo.riv",
    "assets/zombie_skins.riv",
    "assets/echo_show_demo.riv",
    "assets/superbowl.riv",
    "assets/db_health_tracker.riv",
    "assets/data_viz_demo.riv",
    "assets/data_bind_keyframes_test.riv",
    "assets/joystick_flag_test.riv",
    "assets/joystick_nested_remap.riv",
    "assets/animated_clipping.riv",
    "assets/clipping_and_draw_order.riv",
    "assets/trim_path.riv",
    "assets/component_list_virtualized.riv",
    "assets/layout/layout_animation_nested.riv",
    "assets/layoutstest_8-planets-grid.riv",
    "assets/bidirectional_binding_source.riv",
};

TEST_CASE("lazy artboard bodies draw the same frames", "[lazy_artboard]")
{
  for (const char* path : kDrawnPaths)
  {
    INFO(path);
    std::vector<uint8_t> bytes = ReadFile(path);
    for (bool lazyAnimations : {false, true})
    {
      INFO("lazy animations " << lazyAnimations);
      SVGFactory eagerFactory;
      SVGFactory lazyFactory;
      ImportResult result;
      auto eager = import(bytes, false, &result, &eagerFactory, 0);
      REQUIRE(eager != nullptr);
      auto lazy = import(
          bytes,
          true,
          &result,
          &lazyFactory,
          lazyAnimations ? 1 : 0);
      REQUIRE(lazy != nullptr);
      auto e = svgFrames(eager.get(), 60);
      auto l = svgFrames(lazy.get(), 60);
      REQUIRE(l.size() == e.size());
      for (size_t i = 0; i < e.size(); i++)
      {
        INFO("frame " << i);
        CHECK(l[i] == e[i]);
      }
    }
  }
}

TEST_CASE("lazy artboard bodies wait for their first use", "[lazy_artboard]")
{
  std::vector<uint8_t> bytes = ReadFile("assets/superbowl.riv");
  ImportResult result;
  auto file = import(bytes, true, &result);
  REQUIRE(file != nullptr);
  size_t deferred = deferredCount(file.get());
  REQUIRE(deferred > 2);
  REQUIRE(pendingCount(file.get()) == deferred);

  // Names, counts and records don't decode anything.
  int decodes = File::deferredArtboardDecodeCount;
  for (size_t i = 0; i < file->artboardCount(); i++)
  {
    CHECK(!file->artboardNameAt(i).empty());
    CHECK(file->artboardHeader(i)->width() > 0);
  }
  CHECK(File::deferredArtboardDecodeCount == decodes);

  // The first frame decodes what it shows (the default artboard and what it
  // nests), and leaves the rest.
  auto artboard = file->artboardDefault();
  REQUIRE(artboard != nullptr);
  auto stateMachine = artboard->defaultStateMachine();
  if (stateMachine == nullptr)
  {
    stateMachine = artboard->stateMachineAt(0);
  }
  if (stateMachine != nullptr)
  {
    stateMachine->advanceAndApply(0.0f);
    stateMachine->advanceAndApply(1.0f / 60.0f);
  }
  size_t stillPending = pendingCount(file.get());
  CHECK(stillPending < deferred);
  CHECK(stillPending > 0);
  CHECK(
      File::deferredArtboardDecodeCount - decodes ==
      (int)(deferred - stillPending));

  // Instancing a pending artboard decodes it alone; again, nothing.
  for (size_t i = 0; i < file->artboardCount(); i++)
  {
    const Artboard* header = file->artboardHeader(i);
    if (!header->hasDeferredBody())
    {
      continue;
    }
    int before = File::deferredArtboardDecodeCount;
    auto instance = header->instance();
    CHECK(instance != nullptr);
    CHECK_FALSE(header->hasDeferredBody());
    // Its nested artboards were decoded too, as instancing nests them.
    CHECK(File::deferredArtboardDecodeCount > before);
    int after = File::deferredArtboardDecodeCount;
    auto again = header->instance();
    CHECK(File::deferredArtboardDecodeCount == after);
    break;
  }
}

// Two artboards, the second's keyed records written before any animation of
// its own (no exporter writes this): an import hangs them off the first
// artboard's animation, the one its importer left on the stack.
static std::vector<uint8_t> keyedRecordsAcrossArtboards()
{
  std::vector<uint8_t> bytes;
  VectorBinaryWriter writer(&bytes);
  writer.write((const uint8_t*)"RIVE", 4);
  writer.writeVarUint((uint32_t)File::majorVersion);
  writer.writeVarUint((uint32_t)File::minorVersion);
  writer.writeVarUint((uint32_t)0);
  // No table of contents: every key is one the runtime knows.
  writer.writeVarUint((uint32_t)0);
  auto record = [&](uint16_t typeKey) {
    writer.writeVarUint((uint32_t)typeKey);
  };
  auto uintProperty = [&](uint16_t key, uint32_t value) {
    writer.writeVarUint((uint32_t)key);
    writer.writeVarUint(value);
  };
  auto floatProperty = [&](uint16_t key, float value) {
    writer.writeVarUint((uint32_t)key);
    writer.writeFloat(value);
  };
  auto stringProperty = [&](uint16_t key, const std::string& value) {
    writer.writeVarUint((uint32_t)key);
    writer.write(value);
  };
  auto end = [&]() { writer.writeVarUint((uint32_t)0); };
  auto keyedWidth = [&](uint32_t frame, float width) {
    record(KeyedObjectBase::typeKey);
    uintProperty(KeyedObjectBase::objectIdPropertyKey, 0);
    end();
    record(KeyedPropertyBase::typeKey);
    uintProperty(
        KeyedPropertyBase::propertyKeyPropertyKey,
        LayoutComponentBase::widthPropertyKey);
    end();
    record(KeyFrameDoubleBase::typeKey);
    uintProperty(KeyFrameBase::framePropertyKey, frame);
    floatProperty(KeyFrameDoubleBase::valuePropertyKey, width);
    end();
  };
  auto artboard = [&](const char* name) {
    record(ArtboardBase::typeKey);
    stringProperty(ComponentBase::namePropertyKey, name);
    floatProperty(LayoutComponentBase::widthPropertyKey, 100);
    floatProperty(LayoutComponentBase::heightPropertyKey, 100);
    end();
  };
  record(BackboardBase::typeKey);
  end();
  artboard("first");
  record(LinearAnimationBase::typeKey);
  stringProperty(AnimationBase::namePropertyKey, "grow");
  uintProperty(LinearAnimationBase::fpsPropertyKey, 60);
  uintProperty(LinearAnimationBase::durationPropertyKey, 60);
  end();
  keyedWidth(0, 100);
  artboard("second");
  keyedWidth(30, 200);
  return bytes;
}

TEST_CASE("lazy imports decode artboards that can't wait", "[lazy_artboard]")
{
  SECTION("records borrowing an earlier artboard's parent are dropped")
  {
    // An exporter bug: an import hangs the second artboard's keyed records
    // off the first artboard's animation. Decoded on its own, the second
    // artboard has no animation for them, and doesn't borrow one.
    std::vector<uint8_t> bytes = keyedRecordsAcrossArtboards();
    ImportResult result;
    auto eager = import(bytes, false, &result, &gNoOpFactory, 0);
    REQUIRE(eager != nullptr);
    CHECK(eager->artboard(0)->animation(0)->numKeyedObjects() == 2);
    for (int lazyAnimations : {0, 1})
    {
      INFO("lazy animations " << lazyAnimations);
      auto lazy = import(bytes, true, &result, &gNoOpFactory, lazyAnimations);
      REQUIRE(lazy != nullptr);
      // The first artboard keeps only its own.
      CHECK(lazy->artboard(0)->animation(0)->numKeyedObjects() == 1);
      // The second decodes without them.
      CHECK_FALSE(lazy->artboard(1)->failedToDecode());
      CHECK(lazy->artboard(1)->animationCount() == 0);
      CHECK(lazy->deferredArtboardStats().decodeFailures == 0);
    }
  }
  SECTION("nothing is deferred unless asked")
  {
    std::vector<uint8_t> bytes = ReadFile("assets/superbowl.riv");
    ImportResult result;
    auto file = import(bytes, false, &result);
    REQUIRE(file != nullptr);
    CHECK(pendingCount(file.get()) == 0);
    CHECK(outcomeCount(file.get()) == 0);
  }
}

// The second artboard's mesh has no triangle indices, which fails it in
// initialize: an import fails whole, a deferred decode only that artboard.
// The first nests the second.
static std::vector<uint8_t> meshWithoutIndices()
{
  std::vector<uint8_t> bytes;
  VectorBinaryWriter writer(&bytes);
  writer.write((const uint8_t*)"RIVE", 4);
  writer.writeVarUint((uint32_t)File::majorVersion);
  writer.writeVarUint((uint32_t)File::minorVersion);
  writer.writeVarUint((uint32_t)0);
  // No table of contents: every key is one the runtime knows.
  writer.writeVarUint((uint32_t)0);
  auto record = [&](uint16_t typeKey) {
    writer.writeVarUint((uint32_t)typeKey);
  };
  auto uintProperty = [&](uint16_t key, uint32_t value) {
    writer.writeVarUint((uint32_t)key);
    writer.writeVarUint(value);
  };
  auto floatProperty = [&](uint16_t key, float value) {
    writer.writeVarUint((uint32_t)key);
    writer.writeFloat(value);
  };
  auto stringProperty = [&](uint16_t key, const std::string& value) {
    writer.writeVarUint((uint32_t)key);
    writer.write(value);
  };
  auto end = [&]() { writer.writeVarUint((uint32_t)0); };
  auto artboard = [&](const char* name) {
    record(ArtboardBase::typeKey);
    stringProperty(ComponentBase::namePropertyKey, name);
    floatProperty(LayoutComponentBase::widthPropertyKey, 100);
    floatProperty(LayoutComponentBase::heightPropertyKey, 100);
    end();
  };
  record(BackboardBase::typeKey);
  end();
  artboard("first");
  record(NestedArtboardBase::typeKey);
  uintProperty(ComponentBase::parentIdPropertyKey, 0);
  uintProperty(NestedArtboardBase::artboardIdPropertyKey, 1);
  end();
  artboard("second");
  record(ImageBase::typeKey);
  uintProperty(ComponentBase::parentIdPropertyKey, 0);
  end();
  record(MeshBase::typeKey);
  uintProperty(ComponentBase::parentIdPropertyKey, 1);
  end();
  return bytes;
}

TEST_CASE(
    "an artboard that fails part way through initialize instances empty",
    "[lazy_artboard]")
{
  std::vector<uint8_t> bytes = meshWithoutIndices();
  ImportResult result;
  CHECK(import(bytes, false, &result) == nullptr);
  CHECK(result == ImportResult::malformed);
  auto lazy = import(bytes, true, &result);
  REQUIRE(lazy != nullptr);
  CHECK(lazy->artboardHeader(1)->hasDeferredBody());

  // First used through the reference that nests it.
  auto host = lazy->artboardAt(0);
  REQUIRE(host != nullptr);
  REQUIRE(host->nestedArtboards().size() == 1);
  ArtboardInstance* nested = host->nestedArtboards()[0]->artboardInstance();
  REQUIRE(nested != nullptr);
  CHECK(nested->objects().size() == 1);
  CHECK(lazy->deferredArtboardStats().decodeFailures == 1);
  CHECK(lazy->artboardHeader(1)->failedToDecode());

  // The file hands out an empty artboard with its record in its place.
  Artboard* source = lazy->artboard(1);
  REQUIRE(source != nullptr);
  CHECK(source != lazy->artboardHeader(1));
  CHECK(source == lazy->artboard("second"));
  CHECK(source->name() == "second");
  CHECK(source->width() == 100);
  CHECK(source->objects().size() == 1);
  CHECK(source->animationCount() == 0);
  std::unique_ptr<ArtboardInstance> instances[] = {
      lazy->artboardAt(1),
      lazy->artboardNamed("second")};
  for (auto& instance : instances)
  {
    REQUIRE(instance != nullptr);
    CHECK(instance->artboardSource() == source);
    CHECK(instance->objects().size() == 1);
    auto stateMachine = instance->defaultStateMachine();
    if (stateMachine == nullptr)
    {
      stateMachine = instance->stateMachineAt(0);
    }
    REQUIRE(stateMachine != nullptr);
    stateMachine->advanceAndApply(0.0f);
  }
  CHECK(lazy->deferredArtboardStats().decodeFailures == 1);
}

TEST_CASE("the default artboard decodes at import", "[lazy_artboard]")
{
  std::vector<uint8_t> bytes = ReadFile("assets/superbowl.riv");
  FileImportOptions options = deferring(false, true, false);
  ImportResult result;
  auto file = File::import(
      bytes,
      &gNoOpFactory,
      &result,
      (FileAssetLoader*)nullptr,
      nullptr,
      false,
      options);
  REQUIRE(file != nullptr);
  REQUIRE(file->artboardCount() > 1);
  // The one a player opens is read as usual, never skimmed.
  CHECK_FALSE(file->artboardHeader(0)->hasDeferredBody());
  const auto& outcomes = file->deferredArtboardStats().outcomes;
  CHECK(outcomes[(size_t)ArtboardBodySkim::defaultArtboard] == 1);
  CHECK(outcomeCount(file.get()) == file->artboardCount());
  CHECK(pendingCount(file.get()) > 0);
  // Asked to, it waits like the rest.
  options.testing_deferDefaultArtboard = true;
  file = File::import(
      bytes,
      &gNoOpFactory,
      &result,
      (FileAssetLoader*)nullptr,
      nullptr,
      false,
      options);
  REQUIRE(file != nullptr);
  CHECK(file->artboardHeader(0)->hasDeferredBody());
  CHECK(
      file->deferredArtboardStats()
          .outcomes[(size_t)ArtboardBodySkim::defaultArtboard] == 0);
}

TEST_CASE("concurrent first uses decode each artboard once", "[lazy_artboard]")
{
  std::vector<uint8_t> bytes = ReadFile("assets/echo_show_demo.riv");
  ImportResult result;
  auto file = import(bytes, true, &result);
  REQUIRE(file != nullptr);
  size_t deferred = pendingCount(file.get());
  REQUIRE(deferred > 2);

  // Only the decodes race: instancing artboards of one file from several
  // threads adds to its assets' referencer lists concurrently, which is a
  // separate matter.
  int decodesBefore = File::deferredArtboardDecodeCount;
  constexpr int kThreads = 8;
  std::vector<std::thread> threads;
  for (int t = 0; t < kThreads; t++)
  {
    File* raw = file.get();
    threads.emplace_back([raw, t] {
      size_t count = raw->artboardCount();
      for (size_t n = 0; n < count; n++)
      {
        // Start at different artboards so threads meet on every body.
        raw->artboard((n + t) % count);
      }
    });
  }
  for (auto& thread : threads)
  {
    thread.join();
  }
  CHECK(pendingCount(file.get()) == 0);
  CHECK(File::deferredArtboardDecodeCount - decodesBefore == (int)deferred);
  // And each plays.
  for (size_t i = 0; i < file->artboardCount(); i++)
  {
    CHECK(file->artboardAt(i) != nullptr);
  }
}

TEST_CASE(
    "scripted interpolators in lazy artboards initialize as they do eagerly",
    "[lazy_artboard]")
{
  const char* paths[] = {
      "assets/scripted_interpolator_layout_inherit.riv",
      "assets/scripted_interpolator_transition_converter.riv",
      "assets/scripted_interpolator_converter_inputs.riv",
      "assets/data_bound_keyframe_test.riv",
  };
  // Each interpolator's source is initialized (holds a script instance) once
  // the file's scripts registered, at import, or as its artboard decodes.
  auto initialized = [](File* file) {
    std::vector<bool> states;
    for (size_t i = 0; i < file->artboardCount(); i++)
    {
      for (Core* object : file->artboard(i)->objects())
      {
        if (object != nullptr && object->is<ScriptedInterpolator>())
        {
          states.push_back(object->as<ScriptedInterpolator>()->self() != 0);
        }
      }
    }
    return states;
  };
  int interpolators = 0;
  for (const char* path : paths)
  {
    INFO(path);
    std::vector<uint8_t> bytes = ReadFile(path);
    ImportResult result;
    auto eager = import(bytes, false, &result);
    REQUIRE(eager != nullptr);
    auto lazy = import(bytes, true, &result);
    REQUIRE(lazy != nullptr);
    CHECK(
        lazy->deferredArtboardStats()
            .outcomes[(size_t)ArtboardBodySkim::deferred] > 0);
    auto eagerStates = initialized(eager.get());
    auto lazyStates = initialized(lazy.get());
    CHECK(lazyStates == eagerStates);
    interpolators += (int)eagerStates.size();
  }
  // The assets do hold artboard owned ones.
  CHECK(interpolators > 0);
}

// Decodes of every kind (artboards, animations, view model instances) take
// one lock (deferredDecodeMutex): each clones the data converters of the
// binds it reads, and a scripted converter's clone joins its asset's
// referencer list, which isn't synchronized. Run on many threads at once,
// they leave each asset with the referencers a single thread would.
TEST_CASE(
    "decodes of every kind on many threads leave assets consistent",
    "[lazy_artboard]")
{
  // Decodes everything [kind] decodes (0 artboards, 1 animations, 2 view
  // model instances; -1 all three), starting at [offset], without making
  // instances of anything.
  auto decode = [](File* file, int kind, size_t offset) {
    size_t artboards = file->artboardCount();
    if (kind != 2)
    {
      for (size_t n = 0; n < artboards; n++)
      {
        Artboard* artboard = file->artboard((n + offset) % artboards);
        if (kind == 0)
        {
          continue;
        }
        for (size_t a = 0; a < artboard->animationCount(); a++)
        {
          artboard->animation(a)->ensureDecoded();
        }
      }
    }
    if (kind == 2 || kind == -1)
    {
      for (size_t i = 0; i < file->viewModelCount(); i++)
      {
        ViewModel* viewModel = file->viewModel(i);
        for (size_t j = 0; j < viewModel->instanceCount(); j++)
        {
          viewModel->instance((j + offset) % viewModel->instanceCount());
        }
      }
    }
  };
  auto referencers = [](const File* file) {
    std::vector<size_t> counts;
    for (const rcp<FileAsset>& asset : file->assets())
    {
      counts.push_back(asset->fileAssetReferencers().size());
    }
    return counts;
  };
  size_t scriptedFiles = 0;
  for (const std::string& path : allAssets())
  {
    std::vector<uint8_t> bytes = ReadFile(path.c_str());
    ImportResult result;
    auto threaded = File::import(
        bytes,
        &gNoOpFactory,
        &result,
        (FileAssetLoader*)nullptr,
        nullptr,
        false,
        deferring(true, true, true, true));
    if (threaded == nullptr)
    {
      continue;
    }
    // Only files with scripts: their converters' clones join an asset.
    bool scripted = false;
    for (const rcp<FileAsset>& asset : threaded->assets())
    {
      scripted = scripted || asset->is<ScriptAsset>();
    }
    if (!scripted)
    {
      continue;
    }
    INFO(path);
    scriptedFiles++;
    auto serial = File::import(
        bytes,
        &gNoOpFactory,
        &result,
        (FileAssetLoader*)nullptr,
        nullptr,
        false,
        deferring(true, true, true, true));
    REQUIRE(serial != nullptr);
    decode(serial.get(), -1, 0);

    constexpr int kThreads = 6;
    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; t++)
    {
      threads.emplace_back(
          [&, t] { decode(threaded.get(), t % 3, (size_t)t); });
    }
    for (auto& thread : threads)
    {
      thread.join();
    }
    CHECK(referencers(threaded.get()) == referencers(serial.get()));
  }
  CHECK(scriptedFiles > 0);
}

// Not a test: a check for a RIVE_LAZY_DECODING=1 run of the suite. A lazy
// file makes an artboard's paints when the artboard decodes rather than at
// import, so a silver of a file with several artboards can serialize its
// resources in another order and come out tarnished while drawing exactly
// the same. This replays each tarnished silver and its baseline into SVG,
// which names nothing by creation order, and says which ones draw alike.
TEST_CASE("tarnished silvers draw as their baselines", "[.silversvg]")
{
  auto svgOf = [](const std::string& path, std::string* svg) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
      return false;
    }
    std::vector<uint8_t> bytes(
        (std::istreambuf_iterator<char>(stream)),
        std::istreambuf_iterator<char>());
    SVGFactory factory;
    SVGRenderer renderer;
    uint32_t width = 0;
    uint32_t height = 0;
    SerializedReplayHooks hooks;
    hooks.onFrameSize = [&](uint32_t w, uint32_t h) {
      width = w;
      height = h;
    };
    bool replayed = replaySerializedCommands(
        Span<const uint8_t>(bytes.data(), bytes.size()),
        &factory,
        &renderer,
        hooks);
    *svg = renderer.finalize((int)width, (int)height);
    return replayed;
  };
  REQUIRE(std::filesystem::is_directory("silvers/tarnished"));
  int alike = 0;
  int different = 0;
  for (const auto& entry :
       std::filesystem::directory_iterator("silvers/tarnished"))
  {
    std::string name = entry.path().filename().string();
    if (name.size() < 6 || name.compare(name.size() - 5, 5, ".sriv") != 0)
    {
      continue;
    }
    std::string tarnished;
    std::string baseline;
    bool replayed = svgOf("silvers/tarnished/" + name, &tarnished) &&
                    svgOf("silvers/" + name, &baseline);
    bool same = replayed && tarnished == baseline;
    printf("%s %s\n", same ? "alike    " : "DIFFERENT", name.c_str());
    (same ? alike : different)++;
  }
  printf("%d alike, %d different\n", alike, different);
  CHECK(different == 0);
}

TEST_CASE("a list row decodes no artboard before its own", "[lazy_artboard]")
{
  // Main-Vertical (artboard 2) lists rows of child (artboard 1), with an
  // override for child: finding the override's artboard index must not
  // decode Main (artboard 0).
  std::vector<uint8_t> bytes = ReadFile("assets/artboard_list_overrides.riv");
  SVGFactory eagerFactory;
  SVGFactory lazyFactory;
  ImportResult result;
  auto eager = import(bytes, false, &result, &eagerFactory);
  REQUIRE(eager != nullptr);
  auto lazy = import(bytes, true, &result, &lazyFactory);
  REQUIRE(lazy != nullptr);
  REQUIRE(lazy->artboardHeader(0)->name() == "Main");
  REQUIRE(lazy->artboardHeader(1)->name() == "child");
  auto e = svgFrames(eager.get(), 30, "Main-Vertical");
  auto l = svgFrames(lazy.get(), 30, "Main-Vertical");
  CHECK(lazy->artboardHeader(0)->hasDeferredBody());
  CHECK(!lazy->artboardHeader(1)->hasDeferredBody());
  REQUIRE(l.size() == e.size());
  for (size_t i = 0; i < e.size(); i++)
  {
    INFO("frame " << i);
    CHECK(l[i] == e[i]);
  }
}
