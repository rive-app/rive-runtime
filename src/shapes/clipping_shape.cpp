#include "rive/shapes/clipping_shape.hpp"
#include "rive/artboard.hpp"
#include "rive/core_context.hpp"
#include "rive/node.hpp"
#include "rive/renderer.hpp"
#include "rive/shapes/path_composer.hpp"
#include "rive/shapes/shape.hpp"

using namespace rive;

StatusCode ClippingShape::onAddedClean(CoreContext* context)
{
	auto clippingHolder = parent();

	auto artboard = static_cast<Artboard*>(context);
	for (auto core : artboard->objects())
	{
		if (core == nullptr)
		{
			continue;
		}
		// Iterate artboard to find drawables that are parented to this clipping
		// shape, they need to know they'll be clipped by this shape.
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

		// Iterate artboard to find shapes that are parented to the source,
		// their paths will need to be RenderPaths in order to be used for
		// clipping operations.
		if (core->is<Shape>() && core != clippingHolder)
		{
			auto component = core->as<ContainerComponent>();
			while (component != nullptr)
			{
				if (component == m_Source)
				{
					auto shape = core->as<Shape>();
					shape->addDefaultPathSpace(PathSpace::World |
					                           PathSpace::Clipping);
					m_Shapes.push_back(shape);
					break;
				}
				component = component->parent();
			}
		}
	}

	m_RenderPath = rive::makeRenderPath();

	return StatusCode::Ok;
}

StatusCode ClippingShape::onAddedDirty(CoreContext* context)
{
	StatusCode code = Super::onAddedDirty(context);
	if (code != StatusCode::Ok)
	{
		return code;
	}
	auto coreObject = context->resolve(sourceId());
	if (coreObject == nullptr || !coreObject->is<Node>())
	{
		return StatusCode::MissingObject;
	}

	m_Source = reinterpret_cast<Node*>(coreObject);

	return StatusCode::Ok;
}

void ClippingShape::buildDependencies()
{
	for (auto shape : m_Shapes)
	{
		shape->pathComposer()->addDependent(this);
	}
}

static Mat2D identity;
void ClippingShape::update(ComponentDirt value)
{
	if (hasDirt(value, ComponentDirt::Path | ComponentDirt::WorldTransform))
	{
		m_RenderPath->reset();

		m_RenderPath->fillRule((FillRule)fillRule());
		for (auto shape : m_Shapes)
		{
			if (!shape->isHidden())
			{
				m_RenderPath->addPath(shape->pathComposer()->worldPath(),
				                      identity);
			}
		}
	}
}

ClippingShape::~ClippingShape() { delete m_RenderPath; }