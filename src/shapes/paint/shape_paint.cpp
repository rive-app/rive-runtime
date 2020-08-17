#include "shapes/paint/shape_paint.hpp"
#include "shapes/shape_paint_container.hpp"

#include "renderer.hpp"

using namespace rive;

void ShapePaint::onAddedClean(CoreContext* context)
{
	auto container = ShapePaintContainer::from(parent());
	assert(container != nullptr);
	container->addPaint(this);
}

RenderPaint* ShapePaint::initPaintMutator(ShapePaintMutator* mutator)
{
	assert(m_RenderPaint == nullptr);
	m_PaintMutator = mutator;
	return m_RenderPaint = makeRenderPaint();
}

void ShapePaint::blendMode(BlendMode value)
{
    assert(m_RenderPaint != nullptr);
	m_RenderPaint->blendMode(value);
}