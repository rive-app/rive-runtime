#ifndef _RIVE_LINEAR_ANIMATION_HPP_
#define _RIVE_LINEAR_ANIMATION_HPP_
#include "rive/animation/loop.hpp"
#include "rive/generated/animation/linear_animation_base.hpp"
#include <atomic>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
namespace rive
{
class Artboard;
class DataBind;
class DataConverter;
class KeyFrame;
class KeyedObject;
class KeyedCallbackReporter;
class LinearAnimationInstance;
struct DeferredAnimationBody;

// The binds on an animation's keyframes, which it owns. Keyframes aren't
// Components: a bind on one is never applied as is, but cloned by each
// playing LinearAnimationInstance onto a holder of its own (see
// LinearAnimationInstance::keyFrameValueHolder).
struct KeyFrameBinds
{
  // In import order.
  std::vector<DataBind*> binds;
  // The first bind on each keyframe, as a keyframe holds a single value.
  std::unordered_map<const KeyFrame*, DataBind*> byKeyFrame;
  // Binds whose keyframes didn't survive validation. The import still
  // holds them until it resolves, so they're only freed with the animation.
  std::vector<DataBind*> retired;
};

class LinearAnimation : public LinearAnimationBase
{
private:
  // Whether any keyed property is a callback (events, audio...), so
  // reportKeyedCallbacks has something to report. Import clears it when the
  // animation is read and each callback KeyedProperty sets it again; an
  // animation that isn't imported keeps scanning. The flags are declared
  // first so they share one byte of the base's tail padding.
  bool m_hasKeyedCallbacks : 1;
  // Every keyed property has a single keyframe, so applying it writes the
  // same values whatever the time. Set in onAddedClean.
  bool m_isStaticPose : 1;
  // The import deferred the keyed body (FileImportOptions), so
  // decodeDeferredBody initializes it and the artboard's own
  // onAddedDirty/onAddedClean passes leave it alone.
  bool m_bodyWasDeferred : 1;
  // Whether any keyframe has a bind. Set as the animation imports (by
  // addKeyFrameBind, or from the skim of a body it defers) and never after,
  // so it can be read without decoding. A plain bool, not one more bit
  // above: a concurrent decode writes those, and bits share a byte. It
  // lands in padding.
  bool m_hasKeyFrameBinds = false;
  // The earliest first and latest last keyframe among the keyed properties
  // that have more than one, set in onAddedClean (a property with a single
  // keyframe writes it as it is at any time). Applied at or before the first,
  // or at or after the last, every property writes a keyframe's value as it
  // is and interpolates nothing, so the values hold for as long as the time
  // does -- a finished one-shot, or a pose. A state machine layer resting on
  // one stops re-applying it while nothing else changes (see
  // StateMachineLayerInstance::isFrozen). The first may only underestimate,
  // which stays correct; a last that doesn't fit is kNoLastKeyFrame, which
  // never holds.
  uint16_t m_firstKeyFrame = 0;
  uint16_t m_lastKeyFrame = kNoLastKeyFrame;
  static constexpr uint16_t kNoLastKeyFrame = UINT16_MAX;
  std::vector<std::unique_ptr<KeyedObject>> m_KeyedObjects;
  // The keyed body the import skipped, until it is decoded on first use
  // (ensureDecoded); null otherwise. Animations are shared by every instance
  // of their artboard, and two state machines of one file can advance on
  // different threads, so the first use can race: readers load it with
  // acquire, and decodeDeferredBody publishes the decoded body by storing
  // null with release, under deferredDecodeMutex().
  mutable std::atomic<DeferredAnimationBody*> m_deferredBody{nullptr};
  // Allocated with the first bind.
  std::unique_ptr<KeyFrameBinds> m_keyFrameBinds;
#ifdef WITH_RIVE_EDITOR
  // Non-owning parallel list populated by `EditorFile::finalizeBatch`
  // from coop-hydrated KeyedObjects whose `animationId` (an editor-
  // only `runtime: false` Id property) resolves to this animation.
  // Coop-delivered KeyedObjects live in the EditorFile arena and
  // don't fit the `unique_ptr` ownership contract used by the `.riv`
  // importer path — parallel storage keeps both loading modes
  // coexistent within a single editor binary: a runtime `.riv`
  // loaded side-by-side with a coop file uses `m_KeyedObjects`,
  // the coop-loaded one uses `m_EditorKeyedObjects`. Iteration
  // (apply / onAddedDirty / onAddedClean / reportKeyedCallbacks)
  // walks whichever is populated — for any given `LinearAnimation`
  // instance exactly one side is non-empty in practice.
  std::vector<KeyedObject*> m_EditorKeyedObjects;
#endif

  friend class Artboard;

  void decodeDeferredBody() const;
  StatusCode keyedObjectsAddedDirty(CoreContext* context);
  StatusCode keyedObjectsAddedClean(CoreContext* context);
  // Retires the binds whose keyframes are no longer in the keyed objects.
  void retireStrandedKeyFrameBinds();

public:
  LinearAnimation();
  ~LinearAnimation() override;
  StatusCode onAddedDirty(CoreContext* context) override;
  StatusCode onAddedClean(CoreContext* context) override;
  void addKeyedObject(std::unique_ptr<KeyedObject>);
  void hasKeyedCallbacks(bool value) { m_hasKeyedCallbacks = value; }

  /// Hands the animation the keyed body File::import skipped, to decode on
  /// first use. Takes ownership of [body].
  void deferBody(DeferredAnimationBody* body);

