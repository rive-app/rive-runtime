#include "shapes/clipping_shape.hpp"
#include "artboard.hpp"
#include "core_context.hpp"
#include "shapes/shape.hpp"

using namespace rive;

void ClippingShape::onAddedClean(CoreContext* context)
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
					return;
				}
			}
		}
	}
}

void ClippingShape::onAddedDirty(CoreContext* context)
{
	Super::onAddedDirty(context);
	auto coreObject = context->resolve(shapeId());
	if (coreObject == nullptr)
	{
		return;
	}
	if (coreObject->is<Shape>())
	{
		m_Shape = reinterpret_cast<Shape*>(coreObject);
        m_Shape->addDefaultPathSpace((ClipOp)clipOpValue() == ClipOp::intersection
	                         ? PathSpace::World
	                         : PathSpace::World | PathSpace::Difference);
	}
}