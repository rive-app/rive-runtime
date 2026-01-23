#include "rive/component.hpp"
#include "rive/file.hpp"
#include "rive/artboard_list_map_rule.hpp"
#include "rive/artboard_component_list.hpp"

using namespace rive;

StatusCode ArtboardListMapRule::onAddedDirty(CoreContext* context)
{
    StatusCode code = Super::onAddedDirty(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }
    if (!parent()->is<ArtboardComponentList>())
    {
        return StatusCode::MissingObject;
    }
    parent()->as<ArtboardComponentList>()->addMapRule(this);
    return StatusCode::Ok;
}