#include "shapes/paint/shape_paint.hpp"
#include "shapes/shape_paint_container.hpp"

#include "renderer.hpp"

using namespace rive;

void ShapePaint::onAddedDirty(CoreContext* context) 
{
    Super::onAddedDirty(context);
    m_RenderPaint = makeRenderPaint();
}

void ShapePaint::onAddedClean(CoreContext* context) 
{
    // Init the mutator.
    auto container = ShapePaintContainer::from(parent());
    assert(container != nullptr);
    container->addPaint(this);
}

RenderPaint* ShapePaint::initPaintMutator(ShapePaintMutator* mutator)
{
    m_PaintMutator = mutator;
    return m_RenderPaint;
}