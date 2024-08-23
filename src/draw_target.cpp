#include "rive/draw_target.hpp"
#include "rive/core_context.hpp"
#include "rive/drawable.hpp"
#include "rive/artboard.hpp"

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
    m_Drawable = static_cast<Drawable*>(coreObject);
    return StatusCode::Ok;
}

StatusCode DrawTarget::onAddedClean(CoreContext* context) { return StatusCode::Ok; }

void DrawTarget::placementValueChanged() { artboard()->addDirt(ComponentDirt::DrawOrder); }
