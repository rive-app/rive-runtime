#include "rive/shapes/paint/dash_path.hpp"
#include "rive/shapes/paint/dash.hpp"
#include "rive/shapes/paint/stroke.hpp"
#include "rive/factory.hpp"

using namespace rive;

void PathDasher::invalidateSourcePath()
{
    m_contours.clear();
    invalidateDash();
}

void PathDasher::invalidateDash() { m_path.rewind(); }

ShapePaintPath* PathDasher::dash(const RawPath* source,
                                 Dash* offset,
                                 Span<Dash*> dashes)
{
    if (m_path.hasRenderPath())
    {
        // Previous result hasn't been invalidated, it's still good.
        return &m_path;
    }

    m_path.rewind();
    return applyDash(source, offset, dashes);
}
ShapePaintPath* PathDasher::applyDash(const RawPath* source,
                                      Dash* offset,
                                      Span<Dash*> dashes)
{
    if (m_contours.empty())
    {
        // 0.5f / 8.0f is a value that seems to look good on dashes with small
        // gaps and scaled
        ContourMeasureIter iter(source, 0.0625f);
        while (auto meas = iter.next())
        {
            m_contours.push_back(meas);
        }
    }

    // Make sure dashes have some length.
    bool hasValidDash = false;
    for (const rcp<ContourMeasure>& contour : m_contours)
    {
        for (auto dash : dashes)
        {
            if (dash->normalizedLength(contour->length()) > 0.0f)
            {
                hasValidDash = true;
                break;
            }
        }
        if (hasValidDash)
        {
            break;
        }
    }
    if (hasValidDash)
    {
        int dashIndex = 0;
        auto rawPath = m_path.mutableRawPath();
        for (const rcp<ContourMeasure>& contour : m_contours)
        {
            float dashed = 0.0f;
            float distance = offset->normalizedLength(contour->length());
            bool draw = true;
            if (dashLength <= 0.0f)
            {
                dashIndex++;
                continue;
            }
            while (dashed < contour->length())
            {
                const Dash* dash = dashes[dashIndex++ % dashes.size()];
                float dashLength = dash->normalizedLength(contour->length());
                if (dashLength > contour->length())
                {
                    dashLength = contour->length();
                }
                float endLength = distance + dashLength;
                if (endLength > contour->length())
                {
                    endLength -= contour->length();
                    if (draw)
                    {
                        if (distance < contour->length())
                        {
                            contour->getSegment(distance,
                                                contour->length(),
                                                rawPath,
                                                true);
                            contour->getSegment(0.0f,
                                                endLength,
                                                rawPath,
                                                !contour->isClosed());
                        }
                        else
                        {
                            contour->getSegment(0.0f, endLength, rawPath, true);
                        }
                    }

                    // Setup next step.
                    distance = endLength - dashLength;
                }
                else if (draw)
                {
                    contour->getSegment(distance, endLength, rawPath, true);
                }
                distance += dashLength;
                dashed += dashLength;
                draw = !draw;
            }
        }
    }
    return &m_path;
}

float PathDasher::pathLength() const
{
    float totalLength = 0.0f;
    for (auto contour : m_contours)
    {
        totalLength += contour->length();
    }
    return totalLength;
}

StatusCode DashPath::onAddedClean(CoreContext* context)
{
    if (!parent()->is<Stroke>())
    {
        return StatusCode::InvalidObject;
    }
    parent()->as<Stroke>()->addStrokeEffect(this);

    m_dashes.clear();
    for (auto child : children())
    {
        if (child->is<Dash>())
        {
            m_dashes.push_back(child->as<Dash>());
        }
    }
    return StatusCode::Ok;
}

void DashPath::invalidateEffect() { invalidateSourcePath(); }

void DashPath::offsetChanged() { invalidateDash(); }
void DashPath::offsetIsPercentageChanged() { invalidateDash(); }

void DashPath::updateEffect(const ShapePaintPath* source)
{

    if (m_path.hasRenderPath())
    {
        return;
    }
    m_path.rewind(source->isLocal());

    Dash dashOffset(offset(), offsetIsPercentage());
    applyDash(source->rawPath(), &dashOffset, m_dashes);
}

ShapePaintPath* DashPath::effectPath() { return &m_path; }

void DashPath::invalidateDash()
{
    PathDasher::invalidateDash();
    if (parent() != nullptr)
    {
        auto stroke = parent()->as<Stroke>();
        stroke->parent()->addDirt(ComponentDirt::Paint);
        stroke->invalidateRendering();
    }
}
