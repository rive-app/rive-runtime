#include "rive/shapes/paint/fill.hpp"

using namespace rive;

PathSpace Fill::pathSpace() const { return PathSpace::Local; }

RenderPaint* Fill::initRenderPaint(ShapePaintMutator* mutator)
{
    auto renderPaint = Super::initRenderPaint(mutator);
    renderPaint->style(RenderPaintStyle::fill);
    return renderPaint;
}

void Fill::applyTo(RenderPaint* renderPaint, float opacityModifier) const
{
    renderPaint->style(RenderPaintStyle::fill);
    renderPaint->shader(nullptr);
    m_PaintMutator->applyTo(renderPaint, opacityModifier);
}

void Fill::draw(Renderer* renderer, CommandPath* path, RenderPaint* paint)
{
    if (!isVisible())
    {
        return;
    }
    auto renderPath = path->renderPath();
    renderPath->fillRule((FillRule)fillRule());
    renderer->drawPath(renderPath, paint);
}