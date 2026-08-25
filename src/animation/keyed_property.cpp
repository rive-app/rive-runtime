#include "rive/animation/keyed_property.hpp"
#include "rive/animation/keyed_object.hpp"
#include "rive/animation/keyframe.hpp"
#include "rive/animation/keyframe_interpolator.hpp"
#include "rive/animation/interpolating_keyframe.hpp"
#include "rive/animation/keyed_callback_reporter.hpp"
#include "rive/importers/import_stack.hpp"
#include "rive/importers/keyed_object_importer.hpp"
#include <algorithm>

using namespace rive;

KeyedProperty::KeyedProperty() {}
KeyedProperty::~KeyedProperty() {}

void KeyedProperty::addKeyFrame(std::unique_ptr<KeyFrame> keyframe)
{
    m_keyFrames.push_back(std::move(keyframe));
}

// `addKeyFrameForEditor`, `clearEditorKeyFrames`, `sortEditorKeyFrames`
// and `editorClosestFrameIndex` live in the editor_native package
// (`src/editor/animation/keyed_property_editor.cpp`) so the runtime-
// only build never compiles or links them. The declarations stay on
// `KeyedProperty` (under `#ifdef WITH_RIVE_EDITOR` in the header)
// because `apply()` below still uses `m_editorKeyFrames` directly when
// the editor list is populated — the runtime build just sees an empty
// vector and falls through to the runtime-owned `m_keyFrames`.

int KeyedProperty::closestFrameIndex(float seconds, int exactOffset) const
{
    int mid = 0;
    float closestSeconds = 0;
    int start = 0;
    auto numKeyFrames = static_cast<int>(m_keyFrames.size());
    int end = numKeyFrames - 1;

    // If it's the last keyframe, we skip the binary search
    if (seconds > m_keyFrames[end]->seconds())
    {
        return end + 1;
    }

    while (start <= end)
    {
        mid = (start + end) >> 1;
        closestSeconds = m_keyFrames[mid]->seconds();
        if (closestSeconds < seconds)
        {
            start = mid + 1;
        }
        else if (closestSeconds > seconds)
        {
            end = mid - 1;
        }
        else
        {
            return mid + exactOffset;
        }
    }
    return start;
}

void KeyedProperty::reportKeyedCallbacks(KeyedCallbackReporter* reporter,
                                         uint32_t objectId,
                                         float secondsFrom,
                                         float secondsTo,
                                         bool isAtStartFrame) const
{
    if (secondsFrom == secondsTo)
    {
        return;
    }
    bool isForward = secondsFrom <= secondsTo;
    int fromExactOffset = 0;
    int toExactOffset = isForward ? 1 : 0;
    if (isForward)
    {
        if (!isAtStartFrame)
        {
            fromExactOffset = 1;
        }
    }
    else
    {
        if (isAtStartFrame)
        {
            fromExactOffset = 1;
        }
    }
    int idx = closestFrameIndex(secondsFrom, fromExactOffset);
    int idxTo = closestFrameIndex(secondsTo, toExactOffset);

    if (idxTo < idx)
    {
        auto swap = idx;
        idx = idxTo;
        idxTo = swap;
    }
    while (idxTo > idx)
    {
        const std::unique_ptr<KeyFrame>& frame = m_keyFrames[idx];
        reporter->reportKeyedCallback(objectId,
                                      propertyKey(),
                                      secondsTo - frame->seconds());
        idx++;
    }
}

