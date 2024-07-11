#include "rive/shapes/paint/trim_path.hpp"
#include "rive/shapes/paint/stroke.hpp"
#include "rive/factory.hpp"

using namespace rive;

StatusCode TrimPath::onAddedClean(CoreContext* context)
{
    if (!parent()->is<Stroke>())
    {
        return StatusCode::InvalidObject;
    }

    parent()->as<Stroke>()->addStrokeEffect(this);

    return StatusCode::Ok;
}

void TrimPath::trimRawPath(const RawPath& source)
{
    m_rawPath.rewind();
    auto renderOffset = std::fmod(std::fmod(offset(), 1.0f) + 1.0f, 1.0f);

    // Build up contours if they're empty.
    if (m_contours.empty())
    {
        ContourMeasureIter iter(&source);
        while (auto meas = iter.next())
        {
            m_contours.push_back(meas);
        }
    }
    switch (mode())
    {
        case TrimPathMode::sequential:
        {
            float totalLength = 0.0f;
            for (auto contour : m_contours)
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

            int i = 0, subPathCount = (int)m_contours.size();
            while (endLength > 0)
            {
                auto contour = m_contours[i % subPathCount];
                auto contourLength = contour->length();

                if (startLength < contourLength)
                {
                    contour->getSegment(startLength, endLength, &m_rawPath, true);
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
        }
        break;

        case TrimPathMode::synchronized:
        {
            for (auto contour : m_contours)
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

                if (startLength > contourLength)
                {
                    startLength -= contourLength;
                    endLength -= contourLength;
                }
                contour->getSegment(startLength, endLength, &m_rawPath, true);
                while (endLength > contourLength)
                {
                    startLength = 0;
                    endLength -= contourLength;
                    contour->getSegment(startLength, endLength, &m_rawPath, true);
                }
            }
        }
        break;
        default:
            RIVE_UNREACHABLE();
    }
}

RenderPath* TrimPath::effectPath(const RawPath& source, Factory* factory)
{
    if (m_renderPath != nullptr)
    {
        // Previous result hasn't been invalidated, it's still good.
        return m_renderPath;
    }

    trimRawPath(source);

    if (!m_trimmedPath)
    {
        m_trimmedPath = factory->makeEmptyRenderPath();
    }
    else
    {
        m_trimmedPath->rewind();
    }

    m_renderPath = m_trimmedPath.get();
    m_rawPath.addTo(m_renderPath);
    return m_renderPath;
}

void TrimPath::invalidateEffect()
{
    invalidateTrim();
    // This is usually sent when the path is changed so we need to also
    // invalidate the contours, not just the trim effect.
    m_contours.clear();
}

void TrimPath::invalidateTrim()
{
    m_renderPath = nullptr;
    if (parent() != nullptr)
    {
        auto stroke = parent()->as<Stroke>();
        stroke->parent()->addDirt(ComponentDirt::Paint);
        stroke->invalidateRendering();
    }
}

void TrimPath::startChanged() { invalidateTrim(); }
void TrimPath::endChanged() { invalidateTrim(); }
void TrimPath::offsetChanged() { invalidateTrim(); }
void TrimPath::modeValueChanged() { invalidateTrim(); }

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