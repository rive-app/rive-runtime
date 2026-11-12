/*
 * Copyright 2026 Rive
 */

// Deferred animations (FileImportOptions::deferDecoding): import skips each
// LinearAnimation's keyed body and decodes it the first time the animation is
// used. These tests hold the lazy import to the eager one: the same keyed
// data once decoded, the same frames drawn, the same import result for
// damaged bodies, and one decode per body however many threads first use it.

#include "rive/animation/deferred_animation_body.hpp"
#include "rive/animation/interpolating_keyframe.hpp"
#include "rive/animation/keyed_callback_reporter.hpp"
#include "rive/animation/keyed_object.hpp"
#include "rive/animation/keyed_property.hpp"
#include "rive/animation/keyframe.hpp"
#include "rive/animation/keyframe_bool.hpp"
#include "rive/animation/keyframe_color.hpp"
#include "rive/animation/keyframe_double.hpp"
#include "rive/animation/keyframe_id.hpp"
#include "rive/animation/keyframe_string.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/animation/state_machine.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/core/binary_reader.hpp"
#include "rive/core/vector_binary_writer.hpp"
#include "rive/data_bind/converters/data_converter.hpp"
#include "rive/file.hpp"
#include "rive/generated/animation/animation_base.hpp"
#include "rive/generated/animation/interpolating_keyframe_base.hpp"
#include "rive/generated/animation/keyed_object_base.hpp"
#include "rive/generated/animation/keyed_property_base.hpp"
#include "rive/generated/animation/keyframe_base.hpp"
#include "rive/generated/animation/keyframe_color_base.hpp"
#include "rive/generated/animation/keyframe_double_base.hpp"
#include "rive/generated/animation/linear_animation_base.hpp"
#include "rive/generated/artboard_base.hpp"
#include "rive/generated/backboard_base.hpp"
#include "rive/generated/component_base.hpp"
#include "rive/generated/data_bind/converters/data_converter_rounder_base.hpp"
#include "rive/generated/data_bind/converters/data_converter_to_string_base.hpp"
#include "rive/generated/data_bind/data_bind_base.hpp"
#include "rive/generated/layout_component_base.hpp"
#include "rive/generated/shapes/paint/solid_color_base.hpp"
#include "rive/importers/runtime_object_reader.hpp"
#include "rive/runtime_header.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include "utils/serializing_factory.hpp"
#include "lazy_decode_test_helpers.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

using namespace rive;
using namespace lazy_decode_test;

