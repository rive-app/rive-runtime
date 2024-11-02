#include "rive/nested_animation.hpp"
#include "rive/container_component.hpp"
#include "rive/nested_artboard.hpp"
#include "rive/core_context.hpp"

using namespace rive;

bool NestedAnimation::validate(CoreContext* context)
{
    if (!Super::validate(context))
    {
        return false;
    }

    auto parentObject = context->resolve(parentId());
    // We know parentObject is not null from Super::validate().
    return parentObject->is<NestedArtboard>();
}

StatusCode NestedAnimation::onAddedDirty(CoreContext* context)
{
    StatusCode code = Super::onAddedDirty(context);
    if (code == StatusCode::Ok)
    {
        auto nestedArtboard = parent()->as<NestedArtboard>();
        nestedArtboard->addNestedAnimation(this);
    }
    return code;
}