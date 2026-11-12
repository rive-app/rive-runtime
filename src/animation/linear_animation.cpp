#include "rive/animation/linear_animation.hpp"
#include "rive/animation/blend_accumulator.hpp"
#include "rive/animation/deferred_animation_body.hpp"
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/animation/keyed_object.hpp"
#include "rive/animation/keyed_property.hpp"
#include "rive/animation/keyframe.hpp"
#include "rive/animation/keyed_callback_reporter.hpp"
#include "rive/artboard.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/deferred_decode_lock.hpp"
#include "rive/file.hpp"
#include "rive/importers/artboard_importer.hpp"
#include "rive/importers/import_stack.hpp"
#include "rive/math/math_types.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <unordered_set>

using namespace rive;

#ifdef TESTING
int LinearAnimation::deleteCount = 0;
std::atomic<int> LinearAnimation::deferredDecodeCount{0};
#endif

LinearAnimation::LinearAnimation()
    : m_hasKeyedCallbacks(true)
    , m_isStaticPose(false)
    , m_bodyWasDeferred(false)
{}

LinearAnimation::~LinearAnimation()
{
  delete m_deferredBody.load(std::memory_order_relaxed);
  // Before the keyframes they target go. Source binds are never bound, so
  // they don't touch their targets as they go.
  if (m_keyFrameBinds != nullptr)
  {
    for (auto* bind : m_keyFrameBinds->binds)
    {
      delete bind;
    }
    for (auto* bind : m_keyFrameBinds->retired)
    {
      delete bind;
    }
  }
#ifdef TESTING
  deleteCount++;
#endif
}

void LinearAnimation::deferBody(DeferredAnimationBody* body)
{
  assert(m_deferredBody.load(std::memory_order_relaxed) == nullptr);
  m_bodyWasDeferred = true;
  m_deferredBody.store(body, std::memory_order_release);
}

size_t LinearAnimation::deferredBodySize() const
{
  const DeferredAnimationBody* body = m_deferredBody.load(
      std::memory_order_acquire);
  return body == nullptr ? 0 : body->bytes.size();
}

void LinearAnimation::addKeyFrameBind(DataBind* bind)
{
  if (m_keyFrameBinds == nullptr)
  {
    m_keyFrameBinds = std::make_unique<KeyFrameBinds>();
  }
  m_keyFrameBinds->binds.push_back(bind);
  // A keyframe holds a single value, so the first bind on it wins.
  m_keyFrameBinds->byKeyFrame.emplace(bind->target()->as<KeyFrame>(), bind);
  // An eager import sets it here; a deferred body's skim set it already,
  // and other threads may be reading it now.
  if (!m_hasKeyFrameBinds)
  {
    m_hasKeyFrameBinds = true;
  }
}

DataBind* LinearAnimation::keyFrameBind(const KeyFrame* keyframe) const
{
  if (!m_hasKeyFrameBinds)
  {
    return nullptr;
  }
  ensureDecoded();
  if (m_keyFrameBinds == nullptr)
  {
    return nullptr;
  }
  auto itr = m_keyFrameBinds->byKeyFrame.find(keyframe);
  return itr != m_keyFrameBinds->byKeyFrame.end() ? itr->second : nullptr;
}

const std::vector<DataBind*>& LinearAnimation::keyFrameBinds() const
{
  static const std::vector<DataBind*> none;
  if (!m_hasKeyFrameBinds)
  {
    return none;
  }
  ensureDecoded();
  return m_keyFrameBinds != nullptr ? m_keyFrameBinds->binds : none;
}

