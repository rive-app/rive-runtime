#include "shapes/paint/stroke.hpp"
#include "shapes/paint/stroke_cap.hpp"
#include "shapes/paint/stroke_effect.hpp"
#include "shapes/paint/stroke_join.hpp"

using namespace rive;

PathSpace Stroke::pathSpace() const
{
	return transformAffectsStroke() ? PathSpace::Local : PathSpace::World;
}

RenderPaint* Stroke::initPaintMutator(ShapePaintMutator* mutator)
{
	auto renderPaint = Super::initPaintMutator(mutator);
	renderPaint->style(RenderPaintStyle::stroke);
	renderPaint->thickness(thickness());
	renderPaint->cap((StrokeCap)cap());
	renderPaint->join((StrokeJoin)join());
	return renderPaint;
}

void Stroke::draw(Renderer* renderer, RenderPath* path)
{
	if (!isVisible())
	{
		return;
	}

	if (m_Effect != nullptr)
	{
		/// We're guaranteed to get a metrics path here if we have an effect.
		path = m_Effect->effectPath(reinterpret_cast<MetricsPath*>(path));
	}

	renderer->drawPath(path, m_RenderPaint);
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
}