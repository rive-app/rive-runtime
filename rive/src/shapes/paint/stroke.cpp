#include "shapes/paint/stroke.hpp"
#include "shapes/paint/stroke_cap.hpp"
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