namespace
{
// [decodedDuringImport] counts the deferred bodies the import itself went on
// to decode: a Joystick reads its animations as its artboard initializes, and
// nested artboards instanced at import play theirs. Only animations are
// deferred, whatever RIVE_LAZY_DECODING says: these tests count deferred
// animations right after import, and decoding an artboard later would defer
// more.
rcp<File> import(
    const std::vector<uint8_t>& bytes,
    bool lazy,
    ImportResult* result,
    Factory* factory = &gNoOpFactory,
    int* decodedDuringImport = nullptr)
{
  FileImportOptions options = deferring(lazy, false, false);
  int decodes = LinearAnimation::deferredDecodeCount;
  auto file = File::import(
      bytes,
      factory,
      result,
      (FileAssetLoader*)nullptr,
      nullptr,
      false,
      options);
  if (decodedDuringImport != nullptr)
  {
    *decodedDuringImport = LinearAnimation::deferredDecodeCount - decodes;
  }
  return file;
}

std::vector<const LinearAnimation*> animationsOf(const File* file)
{
  std::vector<const LinearAnimation*> animations;
  for (size_t i = 0; i < file->artboardCount(); i++)
  {
    Artboard* artboard = file->artboard(i);
    for (size_t j = 0; j < artboard->animationCount(); j++)
    {
      animations.push_back(artboard->animation(j));
    }
  }
  return animations;
}

size_t deferredCount(const File* file)
{
  size_t count = 0;
  for (auto animation : animationsOf(file))
  {
    count += animation->hasDeferredBody() ? 1 : 0;
  }
  return count;
}

// Decodes every deferred body of [lazy] and requires the same keyed data as
// [eager] holds.
void requireSameAnimations(const File* eager, const File* lazy)
{
  auto eagerAnimations = animationsOf(eager);
  auto lazyAnimations = animationsOf(lazy);
  REQUIRE(lazyAnimations.size() == eagerAnimations.size());
  for (size_t a = 0; a < eagerAnimations.size(); a++)
  {
    requireSameAnimation(eagerAnimations[a], lazyAnimations[a]);
  }
}

// The offsets of every LinearAnimation body the lazy import would defer in
// [bytes], as [start, end) pairs, as far as the stream can be read.
std::vector<std::pair<size_t, size_t>> deferrableBodies(
    const std::vector<uint8_t>& bytes)
{
  std::vector<std::pair<size_t, size_t>> bodies;
  BinaryReader reader(bytes);
  RuntimeHeader header;
  if (!RuntimeHeader::read(reader, header))
  {
    return bodies;
  }
  while (!reader.reachedEnd())
  {
    auto typeKey = reader.readVarUintAs<int>();
    while (true)
    {
      auto propertyKey = reader.readVarUintAs<uint16_t>();
      if (propertyKey == 0 || reader.hasError())
      {
        break;
      }
      int fieldId = runtimePropertyFieldId(header, propertyKey);
      if (fieldId == -1)
      {
        return bodies;
      }
      skipRuntimePropertyValue(reader, fieldId);
    }
    if (reader.hasError())
    {
      return bodies;
    }
    if (typeKey == LinearAnimationBase::typeKey)
    {
      AnimationBodySkimResult skim = skimAnimationBody(reader, header);
      if (skim.outcome == AnimationBodySkim::deferred)
      {
        size_t start = (size_t)(skim.bytes.data() - bytes.data());
        bodies.push_back({start, start + skim.bytes.size()});
      }
    }
  }
  return bodies;
}

// Where each record of [bytes] starts and what type it is, or empty if the
// stream can't be read through.
std::vector<std::pair<size_t, int>> recordLayout(
    const std::vector<uint8_t>& bytes)
{
  std::vector<std::pair<size_t, int>> records;
  BinaryReader reader(bytes);
  RuntimeHeader header;
  if (!RuntimeHeader::read(reader, header))
  {
    return {};
  }
  while (!reader.reachedEnd())
  {
    size_t offset = (size_t)(reader.position() - bytes.data());
    records.push_back({offset, reader.readVarUintAs<int>()});
    while (true)
    {
      auto propertyKey = reader.readVarUintAs<uint16_t>();
      if (propertyKey == 0 || reader.hasError())
      {
        break;
      }
      int fieldId = runtimePropertyFieldId(header, propertyKey);
      if (fieldId == -1)
      {
        return {};
      }
      skipRuntimePropertyValue(reader, fieldId);
    }
    if (reader.hasError())
    {
      return {};
    }
  }
  return records;
}
} // namespace

TEST_CASE(
    "lazy animation bodies decode to what an eager import reads",
    "[lazy_animation]")
{
  for (const std::string& path : allAssets())
  {
    INFO(path);
    std::vector<uint8_t> bytes = ReadFile(path.c_str());
    ImportResult eagerResult;
    ImportResult lazyResult;
    int decodedDuringImport = 0;
    auto eager = import(bytes, false, &eagerResult);
    auto lazy = import(
        bytes,
        true,
        &lazyResult,
        &gNoOpFactory,
        &decodedDuringImport);
    REQUIRE(lazyResult == eagerResult);
    if (eager == nullptr)
    {
      continue;
    }
    REQUIRE(lazy != nullptr);

    // Every body was either deferred or decoded for a reason. The deferred
    // ones are still pending unless the import itself used them, and hold
    // at most the bytes the file counted.
    auto animations = animationsOf(lazy.get());
    const File::DeferredAnimationStats& stats = lazy->deferredAnimationStats();
    uint32_t outcomes = 0;
    for (size_t i = 0; i < kAnimationBodySkimCount; i++)
    {
      outcomes += stats.outcomes[i];
    }
    CHECK(outcomes == animations.size());
    CHECK(
        deferredCount(lazy.get()) ==
        stats.outcomes[(size_t)AnimationBodySkim::deferred] -
            decodedDuringImport);
    size_t heldBytes = 0;
    for (auto animation : animations)
    {
      heldBytes += animation->deferredBodySize();
    }
    CHECK(heldBytes <= stats.deferredBytes);
    if (decodedDuringImport == 0)
    {
      CHECK(heldBytes == stats.deferredBytes);
    }

    requireSameAnimations(eager.get(), lazy.get());
    CHECK(deferredCount(lazy.get()) == 0);
  }
}

