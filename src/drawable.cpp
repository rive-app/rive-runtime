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
		auto shape = clippingShape->shape();
		auto composer = shape->pathComposer();

		RenderPath* renderPath = nullptr;
		switch ((ClipOp)clippingShape->clipOpValue())
		{
			case ClipOp::intersection:
				renderPath = composer->worldPath()->renderPath();
				if (renderPath == nullptr)
				{
					printf("NO INTER\n");
				}
				break;
			case ClipOp::difference:
				renderPath = composer->differencePath();
				if (renderPath == nullptr)
				{
					printf("NO DIFF\n");
				}
				break;
		}

		assert(renderPath != nullptr);
		renderer->clipPath(renderPath);
	}
	return true;
}