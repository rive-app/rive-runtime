#include "rive/shapes/paint/dash_path.hpp"
#include "rive/factory.hpp"

using namespace rive;

Dash::Dash() : m_value(0.0f), m_percentage(false) {}
Dash::Dash(float value, bool percentage) : m_value(value), m_percentage(percentage) {}
float Dash::value() const { return m_value; }

float Dash::normalizedValue(float length) const
{
    float right = m_percentage ? 1.0f : length;
    float p = fmodf(m_value, right);
    fprintf(stderr, "Normalized value: %f | %f %f\n", p, m_value, m_percentage ? 1.0f : length);
    if (p < 0.0f)
    {
        p += right;
    }
    return m_percentage ? p * length : p;
}
bool Dash::percentage() const { return m_percentage; }

void PathDasher::invalidateSourcePath()
{
    m_contours.clear();
    invalidateDash();
}
void PathDasher::invalidateDash() { m_renderPath = nullptr; }

RenderPath* PathDasher::dash(const RawPath& source,
                             Factory* factory,
                             Dash offset,
                             Span<Dash> dashes)
{
    if (m_renderPath != nullptr)
    {
        // Previous result hasn't been invalidated, it's still good.
        return m_renderPath;
    }

    m_rawPath.rewind();
    if (m_contours.empty())
    {
        ContourMeasureIter iter(&source);
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
            if (dash.normalizedValue(contour->length()) > 0.0f)
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
        for (const rcp<ContourMeasure>& contour : m_contours)
        {
            float dashed = 0.0f;
            float distance = offset.normalizedValue(contour->length());
            bool draw = true;
            while (dashed < contour->length())
            {
                const Dash& dash = dashes[dashIndex++ % dashes.size()];
                float dashLength = dash.normalizedValue(contour->length());
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
                        contour->getSegment(distance, contour->length(), &m_rawPath, true);
                        contour->getSegment(0.0f, endLength, &m_rawPath, !contour->isClosed());
                    }

                    // Setup next step.
                    distance = endLength - dashLength;
                }
                else if (draw)
                {
                    contour->getSegment(distance, endLength, &m_rawPath, true);
                }
                distance += dashLength;
                dashed += dashLength;
                draw = !draw;
            }
        }
    }

    if (!m_dashedPath)
    {
        m_dashedPath = factory->makeEmptyRenderPath();
    }
    else
    {
        m_dashedPath->rewind();
    }

    m_renderPath = m_dashedPath.get();
    m_rawPath.addTo(m_renderPath);

    return m_renderPath;
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