TEST_CASE("lazy animation bodies draw the same frames", "[lazy_animation]")
{
  const char* paths[] = {
      "assets/car_widgets_v01.riv",
      "assets/hunter_x_demo.riv",
      "assets/zombie_skins.riv",
      "assets/echo_show_demo.riv",
      "assets/db_health_tracker.riv",
      "assets/data_viz_demo.riv",
      "assets/data_bind_keyframes_test.riv",
      "assets/joystick_flag_test.riv",
      "assets/joystick_nested_remap.riv",
      "assets/joystick_databound_keyframe_test.riv",
      "assets/animated_clipping.riv",
      "assets/clipping_and_draw_order.riv",
      "assets/trim_path.riv",
  };
  for (const char* path : paths)
  {
    INFO(path);
    std::vector<uint8_t> bytes = ReadFile(path);
    SerializingFactory eagerSilver;
    SerializingFactory lazySilver;
    ImportResult result;
    auto eager = import(bytes, false, &result, &eagerSilver);
    REQUIRE(eager != nullptr);
    auto lazy = import(bytes, true, &result, &lazySilver);
    REQUIRE(lazy != nullptr);
    play(eager.get(), eagerSilver, 90);
    play(lazy.get(), lazySilver, 90);
    Span<const uint8_t> e = eagerSilver.bytes();
    Span<const uint8_t> l = lazySilver.bytes();
    REQUIRE(l.size() == e.size());
    CHECK(std::equal(e.begin(), e.end(), l.begin()));
  }
}

TEST_CASE("lazy animation bodies wait for their first use", "[lazy_animation]")
{
  std::vector<uint8_t> bytes = ReadFile("assets/zombie_skins.riv");
  ImportResult result;
  int decodedDuringImport = 0;
  auto file = import(bytes, true, &result, &gNoOpFactory, &decodedDuringImport);
  REQUIRE(file != nullptr);
  const File::DeferredAnimationStats& stats = file->deferredAnimationStats();
  size_t deferred = stats.outcomes[(size_t)AnimationBodySkim::deferred] -
                    decodedDuringImport;
  REQUIRE(deferred > 1);
  CHECK(deferredCount(file.get()) == deferred);

  int decodesBefore = LinearAnimation::deferredDecodeCount;
  auto artboard = file->artboardDefault();
  auto stateMachine = artboard->defaultStateMachine();
  if (stateMachine == nullptr)
  {
    stateMachine = artboard->stateMachineAt(0);
  }
  REQUIRE(stateMachine != nullptr);
  stateMachine->advanceAndApply(0.0f);
  stateMachine->advanceAndApply(1.0f / 60.0f);
  // The first frame decodes what it plays, and leaves the rest.
  REQUIRE(stateMachine->currentAnimationCount() > 0);
  for (size_t i = 0; i < stateMachine->currentAnimationCount(); i++)
  {
    CHECK_FALSE(stateMachine->currentAnimationByIndex(i)
                    ->animation()
                    ->hasDeferredBody());
  }
  size_t stillDeferred = deferredCount(file.get());
  CHECK(stillDeferred < deferred);
  CHECK(stillDeferred > 0);
  CHECK(
      LinearAnimation::deferredDecodeCount - decodesBefore ==
      (int)(deferred - stillDeferred));

  // Instancing an animation decodes it and frees its bytes; a second use
  // decodes nothing.
  for (auto animation : animationsOf(file.get()))
  {
    if (animation->hasDeferredBody())
    {
      CHECK(animation->deferredBodySize() > 0);
      LinearAnimationInstance instance(animation, artboard.get());
      CHECK_FALSE(animation->hasDeferredBody());
      CHECK(animation->deferredBodySize() == 0);
      int decodes = LinearAnimation::deferredDecodeCount;
      LinearAnimationInstance again(animation, artboard.get());
      CHECK(LinearAnimation::deferredDecodeCount == decodes);
      break;
    }
  }
}

