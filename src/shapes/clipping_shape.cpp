#include "shapes/clipping_shape.hpp"
#include "artboard.hpp"
#include "core_context.hpp"
#include "shapes/shape.hpp"

using namespace rive;

StatusCode ClippingShape::onAddedClean(CoreContext* context)
{
	auto clippingHolder = parent();

	auto artboard = static_cast<Artboard*>(context);
	for (auto core : artboard->objects())
	{
		if (core->is<Drawable>())
		{
			auto drawable = core->as<Drawable>();
			for (ContainerComponent* component = drawable; component != nullptr;
			     component = component->parent())
			{
				if (component == clippingHolder)
				{
					drawable->addClippingShape(this);
					break;
				}
			}
		}
	}
	return StatusCode::Ok;
}

StatusCode ClippingShape::onAddedDirty(CoreContext* context)
{
	StatusCode code = Super::onAddedDirty(context);
	if (code != StatusCode::Ok)
	{
		return code;
	}
	auto coreObject = context->resolve(shapeId());
	if (coreObject == nullptr || !coreObject->is<Shape>())
	{
		return StatusCode::MissingObject;
	}

	m_Shape = reinterpret_cast<Shape*>(coreObject);
	m_Shape->addDefaultPathSpace(
	    (ClipOp)clipOpValue() == ClipOp::intersection
	        ? PathSpace::World | PathSpace::Clipping
	        : PathSpace::World | PathSpace::Difference | PathSpace::Clipping);

	return StatusCode::Ok;
}