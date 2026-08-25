#ifndef _RIVE_KEYED_PROPERTY_HPP_
#define _RIVE_KEYED_PROPERTY_HPP_
#include "rive/generated/animation/keyed_property_base.hpp"
#include <vector>
namespace rive
{
class KeyFrame;
class KeyedCallbackReporter;
class LinearAnimationInstance;
class KeyedProperty : public KeyedPropertyBase
{
public:
    KeyedProperty();
    ~KeyedProperty() override;
    void addKeyFrame(std::unique_ptr<KeyFrame>);
    StatusCode onAddedClean(CoreContext* context) override;
    StatusCode onAddedDirty(CoreContext* context) override;

    /// Report any keyframes that occured between secondsFrom and secondsTo.
    void reportKeyedCallbacks(KeyedCallbackReporter* reporter,
                              uint32_t objectId,
                              float secondsFrom,
                              float secondsTo,
                              bool isAtStartFrame) const;

    /// Apply interpolating key frames. `context` is the running
    /// LinearAnimationInstance, propagated so scripted interpolators can vend
    /// per-(LAI, keyframe) stateful clones. Default-null keeps direct callers
    /// (tests, hold animations) source-compatible.
    void apply(Core* object,
               float time,
               float mix,
               const LinearAnimationInstance* context = nullptr);

    StatusCode import(ImportStack& importStack) override;
    KeyFrame* first() const
    {
        if (m_keyFrames.size() > 0)
        {
            return m_keyFrames.front().get();
        }
#ifdef WITH_RIVE_EDITOR
        if (!m_editorKeyFrames.empty())
        {
            return m_editorKeyFrames.front();
        }
#endif
        return nullptr;
    }

    size_t numKeyFrames() const { return m_keyFrames.size(); }
    KeyFrame* getKeyFrame(size_t index) const
    {
        return index < m_keyFrames.size() ? m_keyFrames[index].get() : nullptr;
    }

#ifdef WITH_RIVE_EDITOR
    // Editor-only: parallel non-owning list populated by
    // `EditorFile::finalizeBatch` from coop-hydrated KeyFrames whose
    // `keyedPropertyId` resolves to this property. See
    // `LinearAnimation::m_EditorKeyedObjects` for the dual-mode
    // rationale. Coop delivery order is not guaranteed to be time-
    // sorted; `finalizeBatch` calls `sortEditorKeyFrames` after it
    // finishes populating so `editorClosestFrameIndex`'s binary
    // search is valid.
    void addKeyFrameForEditor(KeyFrame* frame);
    void clearEditorKeyFrames();
    void sortEditorKeyFrames();
    /// Editor-only: remove [frame] from `m_editorKeyFrames`. Called
    /// from `EditorFile::removeObject` when a KeyFrame Core is hard-
    /// deleted so the apply-pass's binary search doesn't dereference
    /// a freed slot. No-op if the kf isn't in the list (idempotent).
    void removeKeyFrameForEditor(KeyFrame* frame);
    /// Read-only view for the timeline FFI. Order matches the most
    /// recent `sortEditorKeyFrames` call (ascending by frame).
    const std::vector<KeyFrame*>& editorKeyFrames() const
    {
        return m_editorKeyFrames;
    }
#endif

private:
    int closestFrameIndex(float seconds, int exactOffset = 0) const;
    std::vector<std::unique_ptr<KeyFrame>> m_keyFrames;

#ifdef WITH_RIVE_EDITOR
    // Editor-only non-owning parallel list. See method declarations
    // above for populate/sort/lifecycle notes.
    std::vector<KeyFrame*> m_editorKeyFrames;
    int editorClosestFrameIndex(float seconds, int exactOffset = 0) const;
#endif
};
} // namespace rive

#endif