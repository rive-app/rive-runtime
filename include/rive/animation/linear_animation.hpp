#ifndef _RIVE_LINEAR_ANIMATION_HPP_
#define _RIVE_LINEAR_ANIMATION_HPP_
#include "rive/animation/loop.hpp"
#include "rive/generated/animation/linear_animation_base.hpp"
#include <vector>
namespace rive
{
class Artboard;
class KeyedObject;
class KeyedCallbackReporter;
class LinearAnimationInstance;

class LinearAnimation : public LinearAnimationBase
{
private:
  // Whether any keyed property is a callback (events, audio...), so
  // reportKeyedCallbacks has something to report. Import clears it when the
  // animation is read and each callback KeyedProperty sets it again; an
  // animation that isn't imported keeps scanning. Declared first so it lands
  // in the base's tail padding.
  bool m_hasKeyedCallbacks = true;
  // Every keyed property has a single keyframe, so applying it writes the
  // same values whatever the time. Set in onAddedClean.
  bool m_isStaticPose = false;
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

public:
  LinearAnimation();
  ~LinearAnimation() override;
  StatusCode onAddedDirty(CoreContext* context) override;
  StatusCode onAddedClean(CoreContext* context) override;
  void addKeyedObject(std::unique_ptr<KeyedObject>);
  void hasKeyedCallbacks(bool value) { m_hasKeyedCallbacks = value; }
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
    if (index < m_KeyedObjects.size())
    {
      return m_KeyedObjects[index].get();
    }
    else
    {
      return nullptr;
    }
  }

  size_t numKeyedObjects() const { return m_KeyedObjects.size(); }

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
