#include "drawable.hpp"
#include "artboard.hpp"
#include "shapes/clipping_shape.hpp"
#include "shapes/path_composer.hpp"
#include "shapes/shape.hpp"

using namespace rive;

void Drawable::addClippingShape(ClippingShape* shape)
{
	m_ClippingShapes.push_back(shape);
}

bool Drawable::clip(Renderer* renderer) const
{
	if (m_ClippingShapes.size() == 0)
	{
		return false;
	}

	renderer->save();

	for (auto clippingShape : m_ClippingShapes)
	{
		if (!clippingShape->isVisible())
		{
			continue;
		}

		RenderPath* renderPath = clippingShape->renderPath();

		assert(renderPath != nullptr);
		renderer->clipPath(renderPath);
	}
	return true;
}