TEST_CASE("what lazy imports defer", "[lazy_animation]")
{
  SECTION("keyframe data binds are deferred with their keyframes")
  {
    std::vector<uint8_t> bytes = ReadFile(
        "assets/data_bind_keyframes_test.riv");
    ImportResult result;
    auto file = import(bytes, true, &result);
    REQUIRE(file != nullptr);
    const File::DeferredAnimationStats& stats = file->deferredAnimationStats();
    CHECK(stats.outcomes[(size_t)AnimationBodySkim::dataBind] == 0);
    CHECK(stats.outcomes[(size_t)AnimationBodySkim::deferred] > 0);
    // Known without decoding, and found once decoded.
    bool anyBinds = false;
    for (auto animation : animationsOf(file.get()))
    {
      if (animation->hasKeyFrameBinds() && animation->hasDeferredBody())
      {
        anyBinds = true;
        CHECK_FALSE(animation->keyFrameBinds().empty());
        CHECK_FALSE(animation->hasDeferredBody());
      }
    }
    CHECK(anyBinds);
  }
  SECTION("nothing is deferred unless asked")
  {
    std::vector<uint8_t> bytes = ReadFile("assets/car_widgets_v01.riv");
    ImportResult result;
    auto file = import(bytes, false, &result);
    REQUIRE(file != nullptr);
    CHECK(deferredCount(file.get()) == 0);
    uint32_t outcomes = 0;
    for (size_t i = 0; i < kAnimationBodySkimCount; i++)
    {
      outcomes += file->deferredAnimationStats().outcomes[i];
    }
    CHECK(outcomes == 0);
  }
}

TEST_CASE(
    "damaged lazy bodies import as they would eagerly",
    "[lazy_animation]")
{
  std::vector<uint8_t> original = ReadFile("assets/joystick_flag_test.riv");
  auto bodies = deferrableBodies(original);
  REQUIRE(!bodies.empty());
  auto originalLayout = recordLayout(original);
  REQUIRE(!originalLayout.empty());
  // A few hundred positions spread over every body, each given values that
  // hit out-of-range varuints, unknown keys and out-of-order records.
  const uint8_t values[] = {0x00, 0x01, 0x19, 0x1A, 0x1E, 0x7F, 0x80, 0xFF};
  size_t total = 0;
  for (auto& body : bodies)
  {
    total += body.second - body.first;
  }
  size_t stride = std::max<size_t>(1, total / 300);
  size_t index = 0;
  for (auto& body : bodies)
  {
    for (size_t at = body.first; at < body.second; at++, index++)
    {
      if (index % stride != 0)
      {
        continue;
      }
      for (uint8_t value : values)
      {
        if (original[at] == value)
        {
          continue;
        }
        std::vector<uint8_t> bytes = original;
        bytes[at] = value;
        INFO("offset " << at << " value " << (int)value);
        ImportResult eagerResult;
        ImportResult lazyResult;
        auto eager = import(bytes, false, &eagerResult);
        auto lazy = import(bytes, true, &lazyResult);
        REQUIRE(lazyResult == eagerResult);
        if (eager == nullptr || lazy == nullptr)
        {
          continue;
        }
        // When the damage leaves every record where it was and of the same
        // type, only values changed, and the two must read the same. Damage
        // that moves records can leave a body's remainder after a record
        // that ends it, or an unknown record at a body's start: the eager
        // import then hangs the strays off the previous body's importers,
        // still on its stack, which the lazy one can't reach into an
        // undecoded body to repeat. Those need only import alike.
        bool moved = recordLayout(bytes) != originalLayout;
        if (!moved)
        {
          requireSameAnimations(eager.get(), lazy.get());
        }
        else
        {
          // Still decodes cleanly.
          for (auto animation : animationsOf(lazy.get()))
          {
            animation->ensureDecoded();
          }
        }
      }
    }
  }
}

// What each animation keys, as a state machine reads it to work out which
// properties its layers share.
static std::vector<std::pair<uint32_t, uint32_t>> keyedProperties(
    const LinearAnimation* animation)
{
  std::vector<std::pair<uint32_t, uint32_t>> keyed;
  animation->forEachKeyedProperty([&](uint32_t objectId, uint32_t key) {
    keyed.push_back({objectId, key});
  });
  return keyed;
}

