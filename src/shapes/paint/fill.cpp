#include "rive/shapes/paint/fill.hpp"

using namespace rive;

PathSpace Fill::pathSpace() const { return PathSpace::Local; }

RenderPaint* Fill::initRenderPaint(ShapePaintMutator* mutator)
{
	auto renderPaint = Super::initRenderPaint(mutator);
	renderPaint->style(RenderPaintStyle::fill);
	return renderPaint;
}

void Fill::draw(Renderer* renderer, CommandPath* path)
{
	if (!isVisible())
	{
		return;
	}
	auto renderPath = path->renderPath();
	renderPath->fillRule((FillRule)fillRule());
	renderer->drawPath(renderPath, m_RenderPaint);
}