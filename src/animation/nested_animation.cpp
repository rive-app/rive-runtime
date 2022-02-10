#include "rive/nested_animation.hpp"
#include "rive/container_component.hpp"
#include "rive/nested_artboard.hpp"

using namespace rive;

StatusCode NestedAnimation::onAddedDirty(CoreContext* context)
{
    StatusCode code = Super::onAddedDirty(context);
    if (code == StatusCode::Ok)
    {
        if (!parent()->is<NestedArtboard>())
        {
            return StatusCode::InvalidObject;
        }
        auto nestedArtboard = parent()->as<NestedArtboard>();
        nestedArtboard->addNestedAnimation(this);
    }
    return code;
}