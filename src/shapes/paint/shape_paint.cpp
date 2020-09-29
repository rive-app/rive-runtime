#include "shapes/paint/shape_paint.hpp"
#include "shapes/shape_paint_container.hpp"

#include "renderer.hpp"

using namespace rive;

ShapePaint::~ShapePaint() { delete m_RenderPaint; }

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