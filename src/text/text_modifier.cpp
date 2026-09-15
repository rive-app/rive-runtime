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
#ifndef WITH_RIVE_EDITOR
        // Runtime-only; editor build registers via editorParentChanged.
        parent()->as<TextModifierGroup>()->addModifier(this);
#endif
        return StatusCode::Ok;
    }

    return StatusCode::MissingObject;
}