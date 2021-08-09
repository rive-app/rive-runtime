#include "rive/animation/keyframe.hpp"
#include "rive/animation/cubic_interpolator.hpp"
#include "rive/animation/keyed_property.hpp"
#include "rive/core_context.hpp"
#include "rive/importers/import_stack.hpp"
#include "rive/importers/keyed_property_importer.hpp"

using namespace rive;

StatusCode KeyFrame::onAddedDirty(CoreContext* context)
{
	if (interpolatorId() > 0)
	{
		auto coreObject = context->resolve(interpolatorId());
		if (coreObject == nullptr || !coreObject->is<CubicInterpolator>())
		{
			return StatusCode::MissingObject;
		}
		m_Interpolator = coreObject->as<CubicInterpolator>();
	}

	return StatusCode::Ok;
}

void KeyFrame::computeSeconds(int fps) { m_Seconds = frame() / (float)fps; }

StatusCode KeyFrame::import(ImportStack& importStack)
{
	auto importer =
	    importStack.latest<KeyedPropertyImporter>(KeyedProperty::typeKey);
	if (importer == nullptr)
	{
		return StatusCode::MissingObject;
	}
	importer->addKeyFrame(this);
	return Super::import(importStack);
}