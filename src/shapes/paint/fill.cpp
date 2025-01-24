#include "rive/shapes/paint/fill.hpp"
#include "rive/shapes/shape_paint_container.hpp"

using namespace rive;

PathFlags Fill::pathFlags() const
{
    return (FillRule)fillRule() == FillRule::clockwise
               ? PathFlags::localClockwise
               : PathFlags::local;
}

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

ShapePaintPath* Fill::pickPath(ShapePaintContainer* shape) const
{
    if ((FillRule)fillRule() == FillRule::clockwise)
    {
        return shape->localClockwisePath();
    }
    return shape->localPath();
}