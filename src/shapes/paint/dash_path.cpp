#include "rive/shapes/paint/dash_path.hpp"
#include "rive/shapes/paint/dash.hpp"
#include "rive/shapes/paint/stroke.hpp"
#include "rive/math/path_measure.hpp"
#include "rive/factory.hpp"

using namespace rive;

void PathDasher::invalidateSourcePath() { invalidateDash(); }

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
    m_pathMeasure = PathMeasure(source);

    // Make sure dashes have some length.
    bool hasValidDash = false;
    for (auto dash : dashes)
    {
        if (dash->normalizedLength(m_pathMeasure.length(), false) > 0.0f)
        {
            hasValidDash = true;
            break;
        }
    }
    if (hasValidDash)
    {
        int dashIndex = 0;
        auto rawPath = m_path.mutableRawPath();
        float dashed = 0.0f;
        float distance = offset->normalizedLength(m_pathMeasure.length(), true);
        bool draw = true;
        while (dashed < m_pathMeasure.length())
        {
            const Dash* dash = dashes[dashIndex++ % dashes.size()];
            float dashLength =
                dash->normalizedLength(m_pathMeasure.length(), false);
            if (dashLength > m_pathMeasure.length())
            {
                dashLength = m_pathMeasure.length();
            }
            float endLength = distance + dashLength;
            if (endLength > m_pathMeasure.length())
            {
                endLength -= m_pathMeasure.length();
                if (draw)
                {
                    if (distance < m_pathMeasure.length())
                    {
                        m_pathMeasure.getSegment(distance,
                                                 m_pathMeasure.length(),
                                                 rawPath,
                                                 true);
                        m_pathMeasure.getSegment(0.0f,
                                                 endLength,
                                                 rawPath,
                                                 !m_pathMeasure.isClosed());
                    }
                    else
                    {
                        m_pathMeasure.getSegment(0.0f,
                                                 endLength,
                                                 rawPath,
                                                 true);
                    }
                }

                // Setup next step.
                distance = endLength - dashLength;
            }
            else if (draw)
            {
                m_pathMeasure.getSegment(distance, endLength, rawPath, true);
            }
            distance += dashLength;
            dashed += dashLength;
            draw = !draw;
        }
    }
    return &m_path;
}

float PathDasher::pathLength() const { return m_pathMeasure.length(); }

StatusCode DashPath::onAddedClean(CoreContext* context)
{
    if (!parent()->is<ShapePaint>())
    {
        return StatusCode::InvalidObject;
    }
    parent()->as<ShapePaint>()->addStrokeEffect(this);

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

void DashPath::updateEffect(const ShapePaintPath* source,
                            ShapePaintType shapePaintType)
{

    if (m_path.hasRenderPath())
    {
        return;
    }
    m_path.rewind(source->isLocal());
    // Dash is not supported on fills so it will use the source as output
    if (shapePaintType == ShapePaintType::fill)
    {
        m_path.addPath(source);
    }
    else
    {
        Dash dashOffset(offset(), offsetIsPercentage());
        applyDash(source->rawPath(), &dashOffset, m_dashes);
    }
}

ShapePaintPath* DashPath::effectPath() { return &m_path; }

void DashPath::invalidateDash()
{
    PathDasher::invalidateDash();
    if (parent() != nullptr)
    {
        auto stroke = parent()->as<ShapePaint>();
        stroke->parent()->addDirt(ComponentDirt::Paint);
        stroke->invalidateEffects(this);
    }
}