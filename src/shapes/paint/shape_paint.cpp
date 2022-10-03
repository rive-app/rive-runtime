#include "rive/shapes/paint/shape_paint.hpp"
#include "rive/shapes/shape_paint_container.hpp"

#include "rive/artboard.hpp"
#include "rive/factory.hpp"

using namespace rive;

StatusCode ShapePaint::onAddedClean(CoreContext* context)
{
    auto container = ShapePaintContainer::from(parent());
    if (container == nullptr)
    {
        return StatusCode::MissingObject;
    }
    container->addPaint(this);
    return StatusCode::Ok;
}

RenderPaint* ShapePaint::initRenderPaint(ShapePaintMutator* mutator)
{
    assert(m_RenderPaint == nullptr);
    m_PaintMutator = mutator;

    auto factory = mutator->component()->artboard()->factory();
    m_RenderPaint = factory->makeRenderPaint();
    return m_RenderPaint.get();
}

void ShapePaint::blendMode(BlendMode value)
{
    assert(m_RenderPaint != nullptr);
    m_RenderPaint->blendMode(value);
}