void KeyedProperty::apply(Core* object,
                          float seconds,
                          float mix,
                          const LinearAnimationInstance* context)
{
#ifdef WITH_RIVE_EDITOR
    // A given KeyedProperty instance is either runtime-loaded (uses
    // m_keyFrames unique_ptr owned) or coop-loaded (uses
    // m_editorKeyFrames non-owning). Pick the populated one; the
    // binary-search / interpolate logic below operates on whichever
    // side holds the data. Both-populated can't happen in practice
    // — a single property either lives in the importer chain or in
    // the arena, never both.
    const bool useEditor = m_keyFrames.empty() && !m_editorKeyFrames.empty();
    const size_t frameCount =
        useEditor ? m_editorKeyFrames.size() : m_keyFrames.size();
    // Editor edge case: the user just deleted every kf in this
    // property (e.g. via kDeleteKeyFrameSelection). The KeyedProperty
    // Core itself stays alive — it's only orphaned when its parent
    // KeyedObject gets deleted — but its editor list is empty, and
    // the binary search below would read `m_editorKeyFrames[-1]`.
    // Skip with no overlay so the property reverts to its design-
    // time value on the next frame.
    if (frameCount == 0)
        return;
    auto frameAt = [this, useEditor](int i) -> InterpolatingKeyFrame* {
        return static_cast<InterpolatingKeyFrame*>(
            useEditor ? m_editorKeyFrames[i] : m_keyFrames[i].get());
    };
    const int closestIdx = useEditor ? editorClosestFrameIndex(seconds)
                                     : closestFrameIndex(seconds);
#else
    assert(!m_keyFrames.empty());
    const size_t frameCount = m_keyFrames.size();
    auto frameAt = [this](int i) -> InterpolatingKeyFrame* {
        return static_cast<InterpolatingKeyFrame*>(m_keyFrames[i].get());
    };
    const int closestIdx = closestFrameIndex(seconds);
#endif

    auto interpolatorHost = InterpolatorHost::from(object);
    auto actualMix = mix;
    if (interpolatorHost != nullptr &&
        interpolatorHost->overridesKeyedInterpolation(propertyKey()))
    {
        actualMix = 1.0f;
    }

    int idx = closestIdx;
    int pk = propertyKey();

    if (idx == 0)
    {
        frameAt(0)->apply(object, pk, actualMix, context);
    }
    else
    {
        if (idx < static_cast<int>(frameCount))
        {
            InterpolatingKeyFrame* fromFrame = frameAt(idx - 1);
            InterpolatingKeyFrame* toFrame = frameAt(idx);
            if (seconds == toFrame->seconds())
            {
                toFrame->apply(object, pk, actualMix, context);
            }
            else
            {
                if (fromFrame->interpolationType() == 0)
                {
                    fromFrame->apply(object, pk, actualMix, context);
                }
                else
                {
                    fromFrame->applyInterpolation(object,
                                                  pk,
                                                  seconds,
                                                  toFrame,
                                                  actualMix,
                                                  context);
                }
            }
        }
        else
        {
            frameAt(idx - 1)->apply(object, pk, actualMix, context);
        }
    }
}

StatusCode KeyedProperty::onAddedDirty(CoreContext* context)
{
    StatusCode code;
    for (auto& keyframe : m_keyFrames)
    {
        if ((code = keyframe->onAddedDirty(context)) != StatusCode::Ok)
        {
            return code;
        }
    }
#ifdef WITH_RIVE_EDITOR
    for (auto* keyframe : m_editorKeyFrames)
    {
        keyframe->onAddedDirty(context);
    }
#endif
    return StatusCode::Ok;
}

StatusCode KeyedProperty::onAddedClean(CoreContext* context)
{
    StatusCode code;
    for (auto& keyframe : m_keyFrames)
    {
        if ((code = keyframe->onAddedClean(context)) != StatusCode::Ok)
        {
            return code;
        }
    }
#ifdef WITH_RIVE_EDITOR
    for (auto* keyframe : m_editorKeyFrames)
    {
        keyframe->onAddedClean(context);
    }
#endif
    return StatusCode::Ok;
}

StatusCode KeyedProperty::import(ImportStack& importStack)
{
    auto importer =
        importStack.latest<KeyedObjectImporter>(KeyedObjectBase::typeKey);
    if (importer == nullptr)
    {
        return StatusCode::MissingObject;
    }
    importer->addKeyedProperty(std::unique_ptr<KeyedProperty>(this));
    return Super::import(importStack);
}
