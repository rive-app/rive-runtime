#include "rive/artboard.hpp"
#include "rive/shapes/paint/stroke.hpp"
#include "rive/shapes/paint/stroke_cap.hpp"
#include "rive/shapes/paint/stroke_effect.hpp"
#include "rive/shapes/paint/stroke_join.hpp"

using namespace rive;

PathSpace Stroke::pathSpace() const
{
    return transformAffectsStroke() ? PathSpace::Local : PathSpace::World;
}

RenderPaint* Stroke::initRenderPaint(ShapePaintMutator* mutator)
{
    auto renderPaint = Super::initRenderPaint(mutator);
    renderPaint->style(RenderPaintStyle::stroke);
    renderPaint->thickness(thickness());
    renderPaint->cap((StrokeCap)cap());
    renderPaint->join((StrokeJoin)join());
    return renderPaint;
}

void Stroke::applyTo(RenderPaint* renderPaint, float opacityModifier) const
{
    renderPaint->style(RenderPaintStyle::stroke);
    renderPaint->thickness(thickness());
    renderPaint->cap((StrokeCap)cap());
    renderPaint->join((StrokeJoin)join());
    renderPaint->shader(nullptr);
    m_PaintMutator->applyTo(renderPaint, opacityModifier);
}

bool Stroke::isVisible() const { return Super::isVisible() && thickness() > 0.0f; }

void Stroke::draw(Renderer* renderer, CommandPath* path, RenderPaint* paint)
{
    if (!isVisible())
    {
        return;
    }

    if (m_Effect != nullptr)
    {
        /// We're guaranteed to get a metrics path here if we have an effect.
        auto factory = artboard()->factory();
        path = m_Effect->effectPath(reinterpret_cast<MetricsPath*>(path), factory);
    }

    renderer->drawPath(path->renderPath(), paint);
}

void Stroke::thicknessChanged()
{
    assert(m_RenderPaint != nullptr);
    m_RenderPaint->thickness(thickness());
}

void Stroke::capChanged()
{
    assert(m_RenderPaint != nullptr);
    m_RenderPaint->cap((StrokeCap)cap());
}

void Stroke::joinChanged()
{
    assert(m_RenderPaint != nullptr);
    m_RenderPaint->join((StrokeJoin)join());
}

void Stroke::addStrokeEffect(StrokeEffect* effect) { m_Effect = effect; }

void Stroke::invalidateEffects()
{
    if (m_Effect != nullptr)
    {
        m_Effect->invalidateEffect();
    }
    invalidateRendering();
}

void Stroke::invalidateRendering()
{
    assert(m_RenderPaint != nullptr);
    m_RenderPaint->invalidateStroke();
}
