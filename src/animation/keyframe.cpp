#include "animation/keyframe.hpp"
#include "animation/cubic_interpolator.hpp"
#include "animation/keyed_property.hpp"
#include "core_context.hpp"
#include "importers/import_stack.hpp"
#include "importers/keyed_property_importer.hpp"

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