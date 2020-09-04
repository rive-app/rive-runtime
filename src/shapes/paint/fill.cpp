#include "shapes/paint/fill.hpp"

using namespace rive;

PathSpace Fill::pathSpace() const { return PathSpace::Local; }

RenderPaint* Fill::initPaintMutator(ShapePaintMutator* mutator)
{
	auto renderPaint = Super::initPaintMutator(mutator);
	renderPaint->style(RenderPaintStyle::fill);
	return renderPaint;
}

void Fill::draw(Renderer* renderer, CommandPath* path)
{
	if (!isVisible())
	{
		return;
	}
	renderer->drawPath(path->renderPath(), m_RenderPaint);
}