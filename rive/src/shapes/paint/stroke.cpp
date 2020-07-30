#include "shapes/paint/stroke.hpp"

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
	return renderPaint;
}

void Stroke::draw(Renderer* renderer, RenderPath* path)
{
	renderer->drawPath(path, m_RenderPaint);
}