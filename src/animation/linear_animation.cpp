#include "rive/animation/linear_animation.hpp"
#include "rive/animation/keyed_object.hpp"
#include "rive/animation/keyed_property.hpp"
#include "rive/animation/keyframe.hpp"
#include "rive/animation/keyed_callback_reporter.hpp"
#include "rive/artboard.hpp"
#include "rive/importers/artboard_importer.hpp"
#include "rive/importers/import_stack.hpp"
#include "rive/math/math_types.hpp"
#include <algorithm>
#include <cmath>

using namespace rive;

#ifdef TESTING
int LinearAnimation::deleteCount = 0;
#endif

LinearAnimation::LinearAnimation() {}

LinearAnimation::~LinearAnimation()
{
#ifdef TESTING
  deleteCount++;
#endif
}

StatusCode LinearAnimation::onAddedDirty(CoreContext* context)
{
  StatusCode status = StatusCode::Ok;
  std::vector<size_t> failedKeyedObjects;
  for (size_t i = 0; i < m_KeyedObjects.size(); ++i)
  {
    StatusCode code = m_KeyedObjects[i]->onAddedDirty(context);
    if (code != StatusCode::Ok)
    {
      failedKeyedObjects.push_back(i);
      if (status == StatusCode::Ok || status == StatusCode::MissingObject)
      {
        status = code;
      }
    }
  }

  // Failed keyed objects should not be applied later in animation playback.
  for (auto it = failedKeyedObjects.rbegin(); it != failedKeyedObjects.rend();
       ++it)
  {
    m_KeyedObjects.erase(m_KeyedObjects.begin() + *it);
  }

#ifdef WITH_RIVE_EDITOR
  // Coop-hydrated KeyedObjects run their own onAddedDirty in the
  // dispatcher's Pass 3 (editor_native/command_dispatcher.cpp); this
  // loop is redundant for arena-owned entries but cheap (vector is
  // empty on .riv-loaded animations).
  for (auto* object : m_EditorKeyedObjects)
  {
    object->onAddedDirty(context);
  }
#endif

  // Missing keyed objects are non-fatal at artboard init time.
  return status == StatusCode::MissingObject ? StatusCode::Ok : status;
}

StatusCode LinearAnimation::onAddedClean(CoreContext* context)
{
  StatusCode code;
  for (const auto& object : m_KeyedObjects)
  {
    if ((code = object->onAddedClean(context)) != StatusCode::Ok)
    {
      return code;
    }
  }
#ifdef WITH_RIVE_EDITOR
  for (auto* object : m_EditorKeyedObjects)
  {
    object->onAddedClean(context);
  }
#else
  // The keyed objects and their keyframes are final now (onAddedDirty dropped
  // the ones that failed). The editor edits keyframes after this point, so it
  // keeps the defaults, which only ever hold at time 0.
  uint32_t first = UINT32_MAX;
  uint32_t last = 0;
  for (const auto& object : m_KeyedObjects)
  {
    for (size_t i = 0; i < object->numKeyedProperties(); i++)
    {
      const KeyedProperty* property = object->getProperty(i);
      size_t count = property->numKeyFrames();
      if (count < 2)
      {
        continue;
      }
      first = std::min(first, property->getKeyFrame(0)->frame());
      last = std::max(last, property->getKeyFrame(count - 1)->frame());
    }
  }
  m_isStaticPose = !m_KeyedObjects.empty() && first == UINT32_MAX;
  if (first != UINT32_MAX)
  {
    m_firstKeyFrame = (uint16_t)std::min<uint32_t>(first, UINT16_MAX);
    m_lastKeyFrame = last < kNoLastKeyFrame ? (uint16_t)last : kNoLastKeyFrame;
  }
#endif
  return StatusCode::Ok;
}

bool LinearAnimation::appliesKeyFrameValuesAt(float seconds) const
{
  if (m_isStaticPose)
  {
    return true;
  }
  // The same time apply() evaluates, and the same seconds the keyframes were
  // given (KeyFrame::computeSeconds), so the comparisons match
  // KeyedProperty::apply's choice of keyframe exactly.
  float ffps = (float)fps();
  if (quantize())
  {
    seconds = std::floor(seconds * ffps) / ffps;
  }
  return seconds <= m_firstKeyFrame / ffps ||
         (m_lastKeyFrame != kNoLastKeyFrame &&
          seconds >= m_lastKeyFrame / ffps);
}