void LinearAnimation::retireStrandedKeyFrameBinds()
{
  if (m_keyFrameBinds == nullptr)
  {
    return;
  }
  // The keyframes that survived validation (onAddedDirty drops keyed
  // objects and properties that don't apply). A bind's target is only
  // compared, never followed: it may be gone.
  std::unordered_set<const Core*> keyframes;
  for (const auto& object : m_KeyedObjects)
  {
    for (size_t i = 0; i < object->numKeyedProperties(); i++)
    {
      const KeyedProperty* property = object->getProperty(i);
      for (size_t j = 0; j < property->numKeyFrames(); j++)
      {
        keyframes.insert(property->getKeyFrame(j));
      }
    }
  }
  auto& binds = m_keyFrameBinds->binds;
  size_t kept = 0;
  for (auto* bind : binds)
  {
    if (keyframes.count(bind->target()) != 0)
    {
      binds[kept++] = bind;
    }
    else
    {
      m_keyFrameBinds->retired.push_back(bind);
    }
  }
  binds.resize(kept);
  m_keyFrameBinds->byKeyFrame.clear();
  for (auto* bind : binds)
  {
    m_keyFrameBinds->byKeyFrame.emplace(
        static_cast<const KeyFrame*>(bind->target()),
        bind);
  }
}

// Set while this thread decodes an animation's body, which never needs
// another animation decoded.
static thread_local bool t_decodingAnimation = false;

void LinearAnimation::decodeDeferredBody() const
{
  assert(!t_decodingAnimation);
  std::lock_guard<std::recursive_mutex> lock(deferredDecodeMutex());
  // Another thread may have decoded it while this one waited for the lock.
  DeferredAnimationBody* body = m_deferredBody.load(std::memory_order_acquire);
  if (body == nullptr)
  {
    return;
  }
  // Shared animations are logically const: decoding only fills in what the
  // import skipped. Nothing below may call a guarded accessor: the lock is
  // recursive, so it would start decoding the body again.
  auto animation = const_cast<LinearAnimation*>(this);
  t_decodingAnimation = true;

  // The exporter writes an animation's keyed objects right after it, so
  // nothing attaches to a deferred animation before it is decoded. Should a
  // stray record (a damaged file) have attached one later, it still follows
  // the body, as it would in an eager import.
  std::vector<std::unique_ptr<KeyedObject>> attached = std::move(
      animation->m_KeyedObjects);
  animation->m_KeyedObjects.clear();

  // Read with the same objects and importers the import uses. The skim
  // admitted only keyed objects, keyed properties, keyframes and the
  // keyframes' binds, all of which this runtime knows.
  if (!body->file->decodeAnimationBody(animation, *body))
  {
    // The skim read these same bytes without error, so this means a bug
    // rather than a bad file. Play nothing rather than something partial.
    fprintf(
        stderr,
        "LinearAnimation: couldn't decode the deferred body of %s.\n",
        name().c_str());
    animation->m_KeyedObjects.clear();
  }
  for (auto& object : attached)
  {
    animation->m_KeyedObjects.push_back(std::move(object));
  }

  // What the artboard's initialize does for an animation it imported whole.
  // Only missing objects can fail here, and those just drop their keyed
  // object, as they would at import.
  animation->keyedObjectsAddedDirty(body->artboard);
  animation->keyedObjectsAddedClean(body->artboard);
  t_decodingAnimation = false;

#ifdef TESTING
  deferredDecodeCount++;
#endif
  m_deferredBody.store(nullptr, std::memory_order_release);
  delete body;
}

void LinearAnimation::forEachKeyedProperty(
    const std::function<void(uint32_t objectId, uint32_t propertyKey)>& visit)
    const
{
  if (hasDeferredBody())
  {
    // Read where it lies, under the lock its decode takes, so it isn't freed
    // meanwhile.
    std::unique_lock<std::recursive_mutex> lock(deferredDecodeMutex());
    DeferredAnimationBody* body = m_deferredBody.load(
        std::memory_order_acquire);
    // Keyed objects attached after the body (only a damaged file has them)
    // aren't validated until the decode, so those files decode.
    if (body != nullptr && m_KeyedObjects.empty())
    {
      forEachKeptKeyedProperty(*body, body->file->m_header, visit);
      return;
    }
    lock.unlock();
    ensureDecoded();
  }
  for (const auto& object : m_KeyedObjects)
  {
    for (size_t i = 0; i < object->numKeyedProperties(); i++)
    {
      const KeyedProperty* property = object->getProperty(i);
      if (property->numKeyFrames() != 0)
      {
        visit(object->objectId(), property->propertyKey());
      }
    }
  }
}

