#include "rive/shapes/paint/trim_path.hpp"
#include "rive/shapes/paint/shape_paint.hpp"
#include "rive/profiler/profiler_macros.h"
using namespace rive;

void TrimEffectPath::invalidateEffect()
{
    m_path.rewind();
    m_contours.clear();
}

StatusCode TrimPath::onAddedClean(CoreContext* context)
{
    auto effectsContainer = EffectsContainer::from(parent());
    if (!effectsContainer)
    {
        return StatusCode::InvalidObject;
    }
    effectsContainer->addStrokeEffect(this);

    return StatusCode::Ok;
}

void TrimPath::trimPath(ShapePaintPath* destination,
                        std::vector<rcp<ContourMeasure>>& contours,
                        const RawPath* source,
                        ShapePaintType shapePaintType)
{
    RIVE_PROF_SCOPE()
    auto rawPath = destination->mutableRawPath();
    auto renderOffset = std::fmod(std::fmod(offset(), 1.0f) + 1.0f, 1.0f);
    auto closeShape = shapePaintType == ShapePaintType::fill;

    // Build up contours if they're empty.
    if (contours.empty())
    {
        ContourMeasureIter iter(source);
        while (auto meas = iter.next())
        {
            contours.push_back(meas);
        }
    }
    switch (mode())
    {
        case TrimPathMode::sequential:
        {
            float totalLength = 0.0f;
            for (auto contour : contours)
            {
                totalLength += contour->length();
            }
            auto startLength = totalLength * (start() + renderOffset);
            auto endLength = totalLength * (end() + renderOffset);

            if (endLength < startLength)
            {
                float swap = startLength;
                startLength = endLength;
                endLength = swap;
            }

            if (startLength > totalLength)
            {
                startLength -= totalLength;
                endLength -= totalLength;
            }

            int i = 0, subPathCount = (int)contours.size();
            std::vector<int> indices;
            std::vector<float> lengths;
            while (endLength > 0)
            {
                auto currentContourIndex = i % subPathCount;
                auto contour = contours[currentContourIndex];
                auto contourLength = contour->length();

                if (startLength < contourLength)
                {
                    indices.push_back(currentContourIndex);
                    lengths.push_back(startLength);
                    lengths.push_back(endLength);
                    endLength -= contourLength;
                    startLength = 0;
                }
                else
                {
                    startLength -= contourLength;
                    endLength -= contourLength;
                }
                i++;
            }

            // This is inintuitive but works. If the last segment and the first
            // segment of the trim path belong to the same contour, we want to
            // draw the first semgment first, then the last one and then the
            // rest. This is intentional to make sure the last segment is
            // connected to the first one in order to avoid a gap. A linear way
            // without reordering indices is to start at index 0 and go
            // backwards. For the remaining segments it's not important in what
            // order they are drawn
            int startingIndex = 0;
            int indexCount = 0;

            int prevContourIndex = -1;
            while (indexCount < indices.size())
            {
                auto index = (startingIndex < 0 ? startingIndex + indices.size()
                                                : startingIndex) %
                             indices.size();
                auto contourIndex = indices[index];
                auto contour = contours[contourIndex];
                auto contourLength = contour->length();
                auto lengthIndex = index * 2;
                auto startLength = lengths[lengthIndex];
                auto endLength = lengths[lengthIndex + 1];
                // if two consecutive segments belong to the same contour, draw
                // them connected.
                contour->getSegment(startLength,
                                    endLength,
                                    rawPath,
                                    prevContourIndex != contourIndex ||
                                        !contour->isClosed());
                // Close contours that are fully used as
                // segments
                if ((startLength == 0.0f &&
                     endLength - startLength >= contourLength &&
                     contour->isClosed()) ||
                    closeShape)
                {
                    rawPath->close();
                }
                prevContourIndex = contourIndex;
                indexCount++;
                startingIndex--;
            }
        }
        break;

        case TrimPathMode::synchronized:
        {
            for (auto contour : contours)
            {
                auto contourLength = contour->length();
                auto startLength = contourLength * (start() + renderOffset);
                auto endLength = contourLength * (end() + renderOffset);
                if (endLength < startLength)
                {
                    auto length = startLength;
                    startLength = endLength;
                    endLength = length;
                }

                if (startLength >= contourLength)
                {
                    startLength -= contourLength;
                    endLength -= contourLength;
                }
                contour->getSegment(startLength, endLength, rawPath, true);
                while (endLength > contourLength)
                {
                    startLength = 0;
                    endLength -= contourLength;
                    contour->getSegment(startLength,
                                        endLength,
                                        rawPath,
                                        !contour->isClosed());
                }

                if ((start() == 0.0f && end() == 1.0f && contour->isClosed()) ||
                    closeShape)
                {
                    rawPath->close();
                }
            }
        }
        break;
        default:
            RIVE_UNREACHABLE();
    }
}

void TrimPath::startChanged() { invalidateEffectFromLocal(); }
void TrimPath::endChanged() { invalidateEffectFromLocal(); }
void TrimPath::offsetChanged() { invalidateEffectFromLocal(); }
void TrimPath::modeValueChanged() { invalidateEffectFromLocal(); }

StatusCode TrimPath::onAddedDirty(CoreContext* context)
{
    auto code = Super::onAddedDirty(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }
    switch (mode())
    {
        case TrimPathMode::sequential:
        case TrimPathMode::synchronized:

            return StatusCode::Ok;
    }
    return StatusCode::InvalidObject;
}

void TrimPath::updateEffect(PathProvider* pathProvider,
                            const ShapePaintPath* source,
                            ShapePaintType shapePaintType)
{
    auto effectPathIt = m_effectPaths.find(pathProvider);
    if (effectPathIt != m_effectPaths.end())
    {
        auto trimEffectPath =
            static_cast<TrimEffectPath*>(effectPathIt->second);
        auto path = trimEffectPath->path();
        if (path->hasRenderPath())
        {
            // Previous result hasn't been invalidated, it's still good.
            return;
        }
        path->rewind(source->isLocal(), source->fillRule());
        trimPath(path,
                 trimEffectPath->contours(),
                 source->rawPath(),
                 shapePaintType);
    }
}

EffectsContainer* TrimPath::parentPaint()
{
    return EffectsContainer::from(parent());
}

EffectPath* TrimPath::createEffectPath() { return new TrimEffectPath(); }