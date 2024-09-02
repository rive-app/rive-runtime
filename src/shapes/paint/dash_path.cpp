#include "rive/shapes/paint/dash_path.hpp"
#include "rive/factory.hpp"

using namespace rive;

Dash::Dash() : m_value(0.0f), m_percentage(false) {}
Dash::Dash(float value, bool percentage) : m_value(value), m_percentage(percentage) {}
float Dash::value() const { return m_value; }
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

    int dashIndex = 0;
    for (const rcp<ContourMeasure>& contour : m_contours)
    {
        float distance = offset.percentage() ? offset.value() * contour->length() : offset.value();
        bool draw = true;
        while (distance < contour->length())
        {
            const Dash& dash = dashes[dashIndex];
            float dashLength = dash.percentage() ? dash.value() * contour->length() : dash.value();
            if (draw)
            {
                contour->getSegment(distance, distance + dashLength, &m_rawPath, true);
            }
            distance += dashLength;
            draw = !draw;
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