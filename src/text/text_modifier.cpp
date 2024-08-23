#include "rive/text/text.hpp"
#include "rive/text/text_modifier.hpp"
#include "rive/text/text_modifier_group.hpp"

using namespace rive;

StatusCode TextModifier::onAddedDirty(CoreContext* context)
{
    StatusCode code = Super::onAddedDirty(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }

    if (parent() != nullptr && parent()->is<TextModifierGroup>())
    {
        parent()->as<TextModifierGroup>()->addModifier(this);
        return StatusCode::Ok;
    }

    return StatusCode::MissingObject;
}