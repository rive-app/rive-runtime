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
        m_ActiveTarget = static_cast<DrawTarget*>(coreObject);
    }

    return StatusCode::Ok;
}

StatusCode DrawRules::onAddedClean(CoreContext* context) { return StatusCode::Ok; }

void DrawRules::drawTargetIdChanged()
{
    auto coreObject = artboard()->resolve(drawTargetId());
    if (coreObject == nullptr || !coreObject->is<DrawTarget>())
    {
        m_ActiveTarget = nullptr;
    }
    else
    {
        m_ActiveTarget = static_cast<DrawTarget*>(coreObject);
    }
    artboard()->addDirt(ComponentDirt::DrawOrder);
}