bool LinearAnimation::anyKeyFrameBindConverter(
    const std::function<bool(DataConverter*)>& test) const
{
  if (!m_hasKeyFrameBinds)
  {
    return false;
  }
  if (hasDeferredBody())
  {
    // Read where it lies, as forEachKeyedProperty does. The binds name the
    // file's converters, complete since the import, by index.
    std::unique_lock<std::recursive_mutex> lock(deferredDecodeMutex());
    DeferredAnimationBody* body = m_deferredBody.load(
        std::memory_order_acquire);
    if (body != nullptr && m_KeyedObjects.empty())
    {
      const auto& converters = body->file->m_DataConverters;
      bool found = false;
      forEachKeptKeyedProperty(
          *body,
          body->file->m_header,
          [](uint32_t, uint32_t) {},
          [&](uint32_t converterId) {
            DataConverter* converter = converterId < converters.size()
                                           ? converters[converterId]
                                           : nullptr;
            found = found || test(converter);
          });
      return found;
    }
    lock.unlock();
    ensureDecoded();
  }
  if (m_keyFrameBinds != nullptr)
  {
    for (DataBind* bind : m_keyFrameBinds->binds)
    {
      if (test(bind->converter()))
      {
        return true;
      }
    }
  }
  return false;
}

StatusCode LinearAnimation::onAddedDirty(CoreContext* context)
{
  // A deferred body is initialized as it's decoded (decodeDeferredBody),
  // which may already have happened (a Joystick reads its animations while
  // the artboard initializes).
  if (m_bodyWasDeferred)
  {
    return StatusCode::Ok;
  }
  return keyedObjectsAddedDirty(context);
}

StatusCode LinearAnimation::keyedObjectsAddedDirty(CoreContext* context)
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

  // Binds on keyframes dropped above can't play; the import may still hold
  // them, so they're retired rather than freed.
  retireStrandedKeyFrameBinds();

  // Missing keyed objects are non-fatal at artboard init time.
  return status == StatusCode::MissingObject ? StatusCode::Ok : status;
}

StatusCode LinearAnimation::onAddedClean(CoreContext* context)
{
  // See onAddedDirty.
  if (m_bodyWasDeferred)
  {
    return StatusCode::Ok;
  }
  return keyedObjectsAddedClean(context);
}

StatusCode LinearAnimation::keyedObjectsAddedClean(CoreContext* context)
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
  ensureDecoded();
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
  ensureDecoded();
  if (quantize())
  {
    float ffps = (float)fps();
    time = std::floor(time * ffps) / ffps;
  }
  // A state machine's accumulator only handles some objects; the rest are
  // written straight to, without asking it.
  BlendAccumulator* accumulator = context != nullptr
                                      ? context->blendAccumulator()
                                      : nullptr;
  const std::vector<bool>* tracked = accumulator != nullptr
                                         ? accumulator->trackedObjects(this)
                                         : nullptr;
  if (tracked == nullptr)
  {
    for (const auto& object : m_KeyedObjects)
    {
      object->apply(artboard, time, mix, context);
    }
  }
  else
  {
    for (size_t i = 0, count = m_KeyedObjects.size(); i < count; i++)
    {
      if ((*tracked)[i])
      {
        m_KeyedObjects[i]->apply(artboard, time, mix, context);
      }
      else
      {
        context->suspendAccumulator(true);
        m_KeyedObjects[i]->apply(artboard, time, mix, context);
        context->suspendAccumulator(false);
      }
    }
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
  ensureDecoded();
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