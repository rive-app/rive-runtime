#include "rive/shapes/paint/dash_path.hpp"
#include "rive/shapes/paint/dash.hpp"
#include "rive/shapes/paint/stroke.hpp"
#include "rive/math/path_measure.hpp"
#include "rive/factory.hpp"

using namespace rive;

void DashEffectPath::invalidateEffect() { m_path.rewind(); }

void DashEffectPath::createPathMeasure(const RawPath* source)
{
    m_pathMeasure = PathMeasure(source);
}

void PathDasher::invalidateDash() {}

ShapePaintPath* PathDasher::dash(ShapePaintPath* destination,
                                 const RawPath* source,
                                 PathMeasure* pathMeasure,
                                 Dash* offset,
                                 Span<Dash*> dashes)
{
    if (destination->hasRenderPath())
    {
        // Previous result hasn't been invalidated, it's still good.
        return destination;
    }

    destination->rewind();
    return applyDash(destination, source, pathMeasure, offset, dashes);
}
ShapePaintPath* PathDasher::applyDash(ShapePaintPath* destination,
                                      const RawPath* source,
                                      PathMeasure* pathMeasure,
                                      Dash* offset,
                                      Span<Dash*> dashes)
{
    // Make sure dashes have some length.
    bool hasValidDash = false;
    for (auto dash : dashes)
    {
        if (dash->normalizedLength(pathMeasure->length(), false) > 0.0f)
        {
            hasValidDash = true;
            break;
        }
    }
    if (hasValidDash)
    {
        int dashIndex = 0;
        auto rawPath = destination->mutableRawPath();
        float dashed = 0.0f;
        float distance = offset->normalizedLength(pathMeasure->length(), true);
        bool draw = true;
        while (dashed < pathMeasure->length())
        {
            const Dash* dash = dashes[dashIndex++ % dashes.size()];
            float dashLength =
                dash->normalizedLength(pathMeasure->length(), false);
            if (dashLength > pathMeasure->length())
            {
                dashLength = pathMeasure->length();
            }
            float endLength = distance + dashLength;
            if (endLength > pathMeasure->length())
            {
                endLength -= pathMeasure->length();
                if (draw)
                {
                    if (distance < pathMeasure->length())
                    {
                        pathMeasure->getSegment(distance,
                                                pathMeasure->length(),
                                                rawPath,
                                                true);
                        pathMeasure->getSegment(0.0f,
                                                endLength,
                                                rawPath,
                                                !pathMeasure->isClosed());
                    }
                    else
                    {
                        pathMeasure->getSegment(0.0f, endLength, rawPath, true);
                    }
                }

                // Setup next step.
                distance = endLength - dashLength;
            }
            else if (draw)
            {
                pathMeasure->getSegment(distance, endLength, rawPath, true);
            }
            distance += dashLength;
            dashed += dashLength;
            draw = !draw;
        }
    }
    return destination;
}

StatusCode DashPath::onAddedClean(CoreContext* context)
{
    auto effectsContainer = EffectsContainer::from(parent());
    if (!effectsContainer)
    {
        return StatusCode::InvalidObject;
    }
    effectsContainer->addStrokeEffect(this);

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

void DashPath::offsetChanged() { invalidateEffectFromLocal(); }
void DashPath::offsetIsPercentageChanged() { invalidateEffectFromLocal(); }

void DashPath::updateEffect(PathProvider* pathProvider,
                            const ShapePaintPath* source,
                            const ShapePaint* shapePaint)
{
    auto effectPathIt = m_effectPaths.find(pathProvider);
    if (effectPathIt != m_effectPaths.end())
    {
        auto dashEffectPath =
            static_cast<DashEffectPath*>(effectPathIt->second);
        auto path = dashEffectPath->path();
        if (path->hasRenderPath())
        {
            return;
        }
        path->rewind(source->isLocal());
        // Dash is not supported on fills so it will use the source as output
        if (shapePaint->paintType() == ShapePaintType::fill)
        {
            path->addPath(source);
        }
        else
        {
            Dash dashOffset(offset(), offsetIsPercentage());
            dashEffectPath->createPathMeasure(source->rawPath());
            applyDash(path,
                      source->rawPath(),
                      &dashEffectPath->pathMeasure(),
                      &dashOffset,
                      m_dashes);
        }
    }
}

void DashPath::invalidateDash() { invalidateEffectFromLocal(); }

EffectsContainer* DashPath::parentPaint()
{
    return EffectsContainer::from(parent());
}

EffectPath* DashPath::createEffectPath() { return new DashEffectPath(); }