  /// Adds a bind on one of this animation's keyframes, as it imports. Takes
  /// ownership of [bind].
  void addKeyFrameBind(DataBind* bind);
  /// Whether any keyframe has a bind, without decoding a deferred body.
  bool hasKeyFrameBinds() const { return m_hasKeyFrameBinds; }
  /// Set by File::import for a body it defers, from what the skim saw.
  void hasKeyFrameBinds(bool value) { m_hasKeyFrameBinds = value; }
  /// The bind on [keyframe], or null.
  DataBind* keyFrameBind(const KeyFrame* keyframe) const;
  /// Every bind on this animation's keyframes.
  const std::vector<DataBind*>& keyFrameBinds() const;
  /// Decodes the keyed body if the import deferred it. Every accessor of
  /// keyed data calls this first, and so does LinearAnimationInstance's
  /// constructor, which is what normally decodes it. Safe on any thread.
  void ensureDecoded() const
  {
    if (m_deferredBody.load(std::memory_order_acquire) != nullptr)
    {
      decodeDeferredBody();
    }
  }
  /// Whether the keyed body is still waiting to be decoded.
  bool hasDeferredBody() const
  {
    return m_deferredBody.load(std::memory_order_acquire) != nullptr;
  }
  /// Calls [visit] with the object id and property key of each keyed
  /// property that has a keyframe, as the decoded body holds them, without
  /// decoding a deferred one (a state machine works out which properties its
  /// layers share before any of its states play). Safe on any thread.
  void forEachKeyedProperty(
      const std::function<void(uint32_t objectId, uint32_t propertyKey)>& visit)
      const;
  /// Whether [test] holds for the converter of any bind on this animation's
  /// keyframes, without decoding a deferred body (a state machine asks
  /// whether its artboard runs scripts before any of its states play). A
  /// pending bind's converter is the file's it names, which the decode
  /// clones for it. Safe on any thread.
  bool anyKeyFrameBindConverter(
      const std::function<bool(DataConverter*)>& test) const;
  /// How many bytes of undecoded body the animation holds. For diagnostics:
  /// don't call it while another thread may be decoding the animation.
  size_t deferredBodySize() const;
#ifdef TESTING
  /// How many deferred bodies have been decoded, by any animation.
  static std::atomic<int> deferredDecodeCount;
#endif
  /// Whether applying at `seconds` writes every keyed property's keyframe
  /// values as they are, interpolating nothing (see m_firstKeyFrame).
  bool appliesKeyFrameValuesAt(float seconds) const;
  /// `context` is the running LinearAnimationInstance, threaded down so
  /// scripted interpolators can vend per-(LAI, keyframe) stateful clones.
  /// Default-null keeps direct callers (state-machine hold animations,
  /// tests) source-compatible — they degrade to identity for any scripted
  /// interpolators in the snapshot.
  void apply(
      Artboard* artboard,
      float time,
      float mix = 1.0f,
      const LinearAnimationInstance* context = nullptr) const;
#ifdef WITH_RIVE_EDITOR
  // Editor-only: push a coop-hydrated KeyedObject onto
  // `m_EditorKeyedObjects`. See field comment for lifetime.
  void addKeyedObjectForEditor(KeyedObject* object);
  // Editor-only: drop editor-only entries so `finalizeBatch` can
  // rebuild idempotently on each coop batch.
  void clearEditorKeyedObjects();
  // Editor-only: how many coop-hydrated KeyedObjects this animation
  // carries. Used by diagnostics to distinguish "empty" animations
  // from real playback candidates.
  size_t editorKeyedObjectCount() const { return m_EditorKeyedObjects.size(); }
  /// Editor-only: read-only view of the coop-hydrated KeyedObjects.
  /// Used by editor_native's timeline FFI to build the row tree.
  /// Pointer lifetime matches the EditorFile arena — entries are
  /// repopulated on every `finalizeBatch`, so iterate within the
  /// drain that called it.
  const std::vector<KeyedObject*>& editorKeyedObjects() const
  {
    return m_EditorKeyedObjects;
  }
#endif

  Loop loop() const { return (Loop)loopValue(); }

  StatusCode import(ImportStack& importStack) override;

  float durationSeconds() const;
  /// Returns the start time/ end time of the animation in seconds
  float startSeconds() const;
  float endSeconds() const;

  /// Returns the start time/ end time of the animation in seconds,
  /// considering speed
  float startTime() const;
  float startTime(float multiplier) const;
  float endTime() const;

  /// Convert a global clock to local seconds (takes into consideration
  /// work area start/end, speed, looping).
  float globalToLocalSeconds(float seconds) const;

  // Returns a list of only the KeyedObjects that were validated during
  // onAddedDirty. This is not guaranteed to be the same as the list in the
  // exported riv.
  const KeyedObject* getObject(size_t index) const
  {
    ensureDecoded();
    if (index < m_KeyedObjects.size())
    {
      return m_KeyedObjects[index].get();
    }
    else
    {
      return nullptr;
    }
  }

  size_t numKeyedObjects() const
  {
    ensureDecoded();
    return m_KeyedObjects.size();
  }

#ifdef TESTING
  // Used in testing to check how many animations gets deleted.
  static int deleteCount;
#endif

  void reportKeyedCallbacks(
      KeyedCallbackReporter* reporter,
      float secondsFrom,
      float secondsTo,
      float speedDirection,
      bool fromPong) const;
};
} // namespace rive

#endif
