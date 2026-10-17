#include "rive/draw_target.hpp"
#include "rive/core_context.hpp"
#include "rive/drawable.hpp"
#include "rive/artboard.hpp"

using namespace rive;

bool DrawTarget::validate(CoreContext* context)
{
  if (!Super::validate(context))
  {
    return false;
  }
  // Artboard::initialize and sortDrawOrder dereference drawable()
  // unconditionally, so a target without one (missing from the file, or
  // culled as invalid itself) must not make it into the artboard.
  auto coreObject = context->resolve(drawableId());
  return coreObject != nullptr && coreObject->is<Drawable>();
}

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
#ifdef WITH_RIVE_EDITOR
  setDrawableForEditor(static_cast<Drawable*>(coreObject));
#else
  m_Drawable = static_cast<Drawable*>(coreObject);
#endif
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