// A state machine works out which properties two or more of its layers key
// as its first instance is made (StateMachine::layerConflicts), before any
// of its states play. It reads deferred bodies where they lie, keeping what
// a decode would keep, and decodes none of them.
TEST_CASE(
    "layer conflicts read deferred bodies without decoding them",
    "[lazy_animation]")
{
  size_t machinesWithConflicts = 0;
  for (const std::string& path : allAssets())
  {
    INFO(path);
    std::vector<uint8_t> bytes = ReadFile(path.c_str());
    ImportResult eagerResult;
    ImportResult lazyResult;
    auto eager = import(bytes, false, &eagerResult);
    auto lazy = import(bytes, true, &lazyResult);
    if (eager == nullptr || lazy == nullptr)
    {
      continue;
    }
    int decodes = LinearAnimation::deferredDecodeCount;
    auto eagerAnimations = animationsOf(eager.get());
    auto lazyAnimations = animationsOf(lazy.get());
    REQUIRE(lazyAnimations.size() == eagerAnimations.size());
    for (size_t i = 0; i < lazyAnimations.size(); i++)
    {
      CHECK(
          keyedProperties(lazyAnimations[i]) ==
          keyedProperties(eagerAnimations[i]));
    }
    for (size_t a = 0; a < lazy->artboardCount(); a++)
    {
      const Artboard* eagerArtboard = eager->artboard(a);
      const Artboard* lazyArtboard = lazy->artboard(a);
      REQUIRE(
          lazyArtboard->stateMachineCount() ==
          eagerArtboard->stateMachineCount());
      for (size_t m = 0; m < lazyArtboard->stateMachineCount(); m++)
      {
        const auto& e = eagerArtboard->stateMachine(m)->layerConflicts();
        const auto& l = lazyArtboard->stateMachine(m)->layerConflicts();
        REQUIRE(l.size() == e.size());
        for (size_t i = 0; i < l.size(); i++)
        {
          CHECK(l[i].objectId == e[i].objectId);
          CHECK(l[i].propertyKey == e[i].propertyKey);
          CHECK(l[i].kind == e[i].kind);
        }
        machinesWithConflicts += e.empty() ? 0 : 1;
      }
    }
    CHECK(LinearAnimation::deferredDecodeCount == decodes);
  }
  // The assets do have machines whose layers share properties.
  CHECK(machinesWithConflicts > 0);
}

TEST_CASE(
    "whether an artboard runs scripts reads deferred bodies without decoding",
    "[lazy_animation]")
{
  // A state machine asks as it's made, before any of its states play.
  auto hasConverter = [](DataConverter* converter) {
    return converter != nullptr;
  };
  size_t animationsWithConverters = 0;
  for (const std::string& path : allAssets())
  {
    INFO(path);
    std::vector<uint8_t> bytes = ReadFile(path.c_str());
    ImportResult eagerResult;
    ImportResult lazyResult;
    auto eager = import(bytes, false, &eagerResult);
    auto lazy = import(bytes, true, &lazyResult);
    if (eager == nullptr || lazy == nullptr)
    {
      continue;
    }
    int decodes = LinearAnimation::deferredDecodeCount;
    auto eagerAnimations = animationsOf(eager.get());
    auto lazyAnimations = animationsOf(lazy.get());
    REQUIRE(lazyAnimations.size() == eagerAnimations.size());
    for (size_t i = 0; i < lazyAnimations.size(); i++)
    {
      bool e = eagerAnimations[i]->anyKeyFrameBindConverter(hasConverter);
      CHECK(lazyAnimations[i]->anyKeyFrameBindConverter(hasConverter) == e);
      animationsWithConverters += e ? 1 : 0;
    }
    for (size_t a = 0; a < lazy->artboardCount(); a++)
    {
      CHECK(
          lazy->artboard(a)->usesScripts() ==
          eager->artboard(a)->usesScripts());
    }
    CHECK(LinearAnimation::deferredDecodeCount == decodes);
  }
  // The assets do bind keyframes through converters.
  CHECK(animationsWithConverters > 0);
}

// One artboard whose animation keys what a decode drops (see
// KeyedObject::onAddedDirty): a missing object, a property its object
// doesn't support, and keyframes naming interpolators that don't resolve,
// each of which drops its whole keyed object. The binds on the keyframes it
// keeps go through the file's rounder; those on the ones it drops, through
// its to-string converter.
static std::vector<uint8_t> keyedPropertiesADecodeDrops()
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
  auto keyedObject = [&](uint32_t objectId) {
    record(KeyedObjectBase::typeKey);
    uintProperty(KeyedObjectBase::objectIdPropertyKey, objectId);
    end();
  };
  auto keyedProperty = [&](uint16_t propertyKey) {
    record(KeyedPropertyBase::typeKey);
    uintProperty(KeyedPropertyBase::propertyKeyPropertyKey, propertyKey);
    end();
  };
  constexpr uint32_t noInterpolator = 0xFFFFFFFFu;
  auto keyFrameDouble = [&](uint32_t interpolatorId) {
    record(KeyFrameDoubleBase::typeKey);
    uintProperty(KeyFrameBase::framePropertyKey, 0);
    if (interpolatorId != noInterpolator)
    {
      uintProperty(
          InterpolatingKeyFrameBase::interpolatorIdPropertyKey,
          interpolatorId);
    }
    floatProperty(KeyFrameDoubleBase::valuePropertyKey, 50);
    end();
  };
  auto keyFrameColor = [&](uint32_t interpolatorId) {
    record(KeyFrameColorBase::typeKey);
    uintProperty(KeyFrameBase::framePropertyKey, 0);
    if (interpolatorId != noInterpolator)
    {
      uintProperty(
          InterpolatingKeyFrameBase::interpolatorIdPropertyKey,
          interpolatorId);
    }
    end();
  };
  constexpr uint32_t rounder = 0;
  constexpr uint32_t toString = 1;
  auto bind = [&](uint16_t propertyKey, uint32_t converterId) {
    record(DataBindBase::typeKey);
    uintProperty(DataBindBase::propertyKeyPropertyKey, propertyKey);
    uintProperty(DataBindBase::converterIdPropertyKey, converterId);
    end();
  };
  const uint16_t doubleValue = KeyFrameDoubleBase::valuePropertyKey;
  const uint16_t colorValue = KeyFrameColorBase::valuePropertyKey;
  const uint16_t width = LayoutComponentBase::widthPropertyKey;
  const uint16_t height = LayoutComponentBase::heightPropertyKey;
  // An artboard has no color of its own.
  const uint16_t unsupported = SolidColorBase::colorValuePropertyKey;

  record(BackboardBase::typeKey);
  end();
  record(DataConverterRounderBase::typeKey);
  end();
  record(DataConverterToStringBase::typeKey);
  end();
  record(ArtboardBase::typeKey);
  stringProperty(ComponentBase::namePropertyKey, "main");
  floatProperty(width, 100);
  floatProperty(height, 100);
  end();
  record(LinearAnimationBase::typeKey);
  stringProperty(AnimationBase::namePropertyKey, "keys");
  uintProperty(LinearAnimationBase::fpsPropertyKey, 60);
  uintProperty(LinearAnimationBase::durationPropertyKey, 60);
  end();
  // Kept: width. Dropped: the unsupported property, and height, which has
  // no keyframe.
  keyedObject(0);
  keyedProperty(width);
  keyFrameDouble(noInterpolator);
  bind(doubleValue, rounder);
  keyedProperty(unsupported);
  keyFrameColor(noInterpolator);
  bind(colorValue, toString);
  keyedProperty(height);
  // Dropped: no object 57.
  keyedObject(57);
  keyedProperty(width);
  keyFrameDouble(noInterpolator);
  bind(doubleValue, toString);
  // Dropped whole: object 0, the artboard, is no interpolator.
  keyedObject(0);
  keyedProperty(height);
  keyFrameDouble(noInterpolator);
  bind(doubleValue, toString);
  keyedProperty(width);
  keyFrameDouble(0);
  // Dropped whole: no object 40.
  keyedObject(0);
  keyedProperty(height);
  keyFrameDouble(40);
  bind(doubleValue, toString);
  // Kept: height. The unsupported property goes before its keyframe's
  // interpolator is looked at.
  keyedObject(0);
  keyedProperty(unsupported);
  keyFrameColor(0);
  bind(colorValue, toString);
  keyedProperty(height);
  keyFrameDouble(noInterpolator);
  bind(doubleValue, rounder);
  return bytes;
}

TEST_CASE(
    "deferred bodies list only the keyed properties a decode keeps",
    "[lazy_animation]")
{
  std::vector<uint8_t> bytes = keyedPropertiesADecodeDrops();
  ImportResult result;
  auto eager = import(bytes, false, &result);
  REQUIRE(eager != nullptr);
  auto lazy = import(bytes, true, &result);
  REQUIRE(lazy != nullptr);
  const LinearAnimation* eagerAnimation = eager->artboard(0)->animation(0);
  const LinearAnimation* lazyAnimation = lazy->artboard(0)->animation(0);
  REQUIRE(lazyAnimation->hasDeferredBody());

  const uint32_t width = LayoutComponentBase::widthPropertyKey;
  const uint32_t height = LayoutComponentBase::heightPropertyKey;
  const std::vector<std::pair<uint32_t, uint32_t>> kept = {
      {0, width},
      {0, height},
  };
  CHECK(keyedProperties(eagerAnimation) == kept);
  CHECK(keyedProperties(lazyAnimation) == kept);
  CHECK(lazyAnimation->hasDeferredBody());
  // The binds' converters, of the kept keyframes only.
  auto rounds = [](DataConverter* converter) {
    return converter != nullptr && converter->is<DataConverterRounderBase>();
  };
  auto stringifies = [](DataConverter* converter) {
    return converter != nullptr && converter->is<DataConverterToStringBase>();
  };
  CHECK(eagerAnimation->anyKeyFrameBindConverter(rounds));
  CHECK_FALSE(eagerAnimation->anyKeyFrameBindConverter(stringifies));
  int decodes = LinearAnimation::deferredDecodeCount;
  CHECK(lazyAnimation->anyKeyFrameBindConverter(rounds));
  CHECK_FALSE(lazyAnimation->anyKeyFrameBindConverter(stringifies));
  CHECK(LinearAnimation::deferredDecodeCount == decodes);
  CHECK(lazyAnimation->hasDeferredBody());
  // And decoded, it holds what it listed.
  lazyAnimation->ensureDecoded();
  CHECK(keyedProperties(lazyAnimation) == kept);
  CHECK(lazyAnimation->anyKeyFrameBindConverter(rounds));
  CHECK_FALSE(lazyAnimation->anyKeyFrameBindConverter(stringifies));
}

TEST_CASE("concurrent first uses decode each body once", "[lazy_animation]")
{
  std::vector<uint8_t> bytes = ReadFile("assets/car_widgets_v01.riv");
  ImportResult result;
  auto file = import(bytes, true, &result);
  REQUIRE(file != nullptr);
  size_t deferred = deferredCount(file.get());
  REQUIRE(deferred > 1);

  // Each thread plays every animation on its own instances of every
  // artboard, made here: only the shared animations are contended. (Their
  // update passes are not run: instances of one file share its fonts, and
  // shaping text from several threads at once is a separate matter.)
  constexpr int kThreads = 8;
  std::vector<std::vector<std::unique_ptr<ArtboardInstance>>> artboards(
      kThreads);
  for (auto& instances : artboards)
  {
    for (size_t i = 0; i < file->artboardCount(); i++)
    {
      instances.push_back(file->artboardAt(i));
    }
  }

  int decodesBefore = LinearAnimation::deferredDecodeCount;
  std::vector<std::thread> threads;
  for (int t = 0; t < kThreads; t++)
  {
    auto* instances = &artboards[t];
    threads.emplace_back([instances, t] {
      // Start at different artboards so threads meet on every body.
      size_t count = instances->size();
      for (size_t n = 0; n < count; n++)
      {
        ArtboardInstance* artboard = (*instances)[(n + t) % count].get();
        for (size_t a = 0; a < artboard->animationCount(); a++)
        {
          LinearAnimationInstance instance(artboard->animation(a), artboard);
          instance.apply(1.0f);
        }
      }
    });
  }
  for (auto& thread : threads)
  {
    thread.join();
  }
  CHECK(deferredCount(file.get()) == 0);
  CHECK(LinearAnimation::deferredDecodeCount - decodesBefore == (int)deferred);
}
