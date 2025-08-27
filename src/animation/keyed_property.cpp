#include "rive/animation/keyed_property.hpp"
#include "rive/animation/keyed_object.hpp"
#include "rive/animation/keyframe.hpp"
#include "rive/animation/keyframe_interpolator.hpp"
#include "rive/animation/interpolating_keyframe.hpp"
#include "rive/animation/keyed_callback_reporter.hpp"
#include "rive/importers/import_stack.hpp"
#include "rive/importers/keyed_object_importer.hpp"

using namespace rive;

KeyedProperty::KeyedProperty() {}
KeyedProperty::~KeyedProperty() {}

void KeyedProperty::addKeyFrame(std::unique_ptr<KeyFrame> keyframe)
{
    m_keyFrames.push_back(std::move(keyframe));
}

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

void KeyedProperty::apply(Core* object, float seconds, float mix)
{
    assert(!m_keyFrames.empty());

    auto interpolatorHost = InterpolatorHost::from(object);
    auto actualMix = mix;
    if (interpolatorHost != nullptr &&
        interpolatorHost->overridesKeyedInterpolation(propertyKey()))
    {
        actualMix = 1.0f;
    }

    int idx = closestFrameIndex(seconds);
    int pk = propertyKey();

    if (idx == 0)
    {
        static_cast<InterpolatingKeyFrame*>(m_keyFrames[0].get())
            ->apply(object, pk, actualMix);
    }
    else
    {
        if (idx < static_cast<int>(m_keyFrames.size()))
        {
            InterpolatingKeyFrame* fromFrame =
                static_cast<InterpolatingKeyFrame*>(m_keyFrames[idx - 1].get());
            InterpolatingKeyFrame* toFrame =
                static_cast<InterpolatingKeyFrame*>(m_keyFrames[idx].get());
            if (seconds == toFrame->seconds())
            {
                toFrame->apply(object, pk, actualMix);
            }
            else
            {
                if (fromFrame->interpolationType() == 0)
                {
                    fromFrame->apply(object, pk, actualMix);
                }
                else
                {
                    fromFrame->applyInterpolation(object,
                                                  pk,
                                                  seconds,
                                                  toFrame,
                                                  actualMix);
                }
            }
        }
        else
        {
            static_cast<InterpolatingKeyFrame*>(m_keyFrames[idx - 1].get())
                ->apply(object, pk, actualMix);
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
