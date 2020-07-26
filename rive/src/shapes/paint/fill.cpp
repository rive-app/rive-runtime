#include "shapes/paint/fill.hpp"

using namespace rive;

PathSpace Fill::pathSpace() const { return PathSpace::Local; }

RenderPaint* Fill::initPaintMutator(ShapePaintMutator* mutator) 
{
	auto renderPaint = Super::initPaintMutator(mutator);
	renderPaint->style(RenderPaintStyle::Fill);
	return renderPaint;
}

void Fill::draw(Renderer* renderer, RenderPath* path)
{
	renderer->drawPath(path, m_RenderPaint);
}