void LinearAnimation::addKeyedObject(std::unique_ptr<KeyedObject> object)
{
  m_KeyedObjects.push_back(std::move(object));
}

// `addKeyedObjectForEditor` and `clearEditorKeyedObjects` live in
// `editor_native/native/src/editor/animation/linear_animation_editor.cpp`
// — see the matching comment in `keyed_property.cpp`.

void LinearAnimation::apply(
    Artboard* artboard,
    float time,
    float mix,
    const LinearAnimationInstance* context) const
{
  if (quantize())
  {
    float ffps = (float)fps();
    time = std::floor(time * ffps) / ffps;
  }
  for (const auto& object : m_KeyedObjects)
  {
    object->apply(artboard, time, mix, context);
  }
#ifdef WITH_RIVE_EDITOR
  for (auto* object : m_EditorKeyedObjects)
  {
    object->apply(artboard, time, mix, context);
  }
#endif
}

StatusCode LinearAnimation::import(ImportStack& importStack)
{
  auto artboardImporter = importStack.latest<ArtboardImporter>(
      ArtboardBase::typeKey);
  if (artboardImporter == nullptr)
  {
    return StatusCode::MissingObject;
  }
  artboardImporter->addAnimation(this);
  return Super::import(importStack);
}

float LinearAnimation::startSeconds() const
{
  return (enableWorkArea() ? (float)workStart() : 0.0f) / (float)fps();
}
float LinearAnimation::endSeconds() const
{
  return (float)(enableWorkArea() ? workEnd() : duration()) / (float)fps();
}

float LinearAnimation::startTime() const
{
  return (speed() >= 0) ? startSeconds() : endSeconds();
}
float LinearAnimation::startTime(float multiplier) const
{
  return ((speed() * multiplier) >= 0) ? startSeconds() : endSeconds();
}
float LinearAnimation::endTime() const
{
  return (speed() >= 0) ? endSeconds() : startSeconds();
}
float LinearAnimation::durationSeconds() const
{
  return std::abs(endSeconds() - startSeconds());
}

// Matches Dart modulus:
// https://api.dart.dev/stable/2.19.0/dart-core/double/operator_modulo.html
static float positiveMod(float value, float range)
{
  assert(range > 0.0f);
  return math::positive_mod(value, range);
}

float LinearAnimation::globalToLocalSeconds(float seconds) const
{
  switch (loop())
  {
    case Loop::oneShot:
      return seconds + startTime();
    case Loop::loop:
      return positiveMod(seconds, (durationSeconds())) + startTime();
    case Loop::pingPong:
      float localTime = positiveMod(seconds, (durationSeconds()));
      int direction = ((int)(seconds / (durationSeconds()))) % 2;
      return direction == 0 ? localTime + startTime() : endTime() - localTime;
  }
  RIVE_UNREACHABLE();
}

void LinearAnimation::reportKeyedCallbacks(
    KeyedCallbackReporter* reporter,
    float secondsFrom,
    float secondsTo,
    float speedDirection,
    bool fromPong) const
{
#ifndef WITH_RIVE_EDITOR
  if (!m_hasKeyedCallbacks)
  {
    return;
  }
#endif
  float startingTime = startTime(speedDirection);
  bool isAtStartFrame = startingTime == secondsFrom;

  if (!isAtStartFrame || !fromPong)
  {
    for (const auto& object : m_KeyedObjects)
    {
      object->reportKeyedCallbacks(
          reporter,
          secondsFrom,
          secondsTo,
          isAtStartFrame);
    }
#ifdef WITH_RIVE_EDITOR
    for (auto* object : m_EditorKeyedObjects)
    {
      object->reportKeyedCallbacks(
          reporter,
          secondsFrom,
          secondsTo,
          isAtStartFrame);
    }
#endif
  }
}