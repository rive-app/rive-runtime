#include "rive/draw_rules.hpp"
#include "rive/artboard.hpp"
#include "rive/core_context.hpp"
#include "rive/draw_target.hpp"

using namespace rive;

StatusCode DrawRules::onAddedDirty(CoreContext* context)
{
    StatusCode result = Super::onAddedDirty(context);
    if (result != StatusCode::Ok)
    {
        return result;
    }
    auto coreObject = context->resolve(drawTargetId());
    if (coreObject != nullptr && coreObject->is<DrawTarget>())
    {
#ifdef WITH_RIVE_EDITOR
        setActiveTargetForEditor(static_cast<DrawTarget*>(coreObject));
#else
        m_ActiveTarget = static_cast<DrawTarget*>(coreObject);
#endif
    }

    return StatusCode::Ok;
}

StatusCode DrawRules::onAddedClean(CoreContext* context)
{
    return StatusCode::Ok;
}

void DrawRules::drawTargetIdChanged()
{
    auto coreObject = artboard()->resolve(drawTargetId());
    DrawTarget* t = (coreObject != nullptr && coreObject->is<DrawTarget>())
                        ? static_cast<DrawTarget*>(coreObject)
                        : nullptr;
#ifdef WITH_RIVE_EDITOR
    setActiveTargetForEditor(t);
#else
    m_ActiveTarget = t;
#endif
    artboard()->addDirt(ComponentDirt::DrawOrder);
}
