#include "rive/core_context.hpp"
#include "rive/text/text.hpp"
#include "rive/text/text_modifier_group.hpp"
#include "rive/text/text_target_modifier.hpp"
#include "rive/transform_component.hpp"

using namespace rive;

StatusCode TextTargetModifier::onAddedDirty(CoreContext* context)
{
    StatusCode code = Super::onAddedDirty(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }

    auto coreObject = context->resolve(targetId());
    m_Target = static_cast<TransformComponent*>(coreObject);

    return StatusCode::Ok;
}

Text* TextTargetModifier::textComponent() const
{
    if (parent() != nullptr && parent()->is<TextModifierGroup>())
    {
        return parent()->as<TextModifierGroup>()->textComponent();
    }
    return nullptr;
}
