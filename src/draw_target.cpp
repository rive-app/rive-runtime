#include "draw_target.hpp"
#include "core_context.hpp"
#include "drawable.hpp"
#include "artboard.hpp"

using namespace rive;

StatusCode DrawTarget::onAddedDirty(CoreContext* context)
{
	StatusCode code = Super::onAddedDirty(context);
	if (code != StatusCode::Ok)
	{
		return code;
	}
	auto coreObject = context->resolve(drawableId());
	if (coreObject == nullptr || !coreObject->is<Drawable>())
	{
		return StatusCode::MissingObject;
	}
	m_Drawable = reinterpret_cast<Drawable*>(coreObject);
	return StatusCode::Ok;
}

StatusCode DrawTarget::onAddedClean(CoreContext* context)
{
	return StatusCode::Ok;
}

void DrawTarget::placementValueChanged()
{
	artboard()->addDirt(ComponentDirt::DrawOrder);
}