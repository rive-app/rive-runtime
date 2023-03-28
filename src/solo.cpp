#include "rive/solo.hpp"
#include "rive/artboard.hpp"

using namespace rive;

void Solo::propagateCollapse()
{
    Core* active = artboard()->resolve(activeComponentId());
    for (Component* child : children())
    {
        child->collapse(child != active);
    }
}
void Solo::activeComponentIdChanged() { propagateCollapse(); }

StatusCode Solo::onAddedClean(CoreContext* context)
{
    StatusCode code = Super::onAddedClean(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }

    propagateCollapse();
    return StatusCode::Ok;
}