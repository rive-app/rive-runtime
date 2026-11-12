/*
 * Copyright 2026 Rive
 */

// Shared by the lazy decoding tests (lazy_animation_test.cpp,
// lazy_artboard_test.cpp, lazy_viewmodel_instance_test.cpp): every asset,
// keyed data compared value by value, and a default scene played into a
// silver or drawn to SVG.

#ifndef _RIVE_LAZY_DECODE_TEST_HELPERS_HPP_
#define _RIVE_LAZY_DECODE_TEST_HELPERS_HPP_

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
#include "rive/animation/state_machine_instance.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/file.hpp"
#include "rive/scene.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive_testing.hpp"
#include "utils/serializing_factory.hpp"
#include "utils/svg_renderer.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace lazy_decode_test
{
using namespace rive;

// Import options deferring exactly the parts asked for (see
// FileImportOptions::deferDecoding).
inline FileImportOptions deferring(
    bool animations,
    bool artboards,
    bool viewModelInstances,
    bool defaultArtboard = false)
{
  FileImportOptions options;
  options.deferDecoding = animations || artboards || viewModelInstances;
  options.testing_deferAnimations = animations;
  options.testing_deferArtboards = artboards;
  options.testing_deferViewModelInstances = viewModelInstances;
  options.testing_deferDefaultArtboard = defaultArtboard;
  return options;
}

// Whether the suite runs deferred (RIVE_LAZY_DECODING=1).
inline bool deferringByDefault()
{
  return File::testing_defaultImportOptions.deferDecoding;
}

struct ReportedCallback
{
  uint32_t objectId;
  uint32_t propertyKey;
  float elapsedSeconds;
  bool operator==(const ReportedCallback& other) const
  {
    return objectId == other.objectId && propertyKey == other.propertyKey &&
           elapsedSeconds == other.elapsedSeconds;
  }
};

class CollectingReporter : public KeyedCallbackReporter
{
public:
  void reportKeyedCallback(
      uint32_t objectId,
      uint32_t propertyKey,
      float elapsedSeconds) override
  {
    callbacks.push_back({objectId, propertyKey, elapsedSeconds});
  }
  std::vector<ReportedCallback> callbacks;
};

inline std::vector<ReportedCallback> callbacksOver(
    const LinearAnimation* animation)
{
  CollectingReporter reporter;
  animation->reportKeyedCallbacks(
      &reporter,
      animation->startSeconds(),
      animation->endSeconds(),
      1.0f,
      false);
  return reporter.callbacks;
}

// Bitwise, so a damaged value that reads as NaN still compares.
inline bool same(float a, float b)
{
  return memcmp(&a, &b, sizeof(float)) == 0;
}

inline void requireSameKeyFrame(const KeyFrame* eager, const KeyFrame* lazy)
{
  REQUIRE(lazy->coreType() == eager->coreType());
  CHECK(lazy->frame() == eager->frame());
  CHECK(same(lazy->seconds(), eager->seconds()));
  if (eager->is<InterpolatingKeyFrame>())
  {
    auto e = eager->as<InterpolatingKeyFrame>();
    auto l = lazy->as<InterpolatingKeyFrame>();
    CHECK(l->interpolationType() == e->interpolationType());
    CHECK(l->interpolatorId() == e->interpolatorId());
    CHECK((l->interpolator() == nullptr) == (e->interpolator() == nullptr));
  }
  switch (eager->coreType())
  {
    case KeyFrameDouble::typeKey:
      CHECK(same(
          lazy->as<KeyFrameDouble>()->value(),
          eager->as<KeyFrameDouble>()->value()));
      break;
    case KeyFrameColor::typeKey:
      CHECK(
          lazy->as<KeyFrameColor>()->value() ==
          eager->as<KeyFrameColor>()->value());
      break;
    case KeyFrameBool::typeKey:
      CHECK(
          lazy->as<KeyFrameBool>()->value() ==
          eager->as<KeyFrameBool>()->value());
      break;
    case KeyFrameId::typeKey:
      CHECK(
          lazy->as<KeyFrameId>()->value() == eager->as<KeyFrameId>()->value());
      break;
    case KeyFrameString::typeKey:
      CHECK(
          lazy->as<KeyFrameString>()->value() ==
          eager->as<KeyFrameString>()->value());
      break;
  }
}

// Requires the same keyed data of two animations, decoding [l] if the import
// deferred its body.
inline void requireSameAnimation(
    const LinearAnimation* e,
    const LinearAnimation* l)
{
  CHECK(l->name() == e->name());
  // Decodes l.
  REQUIRE(l->numKeyedObjects() == e->numKeyedObjects());
  CHECK_FALSE(l->hasDeferredBody());
  CHECK(l->deferredBodySize() == 0);
  for (size_t o = 0; o < e->numKeyedObjects(); o++)
  {
    const KeyedObject* eo = e->getObject(o);
    const KeyedObject* lo = l->getObject(o);
    CHECK(lo->objectId() == eo->objectId());
    REQUIRE(lo->numKeyedProperties() == eo->numKeyedProperties());
    for (size_t p = 0; p < eo->numKeyedProperties(); p++)
    {
      const KeyedProperty* ep = eo->getProperty(p);
      const KeyedProperty* lp = lo->getProperty(p);
      CHECK(lp->propertyKey() == ep->propertyKey());
      REQUIRE(lp->numKeyFrames() == ep->numKeyFrames());
      for (size_t k = 0; k < ep->numKeyFrames(); k++)
      {
        requireSameKeyFrame(ep->getKeyFrame(k), lp->getKeyFrame(k));
      }
    }
  }
  float start = e->startSeconds();
  float end = e->endSeconds();
  for (int i = -1; i <= 11; i++)
  {
    float seconds = start + (end - start) * (float)i / 10.0f;
    CHECK(
        l->appliesKeyFrameValuesAt(seconds) ==
        e->appliesKeyFrameValuesAt(seconds));
  }
  CHECK(callbacksOver(l) == callbacksOver(e));

  // The keyframes' binds, which the animation owns: as many, on the same
  // keyframes (by position), of the same kind.
  CHECK(l->hasKeyFrameBinds() == e->hasKeyFrameBinds());
  auto positions = [](const LinearAnimation* animation) {
    std::unordered_map<const Core*, std::array<size_t, 3>> positions;
    for (size_t o = 0; o < animation->numKeyedObjects(); o++)
    {
      const KeyedObject* object = animation->getObject(o);
      for (size_t p = 0; p < object->numKeyedProperties(); p++)
      {
        const KeyedProperty* property = object->getProperty(p);
        for (size_t k = 0; k < property->numKeyFrames(); k++)
        {
          positions[property->getKeyFrame(k)] = {o, p, k};
        }
      }
    }
    return positions;
  };
  auto ePositions = positions(e);
  auto lPositions = positions(l);
  const auto& eBinds = e->keyFrameBinds();
  const auto& lBinds = l->keyFrameBinds();
  REQUIRE(lBinds.size() == eBinds.size());
  for (size_t i = 0; i < eBinds.size(); i++)
  {
    REQUIRE(ePositions.count(eBinds[i]->target()) == 1);
    REQUIRE(lPositions.count(lBinds[i]->target()) == 1);
    CHECK(lPositions[lBinds[i]->target()] == ePositions[eBinds[i]->target()]);
    CHECK(lBinds[i]->coreType() == eBinds[i]->coreType());
    CHECK(lBinds[i]->propertyKey() == eBinds[i]->propertyKey());
    CHECK(
        (lBinds[i]->converter() == nullptr) ==
        (eBinds[i]->converter() == nullptr));
  }
}

inline std::vector<std::string> allAssets()
{
  std::vector<std::string> names;
  REQUIRE(std::filesystem::is_directory("assets"));
  for (const auto& entry : std::filesystem::directory_iterator("assets"))
  {
    std::string name = entry.path().filename().string();
    if (name.size() > 4 && name.compare(name.size() - 4, 4, ".riv") == 0)
    {
      names.push_back("assets/" + name);
    }
  }
  std::sort(names.begin(), names.end());
  return names;
}

// The default scene of [file]'s default artboard (or [artboardName]) as play()
// picks it, each frame drawn to SVG. SVG names nothing by the order the
// factory made it, which a lazy file changes: it makes an artboard's paints
// as the artboard decodes rather than at import.
inline std::vector<std::string> svgFrames(
    File* file,
    int frames,
    const char* artboardName = nullptr)
{
  std::vector<std::string> svgs;
  auto artboard = artboardName != nullptr ? file->artboardNamed(artboardName)
                                          : file->artboardDefault();
  REQUIRE(artboard != nullptr);
  std::unique_ptr<Scene> scene = artboard->defaultStateMachine();
  if (scene == nullptr && artboard->stateMachineCount() > 0)
  {
    scene = artboard->stateMachineAt(0);
  }
  if (scene == nullptr)
  {
    scene = artboard->animationAt(0);
  }
  auto vmi = file->createDefaultViewModelInstance(artboard.get());
  if (vmi != nullptr)
  {
    artboard->bindViewModelInstance(vmi);
    if (scene != nullptr)
    {
      scene->bindViewModelInstance(vmi);
    }
  }
  for (int i = 0; i < frames; i++)
  {
    float elapsed = i == 0 ? 0.0f : 1.0f / 30.0f;
    if (scene != nullptr)
    {
      scene->advanceAndApply(elapsed);
    }
    else
    {
      artboard->advance(elapsed);
    }
    SVGRenderer renderer;
    artboard->draw(&renderer);
    svgs.push_back(
        renderer.finalize((int)artboard->width(), (int)artboard->height()));
  }
  return svgs;
}

// Advances the default scene of [file]'s default artboard for [frames]
// frames, drawing each into [silver].
inline void play(File* file, SerializingFactory& silver, int frames)
{
  auto artboard = file->artboardDefault();
  REQUIRE(artboard != nullptr);
  silver.frameSize((uint32_t)artboard->width(), (uint32_t)artboard->height());
  std::unique_ptr<Scene> scene = artboard->defaultStateMachine();
  if (scene == nullptr && artboard->stateMachineCount() > 0)
  {
    scene = artboard->stateMachineAt(0);
  }
  if (scene == nullptr)
  {
    scene = artboard->animationAt(0);
  }
  auto vmi = file->createDefaultViewModelInstance(artboard.get());
  if (vmi != nullptr)
  {
    artboard->bindViewModelInstance(vmi);
    if (scene != nullptr)
    {
      scene->bindViewModelInstance(vmi);
    }
  }
  auto renderer = silver.makeRenderer();
  for (int i = 0; i < frames; i++)
  {
    if (i > 0)
    {
      silver.addFrame();
    }
    if (scene != nullptr)
    {
      scene->advanceAndApply(i == 0 ? 0.0f : 1.0f / 30.0f);
    }
    else
    {
      artboard->advance(i == 0 ? 0.0f : 1.0f / 30.0f);
    }
    artboard->draw(renderer.get());
  }
}

} // namespace lazy_decode_test